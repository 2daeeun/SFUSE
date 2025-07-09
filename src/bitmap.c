/**
 * @file src/bitmap.c
 * @brief 비트맵 관리 함수 구현
 *
 * bitmpa.c는 비트맵의 디스크 I/O, 메모리 관리, 데이터 블록 및 아이노드 할당과
 * 해제 관련 함수의 구현을 포함한다.
 * 이 함수들은 슈퍼블록 정보를 기반으로 비트맵을 업데이트하여 파일시스템의
 * 상태를 유지한다.
 */

#include "bitmap.h"
#include "disk.h"
#include "super.h"
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

/**
 * @brief 비트맵 데이터를 디스크에서 읽어 메모리에 로드
 *
 * 파일 시스템에서 블록 또는 아이노드의 할당 여부를 나타내는 비트맵 데이터를
 * 디스크의 특정 블록에서 읽어들여 메모리의 버퍼에 저장한다.
 *
 * 이 함수는 파일 시스템 초기화 및 비트맵 동기화 시 주로 사용된다.
 *
 * @param fd 디바이스 파일 디스크립터 (읽을 대상 디스크 장치)
 * @param block_no 비트맵이 저장된 디스크 내의 블록 번호
 * @param map 비트맵 데이터를 저장할 메모리 버퍼 포인터
 * @param map_size 비트맵 데이터의 크기(바이트 단위)
 *
 * @return 성공 시 0을 반환하며, 오류 발생 시 음수 값으로 오류 코드 반환:
 *         -EIO (I/O 오류), 기타 disk_read가 반환하는 음수 값
 */
int bitmap_load(int fd, uint32_t block_no, uint8_t *map, size_t map_size) {
  ssize_t ret;

  // 디스크에서 비트맵 데이터를 읽어 메모리(map)로 로드한다.
  ret = disk_read(fd, map, map_size, (off_t)block_no * SFUSE_BLOCK_SIZE);
  if (ret < 0)
    return (int)ret; // disk_read 실패: 읽기 자체가 실패함

  // 읽은 데이터 크기가 요청한 크기와 일치하는지 검증한다.
  if ((size_t)ret != map_size)
    return -EIO; // 읽은 데이터 크기가 예상 크기와 불일치

  return 0; // 정상 종료
}

/**
 * @brief 메모리의 비트맵 데이터를 디스크에 기록
 *
 * 파일 시스템에서 현재 메모리에 저장된 비트맵 데이터를
 * 지정된 디스크의 블록 위치로 기록한다.
 *
 * 비트맵의 변경 사항(블록 또는 아이노드 할당 상태 등)을 디스크에 반영하여,
 * 시스템이 재부팅되거나 파일 시스템이 다시 마운트될 때 정확한 상태를 유지하도록
 * 한다.
 *
 * @param fd 디바이스 파일 디스크립터 (기록할 대상 디스크 장치)
 * @param block_no 비트맵을 기록할 디스크 내의 블록 번호
 * @param map 디스크에 기록할 비트맵 데이터가 저장된 메모리 버퍼 포인터
 * @param map_size 비트맵 데이터의 크기(바이트 단위)
 *
 * @return 성공 시 0을 반환하며, 오류 발생 시 음수 값으로 오류 코드 반환:
 *         -EIO (I/O 오류), 또는 disk_write가 반환하는 기타 음수 값
 */
int bitmap_sync(int fd, uint32_t block_no, uint8_t *map, size_t map_size) {
  ssize_t ret;

  // 메모리(map)의 비트맵 데이터를 디스크의 지정된 위치에 기록한다.
  ret = disk_write(fd, map, map_size, (off_t)block_no * SFUSE_BLOCK_SIZE);
  if (ret < 0)
    return (int)ret; // disk_write 실패: 기록 자체가 실패함

  // 기록한 데이터 크기가 요청한 크기와 일치하는지 검증한다.
  if ((size_t)ret != map_size)
    return -EIO; // 기록한 데이터 크기가 예상 크기와 불일치 (부분적 기록 발생)

  return 0; // 정상적으로 비트맵 데이터를 디스크에 기록 완료
}

