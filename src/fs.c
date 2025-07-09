/**
 * @file src/fs.c
 * @brief SFUSE 파일시스템 초기화, 관리 및 경로 분석 함수의 구현
 *
 * 본 파일은 SFUSE 파일시스템을 관리하는 핵심 모듈로서, 슈퍼블록 초기화, inode
 * 관리, 블록 관리, 경로 분석 및 해석 기능 등을 포함하며, 실제 블록 디바이스와의
 * 동기화 작업을 수행하는 함수들을 구현한다. 파일시스템 생성 및 마운트 시 주요
 * 데이터를 초기화하거나 기존 데이터를 불러와 사용자의 요청에 따라 디스크 상의
 * 메타데이터를 관리한다.
 */

#include "fs.h"
#include "bitmap.h"
#include "block.h"
#include "dir.h"
#include "inode.h"
#include "super.h"
#include <errno.h>
#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/**
 * @brief 현재 FUSE 컨텍스트에서 사용 중인 SFUSE 파일 시스템 컨텍스트를
 * 반환한다.
 *
 * 본 함수는 FUSE 라이브러리에서 제공하는 fuse_get_context()를 통해 얻은
 * private_data 필드를 SFUSE 파일 시스템 컨텍스트 구조체 포인터로 반환한다.
 *
 * SFUSE 파일 시스템 컨텍스트(struct sfuse_fs)는 파일 시스템의 상태 정보와
 * 메타데이터를 포함한다.
 *
 * 추가 설명:
 * - 컨텍스트(context):   파일 시스템 연산 처리 시 필요한 사용자 및 상태 정보를
 *                        저장하는 구조체로, 요청자(UID, GID, PID)의 정보와 같은
 *                        메타데이터를 포함한다.
 * - fuse_get_context():  현재 처리 중인 파일 시스템 연산의 컨텍스트를 반환하는
 *                        FUSE 제공 함수.
 * - private_data:        파일 시스템 개발자가 정의한 데이터를 저장하는
 *                        포인터로, SFUSE에서는 파일 시스템의 슈퍼블록, 비트맵,
 *                        블록 정보 등 상태 데이터와 메타데이터 접근을 위해
 *                        사용한다.
 *
 * @return struct sfuse_fs* 현재 파일 시스템의 컨텍스트 포인터
 *
 * @see fuse_get_context()
 * @see struct sfuse_fs
 */
struct sfuse_fs *get_fs_context(void) {
  return (struct sfuse_fs *)fuse_get_context()->private_data;
}

/**
 * @brief SFUSE 파일 시스템을 초기화하고 슈퍼블록 및 메타데이터를 준비한다.
 *
 * 이 함수는 주어진 블록 장치 파일 디스크립터를 이용하여 SFUSE 파일 시스템을
 * 초기화한다. 먼저 장치의 크기를 계산하고, 슈퍼블록이 존재하지 않으면 파일
 * 시스템을 포맷하여 새로 생성하며, 슈퍼블록 및 루트 디렉터리를 초기화한다. 만약
 * 슈퍼블록이 이미 존재한다면 이를 로드하고 기존 비트맵 데이터를 불러온다.
 *
 * @param backing_fd 파일 시스템을 저장하는 장치의 파일 디스크립터.
 * @return 성공 시 0, 실패 시 음수의 에러 코드.
 */
/**
 * @brief SFUSE 파일 시스템을 초기화하고 슈퍼블록 및 메타데이터를 준비한다.
 *
 * 파일 시스템의 슈퍼블록, inode 테이블, 비트맵 등을 초기화하거나 기존 데이터를
 * 로드하여 파일 시스템 사용 준비를 완료한다.
 *
 * @param backing_fd 파일 시스템을 저장하는 블록 장치 파일 디스크립터.
 * @return 성공 시 0, 실패 시 음수의 에러 코드.
 */
