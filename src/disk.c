/**
 * @file disk.c
 * @brief 원시 디바이스에 직접 접근하는 저수준 I/O 연산 구현
 *
 * 이 파일은 disk.h에서 선언된 disk_read()와 disk_write()를 구현하며,
 * 주어진 원시 디바이스에 직접 접근하여 데이터를 읽고 쓰는 기능을 제공한다.
 *
 * @note read_block()과 write_block() 함수는 중복 코드를 방지하기 위해 별도의
 * 파일(block.c)에 구현되어 있다.
 */

#include "disk.h"
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>

/**
 * @brief 원시 디바이스의 지정된 offset에서 데이터를 읽는다.
 *
 * 본 함수는 lseek를 통해 디스크 내 원하는 위치(offset)로 이동한 뒤, read를 통해
 * 데이터를 읽는다. offset은 파일의 시작 위치로부터 바이트 단위로 계산된다.
 *
 * @param fd     디바이스 파일 디스크립터
 * @param buf    읽은 데이터를 저장할 버퍼 포인터
 * @param count  읽을 데이터의 크기(바이트 단위)
 * @param off    읽기 시작할 위치 (바이트 단위 오프셋)
 *
 * @return 성공 시 실제로 읽은 바이트 수 반환. 실패 시 음수(-errno) 반환.
 *
 * @note 반환된 바이트 수는 요청한 count 값보다 작을 수 있다.
 */
ssize_t disk_read(int fd, void *buf, size_t count, off_t off) {
  // 지정된 위치로 디스크 포인터 이동
  if (lseek(fd, off, SEEK_SET) < 0)
    return -errno; // lseek 실패 시 errno 반환

  // 데이터를 실제로 읽음
  ssize_t ret = read(fd, buf, count);
  if (ret < 0)
    return -errno; // 읽기 실패 시 errno 반환

  return ret; // 읽기 성공 시 읽은 바이트 수 반환
}

/**
 * @brief 원시 디바이스의 지정된 offset에 데이터를 기록한다.
 *
 * 본 함수는 lseek를 통해 디스크 내 원하는 위치(offset)로 이동한 뒤, write를
 * 통해 데이터를 쓴다. offset은 파일의 시작 위치로부터 바이트 단위로 계산된다.
 *
 * @param fd     디바이스 파일 디스크립터
 * @param buf    기록할 데이터가 저장된 버퍼 포인터
 * @param count  기록할 데이터 크기(바이트 단위)
 * @param off    기록을 시작할 위치 (바이트 단위 오프셋)
 *
 * @return 성공 시 실제로 기록된 바이트 수 반환. 실패 시 음수(-errno) 반환.
 *
 * @note 기록된 바이트 수가 요청한 count 값보다 작을 수 있다. 디스크가 꽉 차는
 * 등의 상황에서는 ENOSPC 오류가 발생할 수 있다.
 */
ssize_t disk_write(int fd, const void *buf, size_t count, off_t off) {
  // 지정된 위치로 디스크 포인터 이동
  if (lseek(fd, off, SEEK_SET) < 0)
    return -errno; // lseek 실패 시 errno 반환

  // 데이터를 실제로 기록함
  ssize_t ret = write(fd, buf, count);
  if (ret < 0)
    return -errno; // 기록 실패 시 errno 반환

  return ret; // 기록 성공 시 기록한 바이트 수 반환
}

// NOTE: read_block()과 write_block() 함수는 중복 코드를 방지하기 위해
// block.c에구현되어 있다.
