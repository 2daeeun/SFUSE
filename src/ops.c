// ops.c: FUSE 콜백 구현
// ops.c: FUSE 콜백 구현
#include "ops.h"
#include "bitmap.h"
#include "block.h"
#include "dir.h"
#include "fs.h"
#include "inode.h"
#include "super.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

/* FUSE 초기화 콜백 (FUSE3 API) */
static void *sfuse_init_cb(struct fuse_conn_info *conn,
                           struct fuse_config *cfg) {
  (void)conn;
  (void)cfg;
  struct sfuse_fs *fs = get_fs_context();
  // 파일시스템 초기화 (슈퍼블록/비트맵 로드, 루트 아이노드 설정)
  if (fs_initialize(fs->backing_fd) < 0) {
    fprintf(stderr, "[SFUSE] FS initialization failed\n");
    exit(EXIT_FAILURE);
  }
  return fs;
}

/* FUSE 종료 콜백 */
static void sfuse_destroy_cb(void *private_data) {
  struct sfuse_fs *fs = private_data;
  size_t bmap_bytes = fs->sb.blocks_count / 8;
  size_t imap_bytes = fs->sb.inodes_count / 8;
  bitmap_sync(fs->backing_fd, fs->sb.block_bitmap_start, fs->block_map,
              bmap_bytes);
  bitmap_sync(fs->backing_fd, fs->sb.inode_bitmap_start, fs->inode_map,
              imap_bytes);
  sb_sync(fs->backing_fd, &fs->sb);
  free(fs->block_map);
  free(fs->inode_map);
}

/* getattr */
static int sfuse_getattr_cb(const char *path, struct stat *stbuf,
                            struct fuse_file_info *fi) {
  (void)fi;
  struct sfuse_fs *fs = get_fs_context();
  uint32_t ino;
  if (fs_resolve_path(fs, path, &ino) < 0)
    return -ENOENT;
  struct sfuse_inode inode;
  if (inode_load(fs->backing_fd, &fs->sb, ino, &inode) < 0)
    return -EIO;
  memset(stbuf, 0, sizeof(*stbuf));
  stbuf->st_mode = inode.mode;
  stbuf->st_nlink = S_ISDIR(inode.mode) ? 2 : 1; // 디렉터리는 링크수 2로 고정
  stbuf->st_uid = inode.uid;
  stbuf->st_gid = inode.gid;
  stbuf->st_size = inode.size;
  stbuf->st_atime = inode.atime;
  stbuf->st_mtime = inode.mtime;
  stbuf->st_ctime = inode.ctime;
  return 0;
}

/* access */
static int sfuse_access_cb(const char *path, int mask) {
  struct sfuse_fs *fs = get_fs_context();
  uint32_t ino;
  if (fs_resolve_path(fs, path, &ino) < 0)
    return -ENOENT;
  (void)mask;
  return 0;
}

/* readdir */
static int sfuse_readdir_cb(const char *path, void *buf, fuse_fill_dir_t filler,
                            off_t offset, struct fuse_file_info *fi,
                            enum fuse_readdir_flags flags) {
  (void)offset;
  (void)fi;
  struct sfuse_fs *fs = get_fs_context();
  uint32_t ino;
  if (fs_resolve_path(fs, path, &ino) < 0)
    return -ENOENT;
  struct sfuse_inode dinode;
  if (inode_load(fs->backing_fd, &fs->sb, ino, &dinode) < 0)
    return -EIO;
  // 디렉터리 엔트리 목록 채우기
  // **중복된 "." 및 ".."는 추가하지 않습니다 (디렉터리 블록에 이미 존재)**
  filler(buf, ".", NULL, 0, 0); // FUSE 규약상 수동 추가 가능하지만,
  filler(buf, "..", NULL, 0,
         0); // 실제 엔트리도 있으므로 중복 주의 (필요시 제거)
  size_t entries_per_block = SFUSE_BLOCK_SIZE / sizeof(struct sfuse_dirent);
  size_t total_entries = dinode.size / sizeof(struct sfuse_dirent);
  for (int b = 0; b < SFUSE_NDIR_BLOCKS && dinode.direct[b]; b++) {
    uint8_t block[SFUSE_BLOCK_SIZE];
    if (read_block(fs->backing_fd, dinode.direct[b], block) < 0)
      return -EIO;
    struct sfuse_dirent *entries = (struct sfuse_dirent *)block;
    for (size_t i = 0; i < entries_per_block; i++) {
      size_t idx = b * entries_per_block + i;
      if (idx >= total_entries)
        break;
      if (entries[i].ino == 0)
        continue;
      if (strcmp(entries[i].name, ".") == 0 ||
          strcmp(entries[i].name, "..") == 0)
        continue; // 이미 처리한 항목은 건너뛰기
      filler(buf, entries[i].name, NULL, 0, 0);
    }
  }
  return 0;
}