int fs_initialize(int backing_fd) {
  // 현재 사용 중인 파일 시스템 컨텍스트 획득
  struct sfuse_fs *fs = get_fs_context();

  // 얻은 블록 장치 파일 디스크립터 저장
  fs->backing_fd = backing_fd;

  /*
  블록 장치(backing_fd)의 크기를 바이트 단위로 얻는다.
  파일의 크기를 얻으려면 파일의 가장 끝으로 파일 오프셋(offset)을 이동시킨다.
  이때 사용되는 함수가 바로 `lseek`이다.

  lseek 함수 원형:
  off_t lseek(int fd, off_t offset, int whence);

  인자 설명:
  - fd: 오프셋을 이동시킬 파일 디스크립터.
  - offset: 이동할 오프셋의 크기.
  - whence: 오프셋 이동 기준을 설정하는 플래그.
     - SEEK_SET: 파일의 시작 지점으로부터 offset 바이트 이동.
     - SEEK_CUR: 현재 위치에서 offset 바이트 이동.
     - SEEK_END: 파일 끝에서부터 offset 바이트 이동.

  여기서는 파일의 크기를 알고자 하므로,
  SEEK_END를 이용해 파일의 끝으로 이동(offset은 0)을 지정한다.
  그러면 lseek는 파일의 총 크기(바이트 단위)를 반환한다.
 */
  off_t device_size = lseek(backing_fd, 0, SEEK_END);

  /*
  장치 크기(device_size)는 바이트 단위이기 때문에,
  이를 SFUSE 파일 시스템이 사용하는 블록 크기(SFUSE_BLOCK_SIZE)로 나누어
  블록 장치가 가진 전체 블록의 수를 계산한다.

  예시로, 만약 장치 크기가 8192바이트이고 블록 크기가 1024바이트라면,
  총 블록 수(total_blocks)는 8192 / 1024 = 8이 된다.

  total_blocks 값은 파일 시스템의 메타데이터와 데이터 블록을 배치할 때
  사용된다.
  */
  uint32_t total_blocks = device_size / SFUSE_BLOCK_SIZE;

  // 슈퍼블록을 디스크에서 로드(load) 시도.
  // sb_load()가 성공하면 이미 파일 시스템이 포맷되어 있다는 뜻이다.
  // sb_load()는 디스크에서 슈퍼블록 데이터를 읽어 fs->sb 구조체에 저장한다.
  // 성공 시 0을 반환하며, 실패 시 음수를 반환한다.
  //
  // sb_load()가 실패하면 (즉, 음수 반환 시),
  // 이는 슈퍼블록이 존재하지 않거나 손상된 상태를 나타내므로,
  // 새로운 슈퍼블록을 포맷하고 초기화하는 과정을 수행해야 한다.
  if (sb_load(backing_fd, &fs->sb) < 0) {

    // 슈퍼블록 포맷: 슈퍼블록을 깨끗한 초기 상태로 설정.
    // 이 함수는 슈퍼블록의 모든 값을 기본값 또는 초기 상태로 설정한다.
    sb_format(&fs->sb);

    // 슈퍼블록에 파일 시스템의 전체 블록 개수(total_blocks)를 설정한다.
    // 이 값은 파일 시스템이 관리할 수 있는 블록의 최대 개수를 나타낸다.
    fs->sb.blocks_count = total_blocks;

    // inode 개수 설정: 파일 시스템이 관리할 수 있는 inode의 총 개수를 설정.
    // inode는 파일이나 디렉터리 같은 객체의 메타데이터를 저장하는 단위이다.
    fs->sb.inodes_count = SFUSE_INODES_COUNT;

    // --- 메타데이터 영역의 크기 계산 시작 ---

    // 1. 블록 비트맵(block bitmap)의 크기를 블록 단위로 계산
    //
    // 블록 비트맵은 각 블록이 사용 중인지 여부를 비트 단위로 관리한다.
    // 하나의 블록을 1비트(bit)로 관리하므로,
    // 전체 블록 수를 8로 나누면 필요한 바이트(byte) 수가 된다.
    //
    // 이 바이트 수를 블록 단위(SFUSE_BLOCK_SIZE)로 변환하기 위해 다음 수식을
    // 사용:
    //
    // (fs->sb.blocks_count / 8 + SFUSE_BLOCK_SIZE - 1) / SFUSE_BLOCK_SIZE
    //
    // 이 수식은 '올림(ceil)'의 효과를 내는 표현으로,
    // 정확히 나누어 떨어지지 않을 경우 필요한 블록을 추가로 확보하기 위한
    // 계산법이다.
    uint32_t block_bitmap_blocks =
        (fs->sb.blocks_count / 8 + SFUSE_BLOCK_SIZE - 1) / SFUSE_BLOCK_SIZE;

    // 2. inode 비트맵(inode bitmap)의 크기를 블록 단위로 계산
    //
    // inode 비트맵은 inode들이 사용 중인지 여부를 관리하는 비트맵이다.
    // 전체 inode 개수를 8로 나누면 바이트 수가 되며,
    // 이것을 블록 단위로 변환하는 계산법은 블록 비트맵 계산법과 동일하다.
    uint32_t inode_bitmap_blocks =
        (fs->sb.inodes_count / 8 + SFUSE_BLOCK_SIZE - 1) / SFUSE_BLOCK_SIZE;

    // 3. inode 테이블(inode table)의 크기를 블록 단위로 계산
    //
    // inode 테이블은 실제 inode 구조체들을 저장하는 영역이다.
    // 각 inode는 'struct sfuse_inode'의 크기만큼 메모리를 차지하므로,
    // 전체 inode 개수에 inode 크기를 곱하면 inode 테이블의 총 크기가 바이트
    // 단위로 나온다.
    //
    // 이것을 다시 블록 크기(SFUSE_BLOCK_SIZE)로 나누어 몇 개의 블록이 필요한지
    // 계산한다. 여기서도 블록이 정수로 딱 떨어지지 않을 수 있으므로, 올림
    // 효과를 내도록 처리한다.
    uint32_t inode_table_blocks =
        (fs->sb.inodes_count * sizeof(struct sfuse_inode) + SFUSE_BLOCK_SIZE -
         1) /
        SFUSE_BLOCK_SIZE;

    // 슈퍼블록에서 관리하는 각 영역의 위치(블록 번호)를 설정한다.
    // SFUSE 파일 시스템의 디스크 레이아웃은 일반적으로 다음과 같다:
    //
    //    파일 시스템 블록 구조:
    // +--------------------------+
    // | 블록 0 (슈퍼블록)        |
    // +--------------------------+
    // | 블록 1 (예비 블록)       |
    // +--------------------------+
    // | 블록 2 ~ N               |
    // | (block bitmap)           |
    // | [블록 사용 여부 표시]    |
    // +--------------------------+
    // | inode bitmap             |
    // | [inode 사용 여부 표시]   |
    // +--------------------------+
    // | inode tables             |
    // | [파일 메타데이터 저장]   |
    // +--------------------------+
    // | data block               |
    // | [실제 사용자 데이터 저장]|
    // +--------------------------+
    //
    // block 비트맵의 시작 위치는 슈퍼블록(블록 0)과 예비 블록(블록 1) 뒤에 위치
    fs->sb.block_bitmap_start = 2;

    // inode 비트맵은 block 비트맵 다음에 위치
    fs->sb.inode_bitmap_start = fs->sb.block_bitmap_start + block_bitmap_blocks;

    // inode 테이블은 inode 비트맵 다음에 위치
    fs->sb.inode_table_start = fs->sb.inode_bitmap_start + inode_bitmap_blocks;

    // 실제 사용자 데이터 블록은 inode 테이블 다음에 위치
    fs->sb.data_block_start = fs->sb.inode_table_start + inode_table_blocks;

    // 데이터 블록 시작 위치 이후의 블록만 실제 데이터 저장을 위해 사용
    // 가능하므로, 전체 블록 수(total_blocks)에서 데이터 블록 시작 위치를 빼면
    // 여유 블록 수를 구할 수 있다.
    fs->sb.free_blocks = total_blocks - fs->sb.data_block_start;

    // inode 중 루트 디렉터리를 제외한 나머지 inode들이 사용 가능
    fs->sb.free_inodes = fs->sb.inodes_count - 1;

    // 위에서 설정한 슈퍼블록 데이터를 디스크에 즉시 동기화하여 저장
    if (sb_sync(backing_fd, &fs->sb) < 0)
      return -EIO; // 동기화 실패 시 입출력 에러 반환

    // ---- 메모리에 블록 비트맵과 inode 비트맵을 관리할 공간을 확보 ----

    // 비트맵 크기 계산:
    // 비트맵은 비트 단위로 블록이나 inode의 사용 여부를 관리한다.
    // 1 바이트(byte)는 8 비트(bit)를 가지므로, 블록과 inode의 개수를 각각 8로
    // 나누면 비트맵이 필요한 메모리 크기가 바이트 단위로 나온다.
    size_t bmap_bytes = fs->sb.blocks_count / 8;
    size_t imap_bytes = fs->sb.inodes_count / 8;

    // 위에서 계산한 크기만큼 메모리를 할당하고, 그 공간을 0으로 초기화한다.
    // calloc(1, size)는 size 크기만큼 메모리를 할당한 후 모든 비트를 0으로
    // 설정한다. 이렇게 하면 비트맵이 초기 상태에서는 모든 블록과 inode가 "사용
    // 가능"(free)으로 표시된다.
    fs->block_map = calloc(1, bmap_bytes);
    fs->inode_map = calloc(1, imap_bytes);

    // 메모리 할당 실패 시 메모리 부족 에러 반환
    if (!fs->block_map || !fs->inode_map)
      return -ENOMEM;

    // 비트맵의 초기 상태(모든 블록 및 inode가 사용 가능 상태)를 디스크에 동기화
    bitmap_sync(backing_fd, fs->sb.block_bitmap_start, fs->block_map,
                bmap_bytes);
    bitmap_sync(backing_fd, fs->sb.inode_bitmap_start, fs->inode_map,
                imap_bytes);

    // ---- 루트 디렉터리 inode 초기화 ----

    // 루트 inode 번호 설정 (1번으로 정의됨)
    uint32_t root = SFUSE_ROOT_INO;

    // inode 비트맵에서 루트 inode를 사용 중으로 표시하는 코드
    // 비트맵(bitmap)은 각 inode의 사용 여부를 관리하는 데이터 구조로,
    // 1비트가 1개의 inode의 상태를 나타낸다.
    // 비트값이 0이면 해당 inode는 사용 가능(free), 1이면 사용
    // 중(allocated)이다.

    // inode 비트맵은 바이트(byte) 배열 형태로 관리된다.
    // 배열의 각 요소(바이트)마다 8개의 inode 상태를 표현할 수 있다.
    // 특정 inode가 속한 바이트의 위치는 inode 번호를 8로 나눈 값(root / 8)이며,
    // 바이트 내의 정확한 비트 위치는 inode 번호를 8로 나눈 나머지(root % 8)가
    // 된다.

    // 예시:
    // 루트 inode(root)가 5라면, inode_map[0] (바이트의 첫 번째 요소)의 5번째
    // 비트를 설정한다.

    // 이 코드의 동작 방식:
    // 1. (1 << (root % 8)): 1을 root % 8 만큼 왼쪽으로 시프트(shift)하여 원하는
    // 비트 위치로 설정한다.
    // 2. inode_map[root / 8] |= (...): OR 연산으로 해당 비트를 설정하여 inode를
    // '사용 중'으로 표시한다.
    //
    // 비트 연산을 사용하는 이유는 매우 빠르고 공간을 절약하기 때문이다.
    fs->inode_map[root / 8] |= (1 << (root % 8));

    // 위에서 inode 비트맵에 루트 inode를 할당했기 때문에,
    // 슈퍼블록에서 관리하는 사용 가능한(free) inode의 개수도 하나 감소시켜야
    // 한다.
    fs->sb.free_inodes--;

    // 루트 inode 구조체 초기화 (초기화 전에 메모리 전체를 0으로 설정)
    struct sfuse_inode root_inode;
    memset(&root_inode, 0, sizeof(root_inode));

    // 루트 inode 메타데이터 설정 (디렉터리, 접근 권한 0755, 소유자 UID/GID
    // 설정)
    fs_init_inode(&fs->sb, root, S_IFDIR | 0755, fuse_get_context()->uid,
                  fuse_get_context()->gid, &root_inode);

    // ---- 루트 디렉터리의 데이터 블록 할당 및 초기화 ----

    // 블록 할당 함수를 호출하여, 첫 번째 데이터 블록 인덱스를 할당 받음
    int blk_index = alloc_block(&fs->sb, fs->block_map);
    if (blk_index < 0)
      return -ENOSPC; // 사용 가능한 블록 공간 부족 시 에러 반환

    // 실제 블록 번호는 데이터 블록의 시작 위치에서 blk_index만큼 더한 값
    uint32_t phys_block = fs->sb.data_block_start + blk_index;

    // inode가 관리하는 직접(direct) 블록 포인터 중 첫 번째 항목을 설정
    root_inode.direct[0] = phys_block;
    root_inode.size =
        SFUSE_BLOCK_SIZE; // 루트 디렉터리 크기를 블록 크기만큼 설정

    // 데이터 블록 내에 루트 디렉터리의 기본 엔트리("." 및 "..")를 생성하기 위한
    // 초기화
    uint8_t block[SFUSE_BLOCK_SIZE] = {0}; // 블록 크기만큼 0으로 초기화
    struct sfuse_dirent *entries =
        (struct sfuse_dirent *)block; // 디렉터리 엔트리 구조체로 사용

    // "."(현재 디렉터리)는 루트 inode를 가리킴
    entries[0].ino = root;
    strcpy(entries[0].name, ".");

    // ".."(상위 디렉터리) 역시 루트 inode를 가리킴 (루트 디렉터리는 자신을
    // 가리킴)
    entries[1].ino = root;
    strcpy(entries[1].name, "..");

    // 구성된 디렉터리 데이터를 디스크의 실제 블록에 기록
    if (write_block(backing_fd, phys_block, block) < 0)
      return -EIO; // 블록 기록 실패 시 입출력 에러 반환

    // 구성된 루트 inode를 디스크에 저장하여 동기화
    if (inode_sync(backing_fd, &fs->sb, root, &root_inode) < 0)
      return -EIO; // inode 동기화 실패 시 입출력 에러 반환

    // ---- 모든 메타데이터의 최종 상태를 디스크에 동기화 ----

    // 블록 비트맵과 inode 비트맵의 최종 상태를 디스크에 저장
    bitmap_sync(backing_fd, fs->sb.block_bitmap_start, fs->block_map,
                bmap_bytes);
    bitmap_sync(backing_fd, fs->sb.inode_bitmap_start, fs->inode_map,
                imap_bytes);

    // 슈퍼블록의 최종 상태를 디스크에 저장하여 파일 시스템 초기화 완료
    sb_sync(backing_fd, &fs->sb);

  } else {
    // 슈퍼블록이 이미 존재(정상 로드 성공) 시, 기존 메타데이터를 디스크에서
    // 로드

    size_t bmap_bytes = fs->sb.blocks_count / 8;
    size_t imap_bytes = fs->sb.inodes_count / 8;

    fs->block_map = calloc(1, bmap_bytes);
    fs->inode_map = calloc(1, imap_bytes);

    // 기존 비트맵 데이터를 디스크에서 메모리로 로드하여 파일 시스템 재구성
    bitmap_load(backing_fd, fs->sb.block_bitmap_start, fs->block_map,
                bmap_bytes);
    bitmap_load(backing_fd, fs->sb.inode_bitmap_start, fs->inode_map,
                imap_bytes);
  }

  // 초기화 과정이 모두 정상적으로 완료되었으므로 성공(0)을 반환
  return 0;
}

