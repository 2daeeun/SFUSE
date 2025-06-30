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
  if (sb_load(fs->backing_fd, &fs->sb) < 0) {
    sb_format(&fs->sb);
    sb_sync(fs->backing_fd, &fs->sb);
  }
  size_t bmap_bytes = fs->sb.blocks_count / 8;
  size_t imap_bytes = fs->sb.inodes_count / 8;
  fs->block_map = malloc(bmap_bytes);
  fs->inode_map = malloc(imap_bytes);
  bitmap_load(fs->backing_fd, fs->sb.block_bitmap_start, fs->block_map,
              bmap_bytes);
  bitmap_load(fs->backing_fd, fs->sb.inode_bitmap_start, fs->inode_map,
              imap_bytes);
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
  stbuf->st_nlink = S_ISDIR(inode.mode) ? 2 : 1;
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
  (void)flags;
  struct sfuse_fs *fs = get_fs_context();
  uint32_t ino;
  if (fs_resolve_path(fs, path, &ino) < 0)
    return -ENOENT;
  struct sfuse_inode din;
  inode_load(fs->backing_fd, &fs->sb, ino, &din);
  filler(buf, ".", NULL, 0, flags);
  filler(buf, "..", NULL, 0, flags);
  size_t buf_size = SFUSE_NDIR_BLOCKS * SFUSE_BLOCK_SIZE;
  void *dbuf = malloc(buf_size);
  dir_load(fs->backing_fd, &fs->sb, ino, dbuf);
  struct sfuse_dirent *ents = dbuf;
  size_t count = buf_size / sizeof(*ents);
  for (size_t i = 0; i < count; i++) {
    if (ents[i].ino)
      filler(buf, ents[i].name, NULL, 0, flags);
  }
  free(dbuf);
  return 0;
}

/* open */
static int sfuse_open_cb(const char *path, struct fuse_file_info *fi) {
  struct sfuse_fs *fs = get_fs_context();
  uint32_t ino;
  if (fs_resolve_path(fs, path, &ino) < 0)
    return -ENOENT;
  struct sfuse_inode inode;
  inode_load(fs->backing_fd, &fs->sb, ino, &inode);
  if (S_ISDIR(inode.mode))
    return -EISDIR;
  (void)fi;
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
  size_t to_read = size;
  if (offset + to_read > inode.size)
    to_read = inode.size - offset;
  size_t done = 0;
  uint32_t pbn;
  uint8_t tmp[SFUSE_BLOCK_SIZE];
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
  while (written < size) {
    off_t cur = offset + written;
    uint32_t lbn = cur / SFUSE_BLOCK_SIZE;
    size_t boff = cur % SFUSE_BLOCK_SIZE;
    size_t chunk = SFUSE_BLOCK_SIZE - boff;
    if (chunk > size - written)
      chunk = size - written;
    if (logical_to_physical(fs->backing_fd, &fs->sb, &inode, lbn, tmp, &pbn) <
        0) {
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
  return sfuse_create_cb(path, mode | S_IFDIR, NULL);
}

/* unlink */
static int sfuse_unlink_cb(const char *path) {
  struct sfuse_fs *fs = get_fs_context();
  uint32_t parent;
  char *name = fs_split_path(path, &parent);
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
    if (inode.direct[i]) {
      free_block(&fs->sb, fs->block_map,
                 inode.direct[i] - fs->sb.data_block_start);
    }
  }
  free_inode(&fs->sb, fs->inode_map, ino);
  dir_remove_entry(fs->backing_fd, &fs->sb, parent, name);
  free(name);
  return 0;
}

/* rmdir */
static int sfuse_rmdir_cb(const char *path) { return sfuse_unlink_cb(path); }

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

// FUSE 콜백 테이블
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
};
