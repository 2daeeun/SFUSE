#ifndef SFUSE_BITMAP_H
#define SFUSE_BITMAP_H

#include "super.h" // struct sfuse_super 정의
#include <stddef.h>
#include <stdint.h>

/**
 * @brief 비트맵 블록에서 데이터 읽기
 * @param fd         디바이스 FD
 * @param block_no   비트맵이 저장된 블록 번호
 * @param map        읽어들인 비트맵을 저장할 버퍼
 * @param map_size   버퍼 크기(바이트)
 * @return 0 성공, 음수 오류코드
 */
int bitmap_load(int fd, uint32_t block_no, uint8_t *map, size_t map_size);

/**
 * @brief 비트맵 데이터를 디스크에 기록
 * @param fd         디바이스 FD
 * @param block_no   비트맵 블록 번호
 * @param map        기록할 비트맵 버퍼
 * @param map_size   버퍼 크기(바이트)
 * @return 0 성공, 음수 오류코드
 */
int bitmap_sync(int fd, uint32_t block_no, uint8_t *map, size_t map_size);

/**
 * @brief 새로운 데이터 블록 할당
 * @param sb         슈퍼블록 구조체
 * @param block_map  블록 비트맵 버퍼
 * @return 할당된 블록 오프셋(0부터), 실패 시 -ENOSPC
 */
int alloc_block(struct sfuse_super *sb, uint8_t *block_map);

/**
 * @brief 데이터 블록 해제
 * @param sb         슈퍼블록 구조체
 * @param block_map  블록 비트맵 버퍼
 * @param offset     해제할 블록 오프셋(0부터)
 */
void free_block(struct sfuse_super *sb, uint8_t *block_map, uint32_t offset);

/**
 * @brief 새로운 아이노드 할당
 * @param sb         슈퍼블록 구조체
 * @param inode_map  아이노드 비트맵 버퍼
 * @return 할당된 아이노드 번호, 실패 시 -ENOSPC
 */
int alloc_inode(struct sfuse_super *sb, uint8_t *inode_map);

/**
 * @brief 아이노드 해제
 * @param sb         슈퍼블록 구조체
 * @param inode_map  아이노드 비트맵 버퍼
 * @param ino        해제할 아이노드 번호
 */
void free_inode(struct sfuse_super *sb, uint8_t *inode_map, uint32_t ino);

#endif // SFUSE_BITMAP_H