/**
 * @brief 사용 가능한 데이터 블록을 찾아 할당하고, 비트맵과 슈퍼블록 상태를 갱신
 *
 * 이 함수는 파일 시스템에서 데이터 저장을 위해 사용되지 않은 블록을 찾아
 * 할당한다. 블록의 사용 여부는 비트맵(block_map)에 비트 단위로 저장되어 있으며,
 * 이 비트맵을 순차적으로 탐색해 첫 번째 빈 블록을 찾아 할당한다.
 *
 * 블록을 할당하면 해당 블록의 비트를 '사용 중(1)'으로 설정하고,
 * 슈퍼블록의 'free_blocks' 필드 값을 감소시켜 남은 가용 블록 수를 갱신한다.
 *
 * @param sb 슈퍼블록 구조체 포인터 (파일시스템의 전체적인 상태를 관리)
 * @param block_map 블록 할당 여부를 나타내는 비트맵 버퍼 포인터
 *
 * @return 성공 시 할당된 블록의 오프셋(0부터 시작)을 반환하며,
 *         가용 블록이 없으면 공간 부족을 의미하는 -ENOSPC 반환
 */
int alloc_block(struct sfuse_super *sb, uint8_t *block_map) {
  // 데이터 블록 영역에서 사용 가능한 총 블록 개수를 계산
  uint32_t total = sb->blocks_count - sb->data_block_start;

  // 비트맵을 처음부터 끝까지 탐색하며 빈 블록을 찾음
  for (uint32_t i = 0; i < total; i++) {
    uint32_t byte_idx = i / 8; // 비트맵의 바이트 단위 인덱스
    uint32_t bit_idx = i % 8;  // 바이트 내의 비트 위치 (0~7)

    // 현재 블록(i)이 사용 가능한 상태인지 확인 (비트 값이 0이면 사용 가능)
    if (!(block_map[byte_idx] & (1 << bit_idx))) {
      // 사용 가능한 블록 발견 시 해당 비트를 '1'로 설정하여 사용 중으로 표시
      block_map[byte_idx] |= (1 << bit_idx);

      // 슈퍼블록에서 사용 가능한 블록 개수 1 감소
      sb->free_blocks--;

      // 할당된 블록의 오프셋 반환 (0부터 시작)
      return (int)i;
    }
  }

  // 모든 블록이 이미 사용 중이면 ENOSPC(공간 부족 오류) 반환
  return -ENOSPC;
}

/**
 * @brief 이전에 할당된 데이터 블록을 해제하고 비트맵과 슈퍼블록 상태를 갱신
 *
 * 이 함수는 파일 시스템에서 더 이상 사용하지 않는 데이터 블록을
 * 다시 사용 가능한 상태로 되돌리기 위해 해제(free)한다.
 *
 * 주어진 블록 오프셋(offset)의 위치를 비트맵에서 찾아서,
 * 해당 비트를 '0'으로 설정함으로써 빈 블록임을 표시하고,
 * 슈퍼블록의 'free_blocks' 값을 1 증가시켜 남은 가용 블록 수를 갱신한다.
 *
 * 이 함수는 파일 삭제 또는 파일 크기 축소 시 호출되어,
 * 디스크 공간을 효율적으로 재사용할 수 있도록 도와준다.
 *
 * @param sb 슈퍼블록 구조체 포인터 (파일 시스템의 전체 상태 관리)
 * @param block_map 블록 할당 여부를 나타내는 비트맵 버퍼 포인터
 * @param offset 해제할 블록의 오프셋(0부터 시작)
 */
void free_block(struct sfuse_super *sb, uint8_t *block_map, uint32_t offset) {
  uint32_t byte_idx = offset / 8; // 비트맵 내의 바이트 단위 인덱스 계산
  uint32_t bit_idx = offset % 8;  // 바이트 내에서의 비트 위치 계산 (0~7)

  // 비트맵에서 해당 블록의 비트를 '0'으로 설정 (빈 상태로 표시)
  block_map[byte_idx] &= ~(1 << bit_idx);

  // 슈퍼블록 내 빈 블록 개수를 1 증가 (가용 블록 수 갱신)
  sb->free_blocks++;
}

