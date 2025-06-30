// super.c: 슈퍼블록 로드/동기화 및 포맷 구현
#include "super.h"
#include "img.h"
#include <errno.h>
#include <string.h>
#include <unistd.h>

/**
 * sb_load: 디바이스에서 슈퍼블록을 읽어 로드
 * @fd: 디바이스 파일 디스크립터
 * @sb: 읽어들인 슈퍼블록을 저장할 구조체
 * @return: 0 성공, 음수 오류
 */
int sb_load(int fd, struct sfuse_super *sb) {
  ssize_t ret = img_read(fd, sb, sizeof(*sb), SFUSE_SUPERBLOCK_OFFSET);
  if (ret < 0)
    return ret;
  if ((size_t)ret != sizeof(*sb))
    return -EIO;
  if (sb->magic != SFUSE_MAGIC)
    return -EINVAL;
  return 0;
}

/**
 * sb_sync: 슈퍼블록을 디스크에 쓰기
 * @fd: 디바이스 파일 디스크립터
 * @sb: 디스크에 기록할 슈퍼블록 구조체
 * @return: 0 성공, 음수 오류
 */
int sb_sync(int fd, const struct sfuse_super *sb) {
  ssize_t ret = img_write(fd, sb, sizeof(*sb), SFUSE_SUPERBLOCK_OFFSET);
  if (ret < 0)
    return ret;
  if ((size_t)ret != sizeof(*sb))
    return -EIO;
  return 0;
}

/**
 * sb_format: 슈퍼블록 구조체 초기화
 * @sb: 초기화할 슈퍼블록 구조체
 */
void sb_format(struct sfuse_super *sb) {
  memset(sb, 0, sizeof(*sb));
  sb->magic = SFUSE_MAGIC;
  sb->inodes_count = SFUSE_INODES_COUNT;
  sb->blocks_count = SFUSE_BLOCKS_COUNT;
  sb->free_inodes = sb->inodes_count - 1;
  sb->free_blocks = sb->blocks_count - SFUSE_DATA_BLOCK_START;
  sb->inode_bitmap_start = SFUSE_INODE_BITMAP_BLOCK;
  sb->block_bitmap_start = SFUSE_BLOCK_BITMAP_BLOCK;
  sb->inode_table_start = SFUSE_INODE_TABLE_BLOCK;
  sb->data_block_start = SFUSE_DATA_BLOCK_START;
}
