#include "inode.h"
#include "block.h"
#include <errno.h>
#include <unistd.h>

/* Compute byte offset of an inode in the inode table */
static off_t inode_offset(const struct sfuse_superblock *sb, uint32_t ino) {
    uint32_t inode_index = ino;
    off_t block_index = sb->inode_table_start + inode_index / SFUSE_INODES_PER_BLOCK;
    off_t offset_within_block = (inode_index % SFUSE_INODES_PER_BLOCK) * sizeof(struct sfuse_inode);
    return block_index * SFUSE_BLOCK_SIZE + offset_within_block;
}

int inode_load(int fd, const struct sfuse_superblock *sb, uint32_t ino, struct sfuse_inode *out) {
    off_t offs = inode_offset(sb, ino);
    ssize_t n = pread(fd, out, sizeof(struct sfuse_inode), offs);
    if (n < 0) {
        return -errno;
    }
    return (n == sizeof(struct sfuse_inode) ? 0 : -EIO);
}

int inode_sync(int fd, const struct sfuse_superblock *sb, uint32_t ino, const struct sfuse_inode *in) {
    off_t offs = inode_offset(sb, ino);
    ssize_t n = pwrite(fd, in, sizeof(struct sfuse_inode), offs);
    if (n < 0) {
        return -errno;
    }
    return (n == sizeof(struct sfuse_inode) ? 0 : -EIO);
}
