/**
 * @file src/dir.c
 * @brief 디렉터리 엔트리 로드, 추가, 삭제 기능 구현
 *
 * 디렉터리의 데이터를 로드하거나 새 엔트리를 추가하고,
 * 기존의 엔트리를 삭제하는 기능을 구현한 파일이다.
 */

#include "dir.h"
#include "bitmap.h"
#include "block.h"
#include "inode.h"
#include "super.h"
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/**
 * @brief 지정된 디렉터리 아이노드의 모든 직접(direct) 데이터 블록을 메모리에
 * 읽어들인다.
 *
 * 주어진 아이노드 번호(ino)에 해당하는 디렉터리의 아이노드 정보를 먼저
 * 디스크에서 읽어온다. 이 아이노드에는 해당 디렉터리의 실제 데이터가 저장된
 * 데이터 블록의 번호들이 저장되어 있다.
 *
 * 이 함수는 직접(direct) 블록 포인터만을 처리하며, 각 블록이 0인 경우(미할당)는
 * 무시한다. 각 블록을 순서대로 디스크에서 읽어, 버퍼의 알맞은 위치에 복사하여
 * 최종적으로 메모리에 디렉터리 전체 데이터를 적재한다.
 *
 * @param fd   디바이스 파일 디스크립터 (데이터를 읽을 원본 디바이스를 나타냄)
 * @param sb   슈퍼블록 정보 구조체 포인터 (파일 시스템의 레이아웃 정보 제공)
 * @param ino  데이터를 읽어올 디렉터리의 아이노드 번호
 * @param buf  읽어온 디렉터리 데이터를 저장할 버퍼 (충분한 크기가 미리 할당되어
 * 있어야 함)
 *
 * @return 성공적으로 모든 블록을 읽었다면 0을 반환하고,
 *         inode_load 또는 read_block 수행 중 오류가 발생하면 음수의 오류 코드
 * 반환
 */
int dir_load(int fd, const struct sfuse_super *sb, uint32_t ino, void *buf) {
  // 디스크에서 주어진 아이노드 번호(ino)의 아이노드 메타데이터를 로드하여 inode
  // 변수에 저장
  struct sfuse_inode inode;
  int res = inode_load(fd, sb, ino, &inode);
  if (res < 0)
    return res; // 아이노드 로딩 실패 시 즉시 오류 코드 반환

  // 아이노드가 참조하는 모든 직접(direct) 블록들을 순회하며 디스크에서 읽는다
  for (uint32_t i = 0; i < SFUSE_NDIR_BLOCKS; i++) {
    // 현재 블록 번호가 0이면 미할당 상태이므로 건너뜀
    if (inode.direct[i] == 0)
      continue;

    // i번째 블록을 읽어 buf의 i번째 블록 위치로 이동한 메모리 주소에 저장
    // 각 블록 크기(SFUSE_BLOCK_SIZE)를 곱해 정확한 위치 계산
    res = read_block(fd, inode.direct[i], (char *)buf + i * SFUSE_BLOCK_SIZE);
    if (res < 0)
      return res; // 블록 읽기 중 오류 발생 시 즉시 오류 코드 반환
  }

  // 모든 직접 블록을 성공적으로 읽었으면 0 반환
  return 0;
}

/**
 * @brief 디렉터리에 새 파일 또는 디렉터리 엔트리를 추가한다.
 *
 * 지정된 디렉터리 아이노드가 관리하는 직접 블록에서 빈 슬롯을 찾아,
 * 새로운 파일 또는 디렉터리 엔트리를 추가한다.
 * 엔트리 추가 후 수정된 디렉터리 데이터를 디스크에 기록하고,
 * 파일시스템의 슈퍼블록 및 블록/아이노드 비트맵도 최신 상태로 동기화한다.
 *
 * @param fd        디바이스 파일 디스크립터 (디스크 접근용)
 * @param sb        슈퍼블록 구조체 포인터 (파일시스템의 정보 참조용)
 * @param ino       부모 디렉터리 아이노드 번호 (엔트리를 추가할 디렉터리)
 * @param name      추가할 새 엔트리의 이름
 * @param child_ino 새 엔트리에 연결될 아이노드 번호
 * @param block_map 블록 사용 정보를 나타내는 비트맵 버퍼
 * @param inode_map 아이노드 사용 정보를 나타내는 비트맵 버퍼
 * @param supermap  슈퍼블록 복사본 (변경 사항 동기화 시 사용)
 *
 * @return 성공 시 0 반환,
 *         빈 슬롯이 없어 추가가 불가능하면 -ENOSPC 반환,
 *         메모리 할당 실패 시 -ENOMEM,
 *         기타 입출력 오류 발생 시 음수 오류 코드 반환
 */
