/**
 * @file src/super.c
 * @brief SFUSE 파일 시스템의 슈퍼블록 관리 함수 구현
 *
 * 이 소스 파일은 슈퍼블록을 로드, 동기화, 포맷하는 함수를 구현한다.
 */

#include "super.h"
#include "disk.h"
#include <errno.h>
#include <string.h>
#include <unistd.h>

/**
 * @brief 디스크에서 슈퍼블록을 읽어오는 함수
 *
 * 디스크의 정해진 위치에서 슈퍼블록을 읽어 메모리로 로드하고,
 * 읽은 데이터가 정상적인지 유효성을 검사한다. 유효성 검사는 읽은
 * 데이터 크기와 슈퍼블록 매직 넘버를 이용하여 수행된다.
 *
 * @param fd 디바이스 파일 디스크립터 (열린 블록 디바이스 파일)
 * @param sb 읽어온 슈퍼블록을 저장할 구조체 포인터
 * @return 성공 시 0을 반환, 실패 시 음수 오류 코드 반환
 *         -EIO: 디스크에서 읽은 바이트 수가 요청한 크기와 일치하지 않음
 *         -EINVAL: 슈퍼블록의 매직 넘버가 예상 값과 다름 (슈퍼블록이
 * 손상되었거나 올바르지 않은 디바이스) 기타 음수 값: disk_read 함수 자체가
 * 반환한 오류 코드
 */
int sb_load(int fd, struct sfuse_super *sb) {
  ssize_t ret;

  // 슈퍼블록을 디스크(SFUSE_SUPERBLOCK_OFFSET)에서 읽어 메모리(sb)에 저장한다.
  ret = disk_read(fd, sb, sizeof(*sb), SFUSE_SUPERBLOCK_OFFSET);

  // disk_read가 실패하면 반환된 음수 오류 값을 그대로 반환한다.
  if (ret < 0)
    return ret;

  // 요청한 슈퍼블록 크기만큼 정확히 읽었는지 확인한다.
  if ((size_t)ret != sizeof(*sb))
    return -EIO; // 읽은 데이터 크기가 슈퍼블록 크기와 다름 (입출력 오류)

  // 슈퍼블록의 매직 넘버를 검사하여 슈퍼블록 유효성 검사를 수행한다.
  if (sb->magic != SFUSE_MAGIC)
    return -EINVAL; // 잘못된 매직 넘버

  // 성공적으로 슈퍼블록을 읽고 유효성을 확인했으므로 0 반환.
  return 0;
}

/**
 * @brief 슈퍼블록 구조체를 디스크에 쓰기(동기화)하는 함수
 *
 * 메모리 상의 슈퍼블록 구조체의 내용을 디스크의 지정된 위치에 기록하여,
 * 파일 시스템의 메타데이터를 영구적으로 저장한다.
 *
 * @param fd 디바이스 파일 디스크립터
 * @param sb 디스크에 기록할 슈퍼블록 구조체의 포인터
 * @return 성공 시 0, 실패 시 음수 오류 코드 반환
 *         -EIO: 기록된 바이트 수가 슈퍼블록의 크기와 일치하지 않음
 *         기타 음수 값: disk_write 함수 자체에서 반환한 오류 코드
 */
int sb_sync(int fd, const struct sfuse_super *sb) {
  ssize_t ret;

  // 슈퍼블록 내용을 디스크(SFUSE_SUPERBLOCK_OFFSET)에 기록
  ret = disk_write(fd, sb, sizeof(*sb), SFUSE_SUPERBLOCK_OFFSET);

  // 쓰기 실패 시, disk_write 함수가 반환한 음수 오류 코드를 반환
  if (ret < 0)
    return ret;

  // 요청한 슈퍼블록 크기만큼 정확히 기록되었는지 확인
  if ((size_t)ret != sizeof(*sb))
    return -EIO;

  // 정상적으로 슈퍼블록이 디스크에 저장됨
  return 0;
}

/**
 * @brief 슈퍼블록 구조체를 기본값으로 초기화(포맷)하는 함수
 *
 * 이 함수는 파일 시스템을 처음 생성할 때 호출되며,
 * 슈퍼블록 구조체에 초기 설정값을 채워서 파일 시스템의 메타데이터를 초기화한다.
 *
 * @param sb 초기화할 슈퍼블록 구조체의 포인터
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