/**
 * @brief SFUSE 파일 시스템을 종료하고 리소스를 정리하는 함수.
 *
 * 파일 시스템 종료 시 호출되며, 메모리에 존재하는 파일 시스템의
 * 메타데이터(슈퍼블록, 블록 및 inode 비트맵)를 디스크에 동기화하고,
 * 할당된 메모리 리소스를 해제한다.
 *
 * @param private_data 종료될 파일 시스템 컨텍스트(struct sfuse_fs)의 포인터.
 */
void fs_destroy(void *private_data) {
  // 전달받은 private_data 포인터를 struct sfuse_fs 타입으로 변환
  struct sfuse_fs *fs = private_data;

  // 비트맵 데이터를 디스크에 동기화하기 위한 메모리 크기 계산
  // 블록 비트맵 크기: 전체 블록 수(blocks_count)를 비트맵 표현을 위해 바이트로
  // 환산
  size_t bmap_bytes = fs->sb.blocks_count / 8;

  // inode 비트맵 크기: 전체 inode 수(inodes_count)를 비트맵 표현을 위해
  // 바이트로 환산
  size_t imap_bytes = fs->sb.inodes_count / 8;

  // 메모리에 존재하는 블록 비트맵 상태를 디스크에 동기화한다.
  // 이 작업을 통해 파일 시스템 종료 시 블록의 사용 상태가 디스크에 정확히
  // 반영된다.
  bitmap_sync(fs->backing_fd, fs->sb.block_bitmap_start, fs->block_map,
              bmap_bytes);

  // 메모리에 존재하는 inode 비트맵 상태를 디스크에 동기화한다.
  // 파일 시스템 종료 시 inode의 사용 여부를 디스크에 저장한다.
  bitmap_sync(fs->backing_fd, fs->sb.inode_bitmap_start, fs->inode_map,
              imap_bytes);

  // 슈퍼블록 상태를 디스크에 동기화하여 최신의 파일 시스템 메타데이터를
  // 유지한다.
  sb_sync(fs->backing_fd, &fs->sb);

  // 파일 시스템의 종료 작업 이후 메모리에 할당된 비트맵 메모리를 해제하여,
  // 메모리 누수를 방지한다.
  free(fs->block_map); // 블록 비트맵 메모리 해제
  free(fs->inode_map); // inode 비트맵 메모리 해제
}