int dir_add_entry(int fd, const struct sfuse_super *sb, uint32_t ino,
                  const char *name, uint32_t child_ino, uint8_t *block_map,
                  uint8_t *inode_map, struct sfuse_super *supermap) {
  // 부모 디렉터리의 아이노드를 로드하여 inode 구조체에 저장한다.
  struct sfuse_inode inode;
  int res = inode_load(fd, sb, ino, &inode);
  if (res < 0)
    return res; // 아이노드 로딩에 실패하면 즉시 종료한다.

  // 디렉터리 데이터 전체를 저장할 메모리 버퍼를 할당한다.
  size_t buf_size = SFUSE_NDIR_BLOCKS * SFUSE_BLOCK_SIZE;
  void *buf = malloc(buf_size);
  if (!buf)
    return -ENOMEM; // 메모리 할당 실패 시 오류 코드 반환

  // 디스크로부터 디렉터리의 모든 직접 블록 데이터를 읽어온다.
  res = dir_load(fd, sb, ino, buf);
  if (res < 0) {
    free(buf);
    return res; // 블록 로드 실패 시 메모리 해제 후 종료한다.
  }

  // 버퍼를 디렉터리 엔트리 배열로 해석하여 최대 엔트리 개수 산정
  struct sfuse_dirent *ents = (struct sfuse_dirent *)buf;
  uint32_t max_entries = buf_size / sizeof(*ents);

  // 디렉터리 내의 빈 슬롯을 탐색하여 새 엔트리를 추가한다.
  for (uint32_t i = 0; i < max_entries; i++) {
    if (ents[i].ino == 0) {    // 빈 슬롯 발견 (아이노드 번호가 0이면 빈 슬롯)
      ents[i].ino = child_ino; // 새 아이노드 번호를 할당
      strncpy(ents[i].name, name, sizeof(ents[i].name) - 1); // 엔트리 이름 복사
      ents[i].name[sizeof(ents[i].name) - 1] = '\0'; // 문자열의 NULL 종단 보장

      // 수정된 전체 디렉터리 데이터를 디스크의 직접 블록에 다시 기록한다.
      for (uint32_t b = 0; b < SFUSE_NDIR_BLOCKS; b++) {
        if (inode.direct[b] == 0)
          continue; // 사용되지 않은 블록은 건너뛴다.

        res = write_block(fd, inode.direct[b],
                          (char *)buf + b * SFUSE_BLOCK_SIZE);
        if (res < 0) {
          free(buf);
          return res; // 블록 기록 실패 시 메모리 해제 후 종료한다.
        }
      }

      // 변경된 슈퍼블록과 블록 및 아이노드 비트맵을 디스크에 동기화한다.
      sb_sync(fd, supermap);
      bitmap_sync(fd, sb->block_bitmap_start, block_map, sb->blocks_count / 8);
      bitmap_sync(fd, sb->inode_bitmap_start, inode_map, sb->inodes_count / 8);

      free(buf); // 작업이 완료되었으므로 메모리 해제
      return 0;  // 성공적으로 엔트리 추가 완료
    }
  }

  // 디렉터리 내 모든 슬롯이 사용 중이어서 추가 불가 (공간 부족)
  free(buf);
  return -ENOSPC; // 추가 공간 부족 오류 반환
}

/**
 * @brief 디렉터리에서 특정 엔트리를 삭제한다.
 *
 * 이름이 일치하는 엔트리를 찾아 해당 슬롯을 비운 후 디스크에 데이터를 기록한다.
 *
 * @param fd   디바이스 파일 디스크립터
 * @param sb   슈퍼블록 정보 구조체 포인터
 * @param ino  부모 디렉터리 아이노드 번호
 * @param name 삭제할 엔트리의 이름
 * @return 성공 시 0, 엔트리가 존재하지 않으면 -ENOENT, 기타 오류 발생 시 음수
 * 오류 코드
 */
int dir_remove_entry(int fd, const struct sfuse_super *sb, uint32_t ino,
                     const char *name) {
  // 디렉터리 아이노드 로드
  struct sfuse_inode inode;
  int res = inode_load(fd, sb, ino, &inode);
  if (res < 0)
    return res;

  // 디렉터리 데이터 블록 메모리에 로드
  size_t buf_size = SFUSE_NDIR_BLOCKS * SFUSE_BLOCK_SIZE;
  void *buf = malloc(buf_size);
  if (!buf)
    return -ENOMEM;

  res = dir_load(fd, sb, ino, buf);
  if (res < 0) {
    free(buf);
    return res;
  }

  struct sfuse_dirent *ents = (struct sfuse_dirent *)buf;
  uint32_t max_entries = buf_size / sizeof(*ents);

  // 이름이 일치하는 엔트리 탐색 및 삭제
  for (uint32_t i = 0; i < max_entries; i++) {
    if (ents[i].ino != 0 && strcmp(ents[i].name, name) == 0) {
      ents[i].ino = 0;
      ents[i].name[0] = '\0';

      // 수정된 디렉터리 데이터를 디스크에 기록
      for (uint32_t b = 0; b < SFUSE_NDIR_BLOCKS; b++) {
        if (inode.direct[b] == 0)
          continue;

        res = write_block(fd, inode.direct[b],
                          (char *)buf + b * SFUSE_BLOCK_SIZE);
        if (res < 0) {
          free(buf);
          return res;
        }
      }

      free(buf);
      return 0;
    }
  }

  // 해당 이름의 엔트리가 없을 경우 메모리 해제 후 오류 반환
  free(buf);
  return -ENOENT;
}
