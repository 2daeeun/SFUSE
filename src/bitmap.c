#include "bitmap.h"
#include "block.h"
#include <errno.h>

static const uint32_t BITS_PER_BLOCK = SFUSE_BLOCK_SIZE * 8;

int bitmap_load(int fd, uint32_t start_blk, struct sfuse_bitmaps *bmaps, uint32_t count) {
    /* Read 'count' blocks starting at start_blk into the contiguous bitmap memory */
    uint8_t *buffer = (uint8_t *)bmaps;
    for (uint32_t i = 0; i < count; ++i) {
        if (read_block(fd, start_blk + i, buffer + i * SFUSE_BLOCK_SIZE) < 0) {
            return -EIO;
        }
    }
    return 0;
}

int bitmap_sync(int fd, uint32_t start_blk, const struct sfuse_bitmaps *bmaps, uint32_t count) {
    /* Write 'count' blocks from the contiguous bitmap memory back to disk */
    const uint8_t *buffer = (const uint8_t *)bmaps;
    for (uint32_t i = 0; i < count; ++i) {
        if (write_block(fd, start_blk + i, buffer + i * SFUSE_BLOCK_SIZE) < 0) {
            return -EIO;
        }
    }
    return 0;
}

int alloc_bit(uint8_t *map, uint32_t total_bits) {
    uint32_t blocks = (total_bits + BITS_PER_BLOCK - 1) / BITS_PER_BLOCK;
    for (uint32_t b = 0; b < blocks; ++b) {
        uint8_t *block_ptr = map + b * SFUSE_BLOCK_SIZE;
        for (uint32_t byte = 0; byte < SFUSE_BLOCK_SIZE; ++byte) {
            if (block_ptr[byte] == 0xFF) {
                continue;  /* this byte is fully used */
            }
            /* Find the first free bit in this byte */
            for (uint32_t bit = 0; bit < 8; ++bit) {
                if (!(block_ptr[byte] & (1u << bit))) {
                    uint32_t index = b * BITS_PER_BLOCK + byte * 8 + bit;
                    if (index < total_bits) {
                        block_ptr[byte] |= (uint8_t)(1u << bit);
                        return index;
                    }
                }
            }
        }
    }
    return -ENOSPC;
}

void free_bit(uint8_t *map, uint32_t idx) {
    uint32_t byte_index = idx / 8;
    uint32_t bit_offset = idx % 8;
    map[byte_index] &= (uint8_t)~(1u << bit_offset);
}

int alloc_inode(struct sfuse_superblock *sb, struct sfuse_inode_bitmap *imap) {
    int ino = alloc_bit(imap->map, sb->total_inodes);
    if (ino >= 0) {
        sb->free_inodes -= 1;
    }
    return (ino < 0 ? ino : ino);
}

void free_inode(struct sfuse_superblock *sb, struct sfuse_inode_bitmap *imap, uint32_t ino) {
    free_bit(imap->map, ino);
    sb->free_inodes += 1;
}

int alloc_block(struct sfuse_superblock *sb, struct sfuse_block_bitmap *bmap) {
    int blk = alloc_bit(bmap->map, sb->total_blocks);
    if (blk >= 0) {
        sb->free_blocks -= 1;
    }
    return (blk < 0 ? blk : blk);
}

void free_block(struct sfuse_superblock *sb, struct sfuse_block_bitmap *bmap, uint32_t blk) {
    free_bit(bmap->map, blk);
    sb->free_blocks += 1;
}
