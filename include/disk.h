/**
 * @file disk.h
 * @brief 원시 디바이스에 대한 저수준 I/O 연산 함수 선언
 *
 * 본 파일은 원시 디바이스에 직접 접근하여 데이터를 읽고 쓰는 기능을 제공한다.
 *
 * 디스크 접근은 보통 슈퍼블록, 아이노드, 데이터 블록 등의 읽기/쓰기에 사용되며,
 * 다른 상위 레벨 함수들에서 활용된다.
 *
 * @note read_block()과 write_block() 함수는 중복 코드를 방지하기 위해 별도의
 * 파일(block.c)에 구현되어 있다.
 */

#ifndef SFUSE_DISK_H
#define SFUSE_DISK_H

#include "super.h" // SFUSE_BLOCK_SIZE, SFUSE_SUPERBLOCK_OFFSET
#include <stdint.h>
#include <unistd.h>

/**
 * @brief 원시 디바이스에서 특정 offset으로부터 데이터를 읽는다.
 *
 * 이 함수는 주어진 파일 디스크립터(fd)를 사용하여 지정된 offset 위치에서부터
 * size 바이트만큼 데이터를 읽어 buf가 가리키는 메모리 영역에 저장한다.
 *
 * 내부적으로는 디바이스 내 offset 위치로 lseek를 수행한 뒤 read 시스템 호출을
 * 통해 읽기를 수행한다.
 *
 * @param fd     원시 디바이스 파일 디스크립터 (open으로 열림)
 * @param buf    데이터를 저장할 메모리 버퍼의 포인터
 * @param size   읽으려는 데이터의 크기(바이트 단위)
 * @param offset 읽기를 시작할 원시 디바이스 내의 바이트 단위 오프셋
 *
 * @return 성공 시 실제로 읽은 바이트 수를 반환한다.
 *         실패 시 음수(-errno)를 반환하며, errno는 오류를 나타낸다.
 * @retval -EIO   입출력 오류가 발생한 경우
 * @retval -EBADF 유효하지 않은 파일 디스크립터를 전달한 경우
 */
ssize_t disk_read(int fd, void *buf, size_t size, off_t offset);

/**
 * @brief 원시 디바이스의 특정 오프셋(offset)에 데이터를 쓴다.
 *
 * 이 함수는 주어진 파일 디스크립터(fd)를 사용하여 지정된 offset 위치에서부터
 * buf가 가리키는 데이터를 size 바이트만큼 디스크에 기록한다.
 *
 * 내부적으로는 디바이스 내 offset 위치로 lseek를 수행한 뒤 write 시스템 호출을
 * 통해 기록을 수행한다.
 *
 * @param fd     원시 디바이스 파일 디스크립터 (open으로 열림)
 * @param buf    기록할 데이터를 담고 있는 메모리 버퍼의 포인터
 * @param size   기록할 데이터의 크기(바이트 단위)
 * @param offset 기록을 시작할 원시 디바이스 내의 바이트 단위 오프셋
 *
 * @return 성공 시 실제로 기록된 바이트 수를 반환한다.
 *         실패 시 음수(-errno)를 반환하며, errno는 오류를 나타낸다.
 * @retval -EIO   입출력 오류가 발생한 경우
 * @retval -EBADF 유효하지 않은 파일 디스크립터를 전달한 경우
 * @retval -ENOSPC 디스크 공간이 부족한 경우
 */
ssize_t disk_write(int fd, const void *buf, size_t size, off_t offset);

#endif // SFUSE_DISK_H

// NOTE: read_block()과 write_block() 함수는 중복 코드를 방지하기 위해
// block.c에구현되어 있다.
