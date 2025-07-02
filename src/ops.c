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
  struct sfuse_fs *fs = get_fs_context();
  uint32_t ino;

  if (fs_resolve_path(fs, path, &ino) < 0)
    return -ENOENT;

  struct sfuse_inode inode;
  if (inode_load(fs->backing_fd, &fs->sb, ino, &inode) < 0)
    return -EIO;

  size_t current_offset = 0;

  if (current_offset >= offset)
    if (filler(buf, ".", NULL, ++current_offset, 0))
      return 0;

  if (current_offset >= offset)
    if (filler(buf, "..", NULL, ++current_offset, 0))
      return 0;

  size_t entries_per_block = SFUSE_BLOCK_SIZE / sizeof(struct sfuse_dirent);
  size_t total_entries = inode.size / sizeof(struct sfuse_dirent);

  for (int b = 0; b < SFUSE_NDIR_BLOCKS; b++) {
    uint32_t blkno = inode.direct[b];
    if (!blkno)
      continue;

    if (blkno < fs->sb.data_block_start || blkno >= fs->sb.blocks_count)
      return -EIO;

    uint8_t block[SFUSE_BLOCK_SIZE];
    if (read_block(fs->backing_fd, blkno, block) < 0)
      return -EIO;

    struct sfuse_dirent *entries = (struct sfuse_dirent *)block;

    for (size_t i = 0; i < entries_per_block; i++) {
      size_t idx = b * entries_per_block + i;
      if (idx >= total_entries)
        break;

      if (!entries[i].ino || !strlen(entries[i].name))
        continue;

      if (!strcmp(entries[i].name, ".") || !strcmp(entries[i].name, ".."))
        continue;

      current_offset++;

      if (current_offset <= offset)
        continue;

      if (filler(buf, entries[i].name, NULL, current_offset, 0))
        return 0;
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
  struct sfuse_fs *fs = get_fs_context();
  uint32_t ino;

  if (fs_resolve_path(fs, path, &ino) < 0)
    return -ENOENT;

  struct sfuse_inode inode;
  if (inode_load(fs->backing_fd, &fs->sb, ino, &inode) < 0)
    return -EIO;

  if (size > inode.size) {
    // 파일 크기 확장 시 새로 추가되는 영역 0으로 초기화
    uint8_t zero_block[SFUSE_BLOCK_SIZE] = {0};
    while (inode.size < size) {
      int blk_index = alloc_block(&fs->sb, fs->block_map);
      if (blk_index < 0)
        return -ENOSPC;
      uint32_t phys_block = fs->sb.data_block_start + blk_index;
      size_t direct_index = inode.size / SFUSE_BLOCK_SIZE;
      if (direct_index >= SFUSE_NDIR_BLOCKS)
        return -EFBIG; // 직접 블록만 지원한다고 가정
      inode.direct[direct_index] = phys_block;
      write_block(fs->backing_fd, phys_block, zero_block);
      inode.size += SFUSE_BLOCK_SIZE;
    }
  } else if (size < inode.size) {
    // 파일 크기 축소 시 블록 해제
    while (inode.size > size) {
      size_t direct_index = (inode.size - 1) / SFUSE_BLOCK_SIZE;
      if (direct_index < SFUSE_NDIR_BLOCKS && inode.direct[direct_index]) {
        free_block(&fs->sb, fs->block_map,
                   inode.direct[direct_index] - fs->sb.data_block_start);
        inode.direct[direct_index] = 0;
      }
      inode.size -= SFUSE_BLOCK_SIZE;
    }
  }

  inode.size = size;
  inode.mtime = inode.ctime = time(NULL);
  inode_sync(fs->backing_fd, &fs->sb, ino, &inode);

  bitmap_sync(fs->backing_fd, fs->sb.block_bitmap_start, fs->block_map,
              fs->sb.blocks_count / 8);
  sb_sync(fs->backing_fd, &fs->sb);
  return 0;
}

/* utimens */
static int sfuse_utimens_cb(const char *path, const struct timespec tv[2],
                            struct fuse_file_info *fi) {
  struct sfuse_fs *fs = get_fs_context();
  uint32_t ino;

  if (fs_resolve_path(fs, path, &ino) < 0)
    return -ENOENT;

  struct sfuse_inode inode;
  if (inode_load(fs->backing_fd, &fs->sb, ino, &inode) < 0)
    return -EIO;

  inode.atime = tv[0].tv_sec;
  inode.mtime = tv[1].tv_sec;
  inode.ctime = time(NULL); // ctime은 현재 시간으로 업데이트
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

/* statfs */
/* SFUSE statfs 콜백 구현 (파일 시스템 용량 정보 제공) */
static int sfuse_statfs_cb(const char *path, struct statvfs *stbuf) {
  struct sfuse_fs *fs = get_fs_context();

  memset(stbuf, 0, sizeof(struct statvfs));

  /* VSFS의 슈퍼블록(sb)을 참조하여 채우기 */
  stbuf->f_bsize = SFUSE_BLOCK_SIZE; // 블록 크기 (VSFS의 블록 크기)
  stbuf->f_frsize =
      SFUSE_BLOCK_SIZE; // 프래그먼트 크기 (대부분 블록 크기와 같음)

  /* 블록 개수 설정 (슈퍼블록 값 활용) */
  stbuf->f_blocks =
      fs->sb.blocks_count - fs->sb.data_block_start; // 데이터 블록의 전체 개수
  stbuf->f_bfree = fs->sb.free_blocks;               // 빈 블록 개수 (여유 공간)
  stbuf->f_bavail = fs->sb.free_blocks; // 일반 사용자가 쓸 수 있는 빈 블록 수

  /* 아이노드 정보 설정 (VSFS 슈퍼블록 값 활용) */
  stbuf->f_files = fs->sb.inodes_count; // 전체 아이노드 개수
  stbuf->f_ffree = fs->sb.free_inodes;  // 빈 아이노드 개수
  stbuf->f_favail = fs->sb.free_inodes; // 일반 사용자 사용 가능 아이노드 수

  /* FS 고유 식별자 및 기타 정보 */
  stbuf->f_fsid = 0x53465553;        // 임의 FSID (예: "SFUS" ASCII 코드값)
  stbuf->f_flag = ST_NOSUID;         // 마운트 옵션에 따라 설정 가능
  stbuf->f_namemax = SFUSE_NAME_LEN; // 최대 파일 이름 길이

  return 0;
}

// 확장 속성을 지원하지 않음: 빈 목록 반환으로 안정적으로 처리
static int sfuse_listxattr_cb(const char *path, char *list, size_t size) {
  return 0; // 빈 리스트 반환 (성공적으로 처리됨)
}

// 확장 속성을 지원하지 않음: ENODATA(ENOATTR) 반환으로 안정적으로 처리
static int sfuse_getxattr_cb(const char *path, const char *name, char *value,
                             size_t size) {
  return -ENODATA; // 해당 속성 없음 처리 (안정적으로 처리됨)
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
    .statfs = sfuse_statfs_cb,
    .getxattr = sfuse_getxattr_cb,
    .listxattr = sfuse_listxattr_cb,
};
