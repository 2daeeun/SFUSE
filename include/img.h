#ifndef SFUSE_IMG_H
#define SFUSE_IMG_H

#include "super.h"  // SFUSE_BLOCK_SIZE, SFUSE_SUPERBLOCK_OFFSET
#include <stdint.h> // uint32_t
#include <unistd.h> // ssize_t, off_t

/**
 * @brief 원시 디바이스에서 특정 오프셋만큼 읽기
 * @param fd     디바이스 파일 디스크립터
 * @param buf    데이터를 저장할 버퍼
 * @param size   읽을 바이트 수
 * @param offset 디바이스 내 바이트 오프셋
 * @return 읽은 바이트 수 또는 음수 오류 코드
 */
ssize_t img_read(int fd, void *buf, size_t size, off_t offset);

/**
 * @brief 원시 디바이스에 특정 오프셋만큼 쓰기
 * @param fd     디바이스 파일 디스크립터
 * @param buf    쓸 데이터가 들어있는 버퍼
 * @param size   쓸 바이트 수
 * @param offset 디바이스 내 바이트 오프셋
 * @return 쓴 바이트 수 또는 음수 오류 코드
 */
ssize_t img_write(int fd, const void *buf, size_t size, off_t offset);

/**
 * @brief 블록 넘버 단위로 읽기 (4KB 단위)
 * @param fd       디바이스 FD
 * @param block_no 블록 번호 (0부터 시작)
 * @param buf      SFUSE_BLOCK_SIZE 바이트를 담을 버퍼
 * @return 0 성공, 음수 오류
 */
int read_block(int fd, uint32_t block_no, void *buf);

/**
 * @brief 블록 넘버 단위로 쓰기 (4KB 단위)
 * @param fd       디바이스 FD
 * @param block_no 블록 번호 (0부터 시작)
 * @param buf      SFUSE_BLOCK_SIZE 바이트의 데이터가 담긴 버퍼
 * @return 0 성공, 음수 오류
 */
int write_block(int fd, uint32_t block_no, const void *buf);

#endif // SFUSE_IMG_H