/**
 * @brief 빈 아이노드를 찾아 할당하고 비트맵과 슈퍼블록을 갱신
 *
 * 파일 시스템에서 새로운 파일이나 디렉토리를 생성할 때 사용할
 * 빈 아이노드를 탐색하여 할당하는 함수이다.
 *
 * 아이노드 번호 0은 특수 용도로 예약되어 있으므로, 할당 가능한
 * 아이노드의 탐색은 1번 아이노드부터 시작한다.
 *
 * 사용 가능한 아이노드를 찾으면 아이노드 비트맵에서 해당 비트를
 * 사용 중(1)으로 설정하고, 슈퍼블록의 'free_inodes' 값을 감소시켜
 * 파일 시스템의 상태를 업데이트한다.
 *
 * @param sb 슈퍼블록 구조체 포인터 (파일 시스템 상태 정보 관리)
 * @param inode_map 아이노드 할당 상태를 나타내는 비트맵 버퍼 포인터
 *
 * @return 성공 시 할당된 아이노드 번호(1 이상)를 반환하고,
 *         사용 가능한 아이노드가 없을 경우 -ENOSPC 반환
 */
int alloc_inode(struct sfuse_super *sb, uint8_t *inode_map) {
  // 아이노드 0은 예약이므로 1부터 탐색을 시작
  for (uint32_t i = 1; i < sb->inodes_count; i++) {
    uint32_t byte_idx = i / 8; // 비트맵 내의 바이트 인덱스 계산
    uint32_t bit_idx = i % 8;  // 바이트 내의 비트 위치 계산 (0~7)

    // 현재 아이노드(i)가 빈 상태인지 검사 (0이면 빈 상태)
    if (!(inode_map[byte_idx] & (1 << bit_idx))) {
      // 사용 가능한 아이노드를 발견하면, 비트맵에 사용 중으로 표시 (비트=1)
      inode_map[byte_idx] |= (1 << bit_idx);

      // 슈퍼블록의 남은 빈 아이노드 수를 1 감소시킴
      sb->free_inodes--;

      // 할당된 아이노드 번호를 반환 (번호는 1부터 시작)
      return (int)i;
    }
  }

  // 빈 아이노드를 찾지 못했을 때 공간 부족(-ENOSPC) 오류 반환
  return -ENOSPC;
}

/**
 * @brief 할당된 아이노드를 해제하고 비트맵과 슈퍼블록을 갱신
 *
 * 이 함수는 파일이나 디렉토리가 삭제될 때 호출되며,
 * 사용하던 아이노드를 해제하여 다시 사용 가능한 상태로 되돌린다.
 *
 * 주어진 아이노드 번호(ino)를 검사하여 유효한 범위인지 확인한 후,
 * 해당 아이노드의 비트를 아이노드 비트맵(inode_map)에서 0으로 설정하여
 * 빈 상태를 나타내고, 슈퍼블록의 'free_inodes'를 증가시켜 사용 가능한
 * 아이노드 수를 갱신한다.
 *
 * 아이노드 번호가 유효하지 않은 경우(0이거나 총 아이노드 수를 초과하는 경우),
 * 함수는 아무 작업도 수행하지 않고 즉시 종료된다.
 *
 * @param sb 슈퍼블록 구조체 포인터 (파일 시스템의 전역 상태 관리)
 * @param inode_map 아이노드 할당 상태를 나타내는 비트맵 버퍼 포인터
 * @param ino 해제할 아이노드 번호 (유효 범위: 1 ~ inodes_count-1)
 */
void free_inode(struct sfuse_super *sb, uint8_t *inode_map, uint32_t ino) {
  // 아이노드 번호 유효성 검사 (0번 아이노드 및 유효 범위 초과 시 무시)
  if (ino == 0 || ino >= sb->inodes_count)
    return; // 잘못된 아이노드 번호이므로 즉시 종료

  uint32_t byte_idx = ino / 8; // 비트맵의 바이트 인덱스 계산
  uint32_t bit_idx = ino % 8;  // 해당 바이트 내의 비트 위치 계산 (0~7)

  // 비트맵에서 해당 아이노드의 비트를 0으로 설정하여 빈 상태로 변경
  inode_map[byte_idx] &= ~(1 << bit_idx);

  // 슈퍼블록의 빈 아이노드 개수를 1 증가시킴
  sb->free_inodes++;
}