/* open */
static int sfuse_open_cb(const char *path, struct fuse_file_info *fi) {
  struct sfuse_fs *fs = get_fs_context();
  uint32_t ino;

  if (fs_resolve_path(fs, path, &ino) < 0)
    return -ENOENT;

  struct sfuse_inode inode;
  if (inode_load(fs->backing_fd, &fs->sb, ino, &inode) < 0)
    return -EIO;

  // 디렉터리도 정상적으로 open 가능하게 처리
  return 0;
}

/* read */
static int sfuse_read_cb(const char *path, char *buf, size_t size, off_t offset,
                         struct fuse_file_info *fi) {
  (void)fi;
  struct sfuse_fs *fs = get_fs_context();
  uint32_t ino;
  if (fs_resolve_path(fs, path, &ino) < 0)
    return -ENOENT;
  struct sfuse_inode inode;
  inode_load(fs->backing_fd, &fs->sb, ino, &inode);
  if (S_ISDIR(inode.mode))
    return -EISDIR;
  if (offset >= inode.size)
    return 0;
  // 파일 크기를 넘어서는 부분은 읽지 않음
  size_t to_read = size;
  if (offset + to_read > inode.size)
    to_read = inode.size - offset;
  size_t done = 0;
  uint32_t pbn;
  uint8_t tmp[SFUSE_BLOCK_SIZE];
  // 오프셋부터 필요한 바이트만큼 읽기
  while (done < to_read) {
    off_t cur = offset + done;
    uint32_t lbn = cur / SFUSE_BLOCK_SIZE;
    size_t boff = cur % SFUSE_BLOCK_SIZE;
    size_t chunk = SFUSE_BLOCK_SIZE - boff;
    if (chunk > to_read - done)
      chunk = to_read - done;
    logical_to_physical(fs->backing_fd, &fs->sb, &inode, lbn, tmp, &pbn);
    read_block(fs->backing_fd, pbn, tmp);
    memcpy(buf + done, tmp + boff, chunk);
    done += chunk;
  }
  return done;
}

/* write */
static int sfuse_write_cb(const char *path, const char *buf, size_t size,
                          off_t offset, struct fuse_file_info *fi) {
  (void)fi;
  struct sfuse_fs *fs = get_fs_context();
  uint32_t ino;
  if (fs_resolve_path(fs, path, &ino) < 0)
    return -ENOENT;
  struct sfuse_inode inode;
  inode_load(fs->backing_fd, &fs->sb, ino, &inode);
  if (S_ISDIR(inode.mode))
    return -EISDIR;
  size_t written = 0;
  uint32_t pbn;
  uint8_t tmp[SFUSE_BLOCK_SIZE];
  // 데이터 쓰기: 필요한 블록을 할당하거나 찾아서 부분 갱신
  while (written < size) {
    off_t cur = offset + written;
    uint32_t lbn = cur / SFUSE_BLOCK_SIZE;
    size_t boff = cur % SFUSE_BLOCK_SIZE;
    size_t chunk = SFUSE_BLOCK_SIZE - boff;
    if (chunk > size - written)
      chunk = size - written;
    if (logical_to_physical(fs->backing_fd, &fs->sb, &inode, lbn, tmp, &pbn) <
        0) {
      // 아직 물리 블록 할당 안 된 경우 새 블록 할당
      int new_off = alloc_block(&fs->sb, fs->block_map);
      pbn = fs->sb.data_block_start + new_off;
      if (lbn < SFUSE_NDIR_BLOCKS)
        inode.direct[lbn] = pbn;
    }
    read_block(fs->backing_fd, pbn, tmp);
    memcpy(tmp + boff, buf + written, chunk);
    write_block(fs->backing_fd, pbn, tmp);
    written += chunk;
  }
  if (offset + written > inode.size)
    inode.size = offset + written;
  inode.mtime = inode.ctime = (uint32_t)time(NULL);
  inode_sync(fs->backing_fd, &fs->sb, ino, &inode);
  return written;
}