/**
 * @brief 주어진 파일 또는 디렉터리 경로의 inode 번호를 찾는다.
 *
 * 이 함수는 SFUSE 파일 시스템 내에서 주어진 경로를 순차적으로 분석하여
 * 해당 경로에 대응하는 inode 번호를 찾고 결과를 반환한다.
 *
 * 예를 들어 경로 "/home/user/file.txt"가 주어지면,
 * "/" → "home" → "user" → "file.txt" 순서로 디렉터리를 순차적으로 탐색하여
 * 최종적으로 "file.txt"의 inode 번호를 찾는다.
 *
 * @param fs 파일 시스템 컨텍스트(struct sfuse_fs)의 포인터.
 * @param path inode 번호를 찾을 경로 문자열.
 * @param ino_out 찾은 inode 번호를 저장할 포인터.
 * @return 성공 시 0, 실패 시 음수의 에러 코드 반환.
 */
int fs_resolve_path(struct sfuse_fs *fs, const char *path, uint32_t *ino_out) {
  // 시작 지점을 루트 디렉터리의 inode로 설정
  uint32_t cur = SFUSE_ROOT_INO;

  // 예외 처리: 빈 문자열 또는 루트 디렉터리('/')가 들어오면 바로 루트 inode
  // 반환
  if (!path || path[0] == '\0' || strcmp(path, "/") == 0) {
    *ino_out = cur;
    return 0;
  }

  // 경로 문자열을 수정 가능하도록 복제(메모리 복사)
  char *dup = strdup(path);
  if (!dup)
    return -ENOMEM; // 메모리 부족 시 에러 반환

  // 경로 문자열을 "/" 기준으로 토큰(token)으로 나눈다.
  // 예시: "/home/user/file" → ["home", "user", "file"]
  char *token = strtok(dup, "/");

  // 모든 토큰에 대해 순차적으로 디렉터리를 탐색
  while (token) {
    // 디렉터리 데이터를 메모리에 로드하기 위한 버퍼 크기를 계산한다.
    // 해당 버퍼는 디렉터리 엔트리 정보를 저장하는 데 사용된다.
    // SFUSE_NDIR_BLOCKS: 디렉터리 inode가 직접 참조할 수 있는 블록의 최대 개수.
    //                    직접 참조 블록 포인터 수 (12개)
    // SFUSE_BLOCK_SIZE:  각 블록의 크기(바이트 단위)()
    //
    // 따라서 디렉터리 데이터를 로드할 수 있는 전체 버퍼 크기는 다음과 같다:
    //
    //   buf_size = (직접 참조 블록 개수) × (각 블록 크기)
    //
    // 수식으로 표현하면:
    //   buf_size = SFUSE_NDIR_BLOCKS * SFUSE_BLOCK_SIZE;
    //
    // 예시:
    //   만약 SFUSE_NDIR_BLOCKS가 12이고 SFUSE_BLOCK_SIZE가 1024 바이트라면,
    //   buf_size = 12 × 1024 = 12288 바이트(12KB)가 된다.
    size_t buf_size = SFUSE_NDIR_BLOCKS * SFUSE_BLOCK_SIZE;
    void *buf = malloc(buf_size);
    if (!buf) {
      free(dup);
      return -ENOMEM; // 메모리 부족 시 에러 반환
    }

    // 현재 디렉터리의 모든 엔트리(entry)를 디스크에서 메모리로 로드한다.
    // dir_load는 cur(현재 inode 번호)에 해당하는 디렉터리 데이터를 버퍼에
    // 채운다.
    if (dir_load(fs->backing_fd, &fs->sb, cur, buf) < 0) {
      free(buf);
      free(dup);
      return -ENOENT; // 디렉터리를 읽을 수 없는 경우 존재하지 않음 에러 반환
    }

    // 버퍼의 데이터를 디렉터리 엔트리 배열로 캐스팅하여 접근 가능하도록 설정
    struct sfuse_dirent *ents = (struct sfuse_dirent *)buf;

    // 디렉터리 엔트리의 최대 개수를 계산
    size_t max_entries = buf_size / sizeof(*ents);

    // 다음 디렉터리 또는 파일의 inode 번호를 찾기 위한 임시 변수
    uint32_t next_ino = 0;

    // 현재 디렉터리 내의 엔트리 중에서 이름이 토큰과 일치하는 엔트리를 찾는다.
    for (size_t i = 0; i < max_entries; i++) {
      // ents[i].ino가 0이면 빈 엔트리이므로 무시
      // 토큰과 같은 이름의 엔트리를 찾으면 next_ino에 inode 번호 저장 후 반복문
      // 종료
      if (ents[i].ino != 0 && strcmp(ents[i].name, token) == 0) {
        next_ino = ents[i].ino;
        break;
      }
    }

    // 버퍼 사용이 끝났으므로 메모리 해제
    free(buf);

    // 해당하는 이름을 가진 엔트리를 찾지 못한 경우, 경로가 존재하지 않음을 의미
    if (next_ino == 0) {
      free(dup);
      return -ENOENT; // 경로 존재하지 않음 에러 반환
    }

    // 다음 토큰 탐색을 위해 현재 inode 번호를 방금 찾은 inode 번호로 업데이트
    cur = next_ino;

    // 다음 토큰으로 넘어감
    token = strtok(NULL, "/");
  }

  // 모든 토큰 탐색이 완료되면, 최종 inode 번호를 결과로 반환
  free(dup);      // 경로 문자열 복제본의 메모리 해제
  *ino_out = cur; // 최종 inode 번호 저장
  return 0;       // 성공적으로 경로를 해석함
}

