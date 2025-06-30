// inode.c: 아이노드 로드/동기화 및 초기화 구현
#define FUSE_USE_VERSION 26
#include "inode.h"
#include "block.h" // read_block/write_block
#include "img.h"
#include "super.h"
#include <errno.h>
#include <string.h>
#include <sys/stat.h> // S_ISDIR
#include <time.h>
#include <unistd.h>

int inode_load(int fd, const struct sfuse_super *sb, uint32_t ino,
               struct sfuse_inode *inode) {
  if (ino == 0 || ino >= sb->inodes_count)
    return -EINVAL;
  off_t off = ((off_t)sb->inode_table_start * SFUSE_BLOCK_SIZE) +
              (ino * sizeof(*inode));
  ssize_t ret = img_read(fd, inode, sizeof(*inode), off);
  if (ret < 0)
    return (int)ret;
  if ((size_t)ret != sizeof(*inode))
    return -EIO;
  return 0;
}

int inode_sync(int fd, const struct sfuse_super *sb, uint32_t ino,
               const struct sfuse_inode *inode) {
  if (ino == 0 || ino >= sb->inodes_count)
    return -EINVAL;
  off_t off = ((off_t)sb->inode_table_start * SFUSE_BLOCK_SIZE) +
              (ino * sizeof(*inode));
  ssize_t ret = img_write(fd, inode, sizeof(*inode), off);
  if (ret < 0)
    return (int)ret;
  if ((size_t)ret != sizeof(*inode))
    return -EIO;
  return 0;
}

void fs_init_inode(const struct sfuse_super *sb, uint32_t ino, mode_t mode,
                   uid_t uid, gid_t gid, struct sfuse_inode *inode) {
  (void)sb;
  (void)ino;
  memset(inode, 0, sizeof(*inode));
  inode->mode = mode;
  inode->uid = uid;
  inode->gid = gid;
  inode->size = 0;
  inode->atime = inode->mtime = inode->ctime = (uint32_t)time(NULL);
  for (int i = 0; i < SFUSE_NDIR_BLOCKS; i++)
    inode->direct[i] = 0;
  inode->indirect = 0;
  inode->double_indirect = 0;
  inode->links = (S_ISDIR(mode) ? 2 : 1);
}

int logical_to_physical(int fd, const struct sfuse_super *sb,
                        const struct sfuse_inode *inode, uint32_t lbn,
                        void *buf, uint32_t *pbn_out) {
  if (lbn < SFUSE_NDIR_BLOCKS) {
    *pbn_out = inode->direct[lbn];
    return 0;
  }
  lbn -= SFUSE_NDIR_BLOCKS;
  uint32_t entries = SFUSE_BLOCK_SIZE / sizeof(uint32_t);
  if (lbn < entries) {
    if (inode->indirect == 0)
      return -ENOENT;
    if (read_block(fd, inode->indirect, buf) < 0)
      return -EIO;
    uint32_t *ptrs = (uint32_t *)buf;
    *pbn_out = ptrs[lbn];
    return 0;
  }
  lbn -= entries;
  uint32_t idx1 = lbn / entries;
  uint32_t idx2 = lbn % entries;
  if (inode->double_indirect == 0)
    return -ENOENT;
  if (read_block(fd, inode->double_indirect, buf) < 0)
    return -EIO;
  uint32_t *ptrs1 = (uint32_t *)buf;
  uint32_t block2 = ptrs1[idx1];
  if (block2 == 0)
    return -ENOENT;
  if (read_block(fd, block2, buf) < 0)
    return -EIO;
  uint32_t *ptrs2 = (uint32_t *)buf;
  *pbn_out = ptrs2[idx2];
  return 0;
}