/* create */
static int sfuse_create_cb(const char *path, mode_t mode,
                           struct fuse_file_info *fi) {
  struct sfuse_fs *fs = get_fs_context();
  uint32_t parent;
  char *name = fs_split_path(path, &parent);
  int ino = alloc_inode(&fs->sb, fs->inode_map);
  if (ino < 0) {
    free(name);
    return -ENOSPC;
  }
  struct sfuse_inode newnode;
  fs_init_inode(&fs->sb, ino, mode, fuse_get_context()->uid,
                fuse_get_context()->gid, &newnode);
  inode_sync(fs->backing_fd, &fs->sb, ino, &newnode);
  dir_add_entry(fs->backing_fd, &fs->sb, parent, name, ino, fs->block_map,
                fs->inode_map, &fs->sb);
  free(name);
  fi->fh = ino;
  return 0;
}

/* mkdir */
static int sfuse_mkdir_cb(const char *path, mode_t mode) {
  struct sfuse_fs *fs = get_fs_context();
  uint32_t parent;
  char *name = fs_split_path(path, &parent);
  if (!name)
    return -EINVAL;

  int ino = alloc_inode(&fs->sb, fs->inode_map);
  if (ino < 0) {
    free(name);
    return -ENOSPC;
  }

  struct sfuse_inode dir_inode;
  fs_init_inode(&fs->sb, ino, mode | S_IFDIR, fuse_get_context()->uid,
                fuse_get_context()->gid, &dir_inode);

  int blk_index = alloc_block(&fs->sb, fs->block_map);
  if (blk_index < 0) {
    free_inode(&fs->sb, fs->inode_map, ino);
    free(name);
    return -ENOSPC;
  }

  dir_inode.direct[0] = fs->sb.data_block_start + blk_index;
  dir_inode.size = SFUSE_BLOCK_SIZE;

  uint8_t block[SFUSE_BLOCK_SIZE] = {0};
  struct sfuse_dirent *entries = (struct sfuse_dirent *)block;
  entries[0].ino = ino;
  strcpy(entries[0].name, ".");
  entries[1].ino = parent;
  strcpy(entries[1].name, "..");
  write_block(fs->backing_fd, dir_inode.direct[0], block);

  inode_sync(fs->backing_fd, &fs->sb, ino, &dir_inode);
  dir_add_entry(fs->backing_fd, &fs->sb, parent, name, ino, fs->block_map,
                fs->inode_map, &fs->sb);

  free(name);
  return 0;
}

