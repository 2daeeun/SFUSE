/**
 * @file include/bitmap.h
 * @brief 비트맵 관리 함수 선언
 *
 * bitmap.h에서는 데이터 블록과 아이노드를 관리하기 위한 비트맵 관련 함수들의
 * 인터페이스를 정의한다. 비트맵 로딩, 디스크 동기화 및 블록과 아이노드
 * 할당/해제 기능을 제공한다.
 */

#ifndef SFUSE_BITMAP_H
#define SFUSE_BITMAP_H

#include "super.h" // struct sfuse_super 정의
#include <stddef.h>
#include <stdint.h>

/**
 * @brief 비트맵 데이터를 디스크에서 로드
 *
 * 디스크의 특정 블록 번호에서 비트맵 데이터를 읽어 메모리에 저장한다.
 *
 * @param fd 디바이스 파일 디스크립터
 * @param block_no 비트맵이 저장된 블록 번호
 * @param map 읽어온 비트맵 데이터를 저장할 버퍼
 * @param map_size 버퍼의 크기 (바이트 단위)
 * @return 성공 시 0, 실패 시 음수 오류 코드 반환
 */
int bitmap_load(int fd, uint32_t block_no, uint8_t *map, size_t map_size);

/**
 * @brief 비트맵 데이터를 디스크에 기록
 *
 * 메모리에 있는 비트맵 데이터를 지정된 디스크 블록에 저장한다.
 *
 * @param fd 디바이스 파일 디스크립터
 * @param block_no 비트맵을 기록할 블록 번호
 * @param map 기록할 비트맵 데이터를 담고 있는 버퍼
 * @param map_size 버퍼의 크기 (바이트 단위)
 * @return 성공 시 0, 실패 시 음수 오류 코드 반환
 */
int bitmap_sync(int fd, uint32_t block_no, uint8_t *map, size_t map_size);

/**
 * @brief 데이터 블록 할당
 *
 * 사용 가능한 빈 블록을 찾아 할당하고, 비트맵을 업데이트한다.
 *
 * @param sb 슈퍼블록 구조체 포인터
 * @param block_map 블록 비트맵 버퍼
 * @return 할당된 블록의 오프셋(0부터 시작), 실패 시 -ENOSPC
 */
int alloc_block(struct sfuse_super *sb, uint8_t *block_map);

/**
 * @brief 데이터 블록 해제
 *
 * 할당된 블록을 해제하고 비트맵을 업데이트한다.
 *
 * @param sb 슈퍼블록 구조체 포인터
 * @param block_map 블록 비트맵 버퍼
 * @param offset 해제할 블록의 오프셋(0부터 시작)
 */
void free_block(struct sfuse_super *sb, uint8_t *block_map, uint32_t offset);

/**
 * @brief 아이노드 할당
 *
 * 비어 있는 아이노드를 찾아 할당하고, 아이노드 비트맵을 업데이트한다.
 *
 * @param sb 슈퍼블록 구조체 포인터
 * @param inode_map 아이노드 비트맵 버퍼
 * @return 할당된 아이노드 번호, 실패 시 -ENOSPC
 */
int alloc_inode(struct sfuse_super *sb, uint8_t *inode_map);

/**
 * @brief 아이노드 해제
 *
 * 할당된 아이노드를 해제하고, 아이노드 비트맵을 업데이트한다.
 *
 * @param sb 슈퍼블록 구조체 포인터
 * @param inode_map 아이노드 비트맵 버퍼
 * @param ino 해제할 아이노드 번호
 */
void free_inode(struct sfuse_super *sb, uint8_t *inode_map, uint32_t ino);

#endif // SFUSE_BITMAP_H
