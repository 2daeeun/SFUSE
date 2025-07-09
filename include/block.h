/**
 * @file include/block.h
 * @brief 블록 단위 읽기 및 쓰기를 위한 인터페이스 정의
 *
 * 디바이스와의 입출력을 블록 단위로 처리하기 위한 인터페이스를 제공한다.
 * 블록 크기는 SFUSE_BLOCK_SIZE (일반적으로 4KB)로 고정된다.
 */

#ifndef SFUSE_BLOCK_H
#define SFUSE_BLOCK_H

#include <stdint.h>

/**
 * @brief 디바이스에서 블록 단위(4KB)로 읽기
 * @param fd        디바이스 파일 디스크립터
 * @param block_no  읽을 블록 번호 (0부터 시작)
 * @param buf       SFUSE_BLOCK_SIZE 바이트 버퍼
 * @return 0 성공, 음수 오류코드
 */
int read_block(int fd, uint32_t block_no, void *buf);

/**
 * @brief 디바이스에 블록 단위(4KB)로 쓰기
 * @param fd        디바이스 파일 디스크립터
 * @param block_no  쓸 블록 번호 (0부터 시작)
 * @param buf       SFUSE_BLOCK_SIZE 바이트 데이터 버퍼
 * @return 0 성공, 음수 오류코드
 */
int write_block(int fd, uint32_t block_no, const void *buf);

#endif // SFUSE_BLOCK_H