/* unlink */
static int sfuse_unlink_cb(const char *path) {
  struct sfuse_fs *fs = get_fs_context();
  uint32_t parent;
  char *name = fs_split_path(path, &parent);
  if (!name)
    return -EINVAL;

  uint32_t ino;
  if (fs_resolve_path(fs, path, &ino) < 0) {
    free(name);
    return -ENOENT;
  }

  struct sfuse_inode inode;
  inode_load(fs->backing_fd, &fs->sb, ino, &inode);
  if (S_ISDIR(inode.mode)) {
    free(name);
    return -EISDIR;
  }

  for (int i = 0; i < SFUSE_NDIR_BLOCKS; i++) {
    if (inode.direct[i])
      free_block(&fs->sb, fs->block_map,
                 inode.direct[i] - fs->sb.data_block_start);
  }

  free_inode(&fs->sb, fs->inode_map, ino);
  dir_remove_entry(fs->backing_fd, &fs->sb, parent, name);

  // 상위 디렉터리 inode 정확히 업데이트
  struct sfuse_inode parent_inode;
  inode_load(fs->backing_fd, &fs->sb, parent, &parent_inode);
  parent_inode.mtime = parent_inode.ctime = time(NULL);
  inode_sync(fs->backing_fd, &fs->sb, parent, &parent_inode);

  bitmap_sync(fs->backing_fd, fs->sb.block_bitmap_start, fs->block_map,
              fs->sb.blocks_count / 8);
  bitmap_sync(fs->backing_fd, fs->sb.inode_bitmap_start, fs->inode_map,
              fs->sb.inodes_count / 8);
  sb_sync(fs->backing_fd, &fs->sb);

  free(name);
  return 0;
}

/* rmdir */
static int sfuse_rmdir_cb(const char *path) {
  struct sfuse_fs *fs = get_fs_context();
  uint32_t parent;
  char *name = fs_split_path(path, &parent);
  if (!name)
    return -EINVAL;

  uint32_t ino;
  if (fs_resolve_path(fs, path, &ino) < 0) {
    free(name);
    return -ENOENT;
  }

  struct sfuse_inode inode;
  inode_load(fs->backing_fd, &fs->sb, ino, &inode);
  if (!S_ISDIR(inode.mode)) {
    free(name);
    return -ENOTDIR;
  }

  uint8_t block[SFUSE_BLOCK_SIZE];
  read_block(fs->backing_fd, inode.direct[0], block);
  struct sfuse_dirent *ents = (struct sfuse_dirent *)block;
  int entries_per_block = SFUSE_BLOCK_SIZE / sizeof(struct sfuse_dirent);

  for (int i = 0; i < entries_per_block; i++) {
    if (ents[i].ino && strcmp(ents[i].name, ".") &&
        strcmp(ents[i].name, "..")) {
      free(name);
      return -ENOTEMPTY;
    }
  }

  free_block(&fs->sb, fs->block_map, inode.direct[0] - fs->sb.data_block_start);
  free_inode(&fs->sb, fs->inode_map, ino);
  dir_remove_entry(fs->backing_fd, &fs->sb, parent, name);

  struct sfuse_inode parent_inode;
  inode_load(fs->backing_fd, &fs->sb, parent, &parent_inode);
  parent_inode.mtime = parent_inode.ctime = time(NULL);
  inode_sync(fs->backing_fd, &fs->sb, parent, &parent_inode);

  bitmap_sync(fs->backing_fd, fs->sb.block_bitmap_start, fs->block_map,
              fs->sb.blocks_count / 8);
  bitmap_sync(fs->backing_fd, fs->sb.inode_bitmap_start, fs->inode_map,
              fs->sb.inodes_count / 8);
  sb_sync(fs->backing_fd, &fs->sb);

  free(name);
  return 0;
}

/* rename */
static int sfuse_rename_cb(const char *oldpath, const char *newpath,
                           unsigned int flags) {
  (void)flags;
  struct sfuse_fs *fs = get_fs_context();
  uint32_t oldp, newp;
  char *oldn = fs_split_path(oldpath, &oldp);
  char *newn = fs_split_path(newpath, &newp);
  uint32_t ino;
  fs_resolve_path(fs, oldpath, &ino);
  // 기존 경로의 디렉터리 엔트리 제거 및 새 경로로 추가
  dir_remove_entry(fs->backing_fd, &fs->sb, oldp, oldn);
  dir_add_entry(fs->backing_fd, &fs->sb, newp, newn, ino, fs->block_map,
                fs->inode_map, &fs->sb);
  free(oldn);
  free(newn);
  return 0;
}

/* truncate */
static int sfuse_truncate_cb(const char *path, off_t size,
                             struct fuse_file_info *fi) {
  (void)fi;
  struct sfuse_fs *fs = get_fs_context();
  uint32_t ino;
  fs_resolve_path(fs, path, &ino);
  struct sfuse_inode inode;
  inode_load(fs->backing_fd, &fs->sb, ino, &inode);
  inode.size = size;
  inode_sync(fs->backing_fd, &fs->sb, ino, &inode);
  return 0;
}