/**
 * @brief 전체 경로(fullpath)를 부모 경로와 파일명(또는 디렉터리명)으로
 * 분리한다.
 *
 * 주어진 전체 경로에서 마지막 경로 구성 요소를 파일 또는 디렉터리 이름으로
 * 분리하고, 나머지 앞부분 경로는 부모 디렉터리로 간주하여 해당하는 부모
 * 디렉터리의 inode 번호를 찾는다.
 *
 * 예시:
 *   "/home/user/file.txt" → parent_ino: "/home/user"의 inode 번호
 *                           반환값: "file.txt"
 *
 *   "/file.txt" → parent_ino: 루트 디렉터리의 inode 번호 ("/")
 *                 반환값: "file.txt"
 *
 * @param fullpath 전체 파일 또는 디렉터리 경로 문자열
 * @param parent_ino 부모 디렉터리의 inode 번호가 저장될 포인터
 *
 * @return 성공 시 파일명 또는 디렉터리명 문자열 포인터 반환 (반드시 호출한
 * 곳에서 free 해야 함) 실패 시 NULL 반환
 */
char *fs_split_path(const char *fullpath, uint32_t *parent_ino) {
  char *dup = strdup(fullpath); // 경로를 수정하기 위해 복제본 생성
  if (!dup)
    return NULL; // 메모리 부족 시 실패 처리

  char *slash = strrchr(dup, '/'); // 마지막 '/' 문자를 찾아 경로 분리 지점 설정
  char *name;

  if (!slash) {
    // '/' 문자가 없을 경우 (예: "file.txt")
    *parent_ino = SFUSE_ROOT_INO; // 부모는 루트 디렉터리로 설정
    name = dup;                   // 전체 경로가 곧 파일명
  } else if (slash == dup) {
    // 경로가 루트에 바로 붙은 경우 (예: "/file.txt")
    *parent_ino = SFUSE_ROOT_INO; // 부모는 루트 디렉터리
    name = strdup(slash + 1);     // '/' 다음부터 이름 복사 ("file.txt")
    free(dup);                    // 최초 복제한 문자열은 더 이상 필요 없음
  } else {
    // 그 외 일반적인 경우 (예: "/home/user/file.txt")
    *slash =
        '\0'; // '/' 위치에서 문자열을 종료시켜 부모 경로만 남김 ("/home/user")

    // 남은 부모 경로(dup)를 이용하여 부모 디렉터리의 inode 번호를 찾는다.
    if (fs_resolve_path(get_fs_context(), dup, parent_ino) < 0) {
      free(dup);   // 부모 경로 탐색 실패 시 메모리 해제
      return NULL; // 실패 반환
    }

    // '/' 뒤의 문자열("file.txt")을 이름으로 복사하여 반환
    name = strdup(slash + 1);
    free(dup); // 부모 경로 탐색 완료 후 메모리 해제
  }

  return name; // 호출한 곳에서 반드시 free(name) 수행 필요
}
