/**
 * @file src/block.c
 * @brief 블록 단위 입출력 헬퍼 함수 구현
 *
 * 디바이스 파일에서 블록 단위로 데이터를 읽고 쓰는 함수들을 제공한다.
 * 내부적으로 disk_read()와 disk_write()를 호출하여 실제 입출력을 수행한다.
 */

#include "block.h"
#include "disk.h"
#include "super.h" // SFUSE_BLOCK_SIZE
#include <errno.h>
#include <stdint.h>
#include <unistd.h>

/**
 * @brief 디바이스에서 지정된 블록 번호의 데이터를 읽는다.
 *
 * 블록 번호를 이용하여 디바이스 파일의 정확한 위치(offset)를 계산한 후,
 * disk_read()를 호출하여 해당 위치에서 블록 단위의 데이터를 읽는다.
 * 읽기 작업은 SFUSE_BLOCK_SIZE 단위로 이루어진다.
 *
 * @param fd 디바이스 파일 디스크립터
 * @param block_no 읽을 블록 번호 (0부터 시작)
 * @param buf 읽은 데이터를 저장할 SFUSE_BLOCK_SIZE 크기의 버퍼
 * @return 0 성공, 음수 오류 코드
 *         -EIO: 읽은 데이터 크기가 SFUSE_BLOCK_SIZE와 다를 경우
 *         disk_read()에서 반환된 음수의 오류 코드
 */
int read_block(int fd, uint32_t block_no, void *buf) {
  // 블록 번호를 실제 파일의 바이트 오프셋으로 변환
  off_t offset = (off_t)block_no * SFUSE_BLOCK_SIZE;

  // 계산된 오프셋에서 블록 크기만큼 데이터를 읽기
  ssize_t ret = disk_read(fd, buf, SFUSE_BLOCK_SIZE, offset);

  // disk_read 실패 시 오류 코드 반환
  if (ret < 0)
    return (int)ret;

  // 읽은 데이터가 요청한 블록 크기와 일치하지 않을 경우 EIO 오류 반환
  if ((size_t)ret != SFUSE_BLOCK_SIZE)
    return -EIO;

  return 0;
}

/**
 * @brief 디바이스의 지정된 블록 번호 위치에 데이터를 기록한다.
 *
 * 블록 번호를 기반으로 디바이스 파일 내의 정확한 위치(offset)를 계산한 후,
 * disk_write()를 호출하여 해당 위치에 블록 크기의 데이터를 기록한다.
 * 쓰기 작업은 SFUSE_BLOCK_SIZE 단위로 이루어진다.
 *
 * @param fd 디바이스 파일 디스크립터
 * @param block_no 기록할 블록 번호 (0부터 시작)
 * @param buf 기록할 데이터를 담고 있는 SFUSE_BLOCK_SIZE 크기의 버퍼
 * @return 0 성공, 음수 오류 코드
 *         -EIO: 기록한 데이터 크기가 SFUSE_BLOCK_SIZE와 다를 경우
 *         disk_write()에서 반환된 음수의 오류 코드
 */
int write_block(int fd, uint32_t block_no, const void *buf) {
  // 블록 번호를 실제 파일의 바이트 오프셋으로 변환
  off_t offset = (off_t)block_no * SFUSE_BLOCK_SIZE;

  // 계산된 오프셋에서 블록 크기만큼 데이터를 쓰기
  ssize_t ret = disk_write(fd, buf, SFUSE_BLOCK_SIZE, offset);

  // disk_write 실패 시 오류 코드 반환
  if (ret < 0)
    return (int)ret;

  // 기록한 데이터가 요청한 블록 크기와 일치하지 않을 경우 EIO 오류 반환
  if ((size_t)ret != SFUSE_BLOCK_SIZE)
    return -EIO;

  return 0;
}