/* utimens */
static int sfuse_utimens_cb(const char *path, const struct timespec ts[2],
                            struct fuse_file_info *fi) {
  (void)fi;
  struct sfuse_fs *fs = get_fs_context();
  uint32_t ino;
  if (fs_resolve_path(fs, path, &ino) < 0)
    return -ENOENT;
  struct sfuse_inode inode;
  if (inode_load(fs->backing_fd, &fs->sb, ino, &inode) < 0)
    return -EIO;
  inode.atime = ts[0].tv_sec;
  inode.mtime = ts[1].tv_sec;
  inode_sync(fs->backing_fd, &fs->sb, ino, &inode);
  return 0;
}

/* flush */
static int sfuse_flush_cb(const char *path, struct fuse_file_info *fi) {
  (void)path;
  (void)fi;
  struct sfuse_fs *fs = get_fs_context();
  if (fsync(fs->backing_fd) < 0)
    return -errno;
  return 0;
}

/* fsync */
static int sfuse_fsync_cb(const char *path, int datasync,
                          struct fuse_file_info *fi) {
  (void)path;
  (void)fi;
  struct sfuse_fs *fs = get_fs_context();
  int res = datasync ? fdatasync(fs->backing_fd) : fsync(fs->backing_fd);
  if (res < 0)
    return -errno;
  return 0;
}

static int sfuse_statfs_cb(const char *path, struct statvfs *st) {
  struct sfuse_fs *fs = get_fs_context();
  memset(st, 0, sizeof(*st));

  st->f_bsize = SFUSE_BLOCK_SIZE; // 블록 크기 (4096 바이트)
  st->f_frsize = SFUSE_BLOCK_SIZE;
  st->f_blocks = fs->sb.blocks_count - fs->sb.data_block_start;
  st->f_bfree = fs->sb.free_blocks;
  st->f_bavail = fs->sb.free_blocks;
  st->f_files = fs->sb.inodes_count;
  st->f_ffree = fs->sb.free_inodes;
  st->f_namemax = SFUSE_NAME_LEN;

  return 0;
}

// 확장 속성(xattr) 요청 무시 - 지원하지 않음 처리
static int sfuse_getxattr_cb(const char *path, const char *name, char *value,
                             size_t size) {
  // 확장 속성을 지원하지 않는다는 오류 반환
  return -ENOTSUP; // 또는 -EOPNOTSUPP로도 가능
}

// 확장 속성 목록(listxattr) 요청 무시 - 지원하지 않음 처리
static int sfuse_listxattr_cb(const char *path, char *list, size_t size) {
  // 확장 속성을 지원하지 않는다는 오류 반환
  return -ENOTSUP; // 또는 -EOPNOTSUPP로도 가능
}

// fuse_operations 구조체에 추가하여 최종 적용
const struct fuse_operations sfuse_ops = {
    .init = sfuse_init_cb,
    .destroy = sfuse_destroy_cb,
    .getattr = sfuse_getattr_cb,
    .access = sfuse_access_cb,
    .readdir = sfuse_readdir_cb,
    .open = sfuse_open_cb,
    .read = sfuse_read_cb,
    .write = sfuse_write_cb,
    .create = sfuse_create_cb,
    .mkdir = sfuse_mkdir_cb,
    .unlink = sfuse_unlink_cb,
    .rmdir = sfuse_rmdir_cb,
    .rename = sfuse_rename_cb,
    .truncate = sfuse_truncate_cb,
    .utimens = sfuse_utimens_cb,
    .flush = sfuse_flush_cb,
    .fsync = sfuse_fsync_cb,
    .statfs = sfuse_statfs_cb,     // statfs 콜백 (파일시스템 용량 정보 표시용)
    .getxattr = sfuse_getxattr_cb, // 추가된 부분
    .listxattr = sfuse_listxattr_cb, // 추가된 부분
};
