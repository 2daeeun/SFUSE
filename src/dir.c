#include "dir.h"
#include "block.h"
#include "inode.h"
#include <errno.h>
#include <string.h>

/* Entries per directory block */
#define DENTS_PER_BLOCK (SFUSE_BLOCK_SIZE / sizeof(struct sfuse_dirent))

int dir_lookup(const struct sfuse_fs *fs, uint32_t dir_ino, const char *name, uint32_t *out_ino) {
    struct sfuse_inode dir_inode;
    if (inode_load(fs->backing_fd, &fs->sb, dir_ino, &dir_inode) < 0) {
        return -ENOENT;
    }
    /* Iterate over directory entries in each direct block */
    for (uint32_t i = 0; i < 12; i++) {
        uint32_t blk = dir_inode.direct[i];
        if (blk == 0) {
            continue;  /* no more blocks */
        }
        struct sfuse_dirent entries[DENTS_PER_BLOCK];
        if (read_block(fs->backing_fd, blk, entries) < 0) {
            return -EIO;
        }
        for (int j = 0; j < DENTS_PER_BLOCK; ++j) {
            if (entries[j].inode == 0) {
                continue;
            }
            if (strncmp(entries[j].name, name, SFUSE_NAME_MAX) == 0) {
                *out_ino = entries[j].inode;
                return 0;
            }
        }
    }
    return -ENOENT;
}

int dir_list(const struct sfuse_fs *fs, uint32_t dir_ino, void *buf, fuse_fill_dir_t filler, off_t offset) {
    struct sfuse_inode dir_inode;
    if (inode_load(fs->backing_fd, &fs->sb, dir_ino, &dir_inode) < 0) {
        return -ENOENT;
    }
    /* Add "." and ".." entries (provided by FUSE filler) */
    filler(buf, ".", NULL, 0, 0);
    filler(buf, "..", NULL, 0, 0);
    /* List all directory entries from storage */
    for (uint32_t i = 0; i < 12; i++) {
        uint32_t blk = dir_inode.direct[i];
        if (blk == 0) {
            break;
        }
        struct sfuse_dirent entries[DENTS_PER_BLOCK];
        if (read_block(fs->backing_fd, blk, entries) < 0) {
            return -EIO;
        }
        for (int j = 0; j < DENTS_PER_BLOCK; ++j) {
            if (entries[j].inode == 0) {
                continue;
            }
            if (entries[j].name[0] == '\0') {
                continue;
            }
            filler(buf, entries[j].name, NULL, 0, 0);
        }
    }
    return 0;
}
