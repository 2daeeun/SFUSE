/**
 * @file src/inode.c
 * @brief 아이노드 로드, 동기화 및 블록 주소 변환 함수 구현
 *
 * 이 파일은 SFUSE 파일 시스템에서 아이노드를 디스크로부터 읽거나 기록하는 함수,
 * 아이노드를 초기화하는 함수, 논리적 블록 번호를 물리적 블록 번호로 변환하는
 * 함수의 구현을 제공한다.
 */

#include "inode.h"
#include "block.h" ///< 블록 읽기/쓰기 (read_block/write_block)
#include "disk.h"  ///< 디스크 읽기/쓰기 함수 (disk_read/disk_write)
#include "super.h"
#include <errno.h>
#include <string.h>
#include <sys/stat.h> ///< 파일 타입 매크로 (S_ISDIR)
#include <time.h>
#include <unistd.h>

/**
 * @brief 새로운 아이노드를 기본 값으로 초기화한다.
 *
 * 이 함수는 파일 시스템 내에서 새로운 아이노드를 생성할 때 호출된다.
 * 아이노드는 파일 또는 디렉터리와 같은 객체의 메타데이터를 관리하는 핵심적인
 * 구조체로, 이 함수는 아이노드를 사용 가능한 기본값으로 설정하여 초기 상태를
 * 정의한다.
 *
 * 세부적으로는 다음의 작업을 수행한다:
 *   - 아이노드 메모리 영역을 모두 0으로 초기화한다.
 *   - 파일의 접근 권한(mode), 소유자(uid), 그룹(gid)을 설정한다.
 *   - 파일 크기를 0으로 설정한다 (새로 생성된 파일이므로 초기 크기는 0이다).
 *   - 접근(atime), 수정(mtime), 상태 변경(ctime) 시간을 현재 시각으로 설정한다.
 *   - 모든 Direct 블록 포인터를 0으로 초기화하여, 아직 데이터를 가리키지 않도록
 *     설정한다.
 *   - Single indirect, Double indirect 블록 포인터 역시 0으로 설정하여,
 *     추가적인 데이터 블록 참조가 없음을 나타낸다.
 *   - 링크 수(links)를 초기화한다. 만약 디렉터리라면 기본적으로 2개(자기 자신
 *     '.'과 상위 디렉터리 '..'), 파일이라면 1개로 설정한다.
 *
 * @param sb     슈퍼블록 정보 포인터 (미사용 파라미터)
 * @param ino    초기화할 아이노드 번호 (미사용 파라미터)
 * @param mode   새 아이노드의 파일 타입(파일/디렉터리 등)과 접근 권한
 * @param uid    새 아이노드를 소유할 사용자의 사용자 ID
 * @param gid    새 아이노드를 소유할 사용자의 그룹 ID
 * @param inode  실제로 초기화할 아이노드 구조체 포인터
 */
void fs_init_inode(const struct sfuse_super *sb, uint32_t ino, mode_t mode,
                   uid_t uid, gid_t gid, struct sfuse_inode *inode) {
  (void)sb;  // 미사용 파라미터로 인한 컴파일러 경고 방지
  (void)ino; // 미사용 파라미터로 인한 컴파일러 경고 방지

  // inode 메모리를 0으로 초기화하여 깨끗한 상태로 시작
  memset(inode, 0, sizeof(*inode));

  // 파일 타입과 접근 권한(mode)을 설정
  inode->mode = mode;

  // 아이노드 소유자의 사용자(uid)와 그룹(gid) ID를 설정
  inode->uid = uid;
  inode->gid = gid;

  // 새로 생성된 파일/디렉터리이므로 크기를 0으로 설정
  inode->size = 0;

  // 접근(atime), 수정(mtime), 상태 변경(ctime) 시간을 현재 시각으로 초기화
  inode->atime = inode->mtime = inode->ctime = (uint32_t)time(NULL);

  // Direct 블록 포인터를 모두 0으로 초기화하여 데이터 블록을 아직 가리키지 않음
  for (int i = 0; i < SFUSE_NDIR_BLOCKS; i++)
    inode->direct[i] = 0;

  // Single indirect, Double indirect 블록 포인터도 데이터가 없음을 나타내기
  // 위해 0 설정
  inode->indirect = 0;
  inode->double_indirect = 0;

  // 디렉터리면 기본 링크 수를 2로, 파일이면 1로 설정
  inode->links = (S_ISDIR(mode) ? 2 : 1);
}

/**
 * @brief 아이노드를 디스크에서 읽어 메모리에 로드한다.
 *
 * 파일 시스템에서 파일과 디렉터리의 메타데이터(크기, 권한, 소유자, 데이터 블록
 * 위치 등)는 아이노드(inode)에 저장된다. 이 함수는 디스크에 저장된 아이노드
 * 데이터를 읽어 메모리의 inode 구조체에 로드한다.
 *
 * 아이노드를 로드하는 과정은 다음과 같다:
 *
 * 1. 아이노드 번호(ino)의 유효성을 검사한다.
 *    - ino 번호는 0이 아니어야 하고, 슈퍼블록의 inodes_count보다 작아야 한다.
 *
 * 2. 디스크 내의 아이노드 위치(offset)를 계산한다.
 *    - 슈퍼블록의 inode_table_start는 아이노드 테이블의 시작 위치를 나타내는
 *      블록 번호이다.
 *    - 아이노드의 크기(sizeof(*inode))에 아이노드 번호를 곱해 정확한 위치를
 *      계산한다.
 *
 * 3. 계산된 위치에서 아이노드 데이터를 읽어 inode 버퍼에 저장한다.
 *    - 읽은 데이터의 크기가 아이노드 구조체 크기와 일치하지 않으면 입출력
 *      오류(EIO)를 반환한다.
 *
 * @param fd      디바이스 파일 디스크립터
 * @param sb      슈퍼블록 정보 포인터 (아이노드 테이블의 위치와 총 개수 정보
 * 포함)
 * @param ino     읽을 아이노드 번호 (0번 아이노드는 무효로 간주됨)
 * @param inode   디스크에서 읽은 아이노드를 저장할 메모리 버퍼 포인터
 *
 * @return 성공 시 0 반환, 실패 시 음수의 오류 코드 반환
 *         -EINVAL : 유효하지 않은 아이노드 번호(ino가 0이거나 유효 범위를 초과)
 *         -EIO    : 아이노드 데이터를 읽는 중 입출력 오류 발생
 */
int inode_load(int fd, const struct sfuse_super *sb, uint32_t ino,
               struct sfuse_inode *inode) {
  /* [1단계] 아이노드 번호(ino) 유효성 검사 */
  if (ino == 0 || ino >= sb->inodes_count)
    return -EINVAL; // ino가 유효하지 않음 (0이거나 최대 범위 초과)

  /* [2단계] 디스크에서 아이노드가 저장된 위치(offset) 계산 */
  off_t off = ((off_t)sb->inode_table_start * SFUSE_BLOCK_SIZE) +
              (ino * sizeof(*inode));
  /*
   * inode_table_start 블록 번호를 실제 바이트 단위로 변환하기 위해 블록
   * 크기(SFUSE_BLOCK_SIZE)를 곱함. 거기에 inode 번호(ino)에 inode
   * 크기(sizeof(*inode))를 곱하여 정확한 위치 계산.
   */

  /* [3단계] 디스크에서 아이노드 데이터를 읽어 메모리에 로드 */
  ssize_t ret = disk_read(fd, inode, sizeof(*inode), off);
  if (ret < 0)
    return (int)ret; // 디스크 읽기 오류 발생 시 해당 오류 코드 반환

  // 읽은 데이터 크기가 아이노드 크기와 불일치 시 입출력 오류 반환
  if ((size_t)ret != sizeof(*inode))
    return -EIO;

  return 0; // 성공적으로 아이노드 로드 완료
}

/**
 * @brief 메모리에 저장된 아이노드를 디스크에 기록(동기화)한다.
 *
 * 파일 시스템은 아이노드를 메모리 내에서 수정한 후, 변경된 내용을 디스크에
 * 영구적으로 저장해야 한다. 이 함수는 메모리 상의 아이노드를 디스크의 올바른
 * 위치에 정확하게 기록하여 파일의 메타데이터가 유지되도록 한다.
 *
 * 아이노드를 디스크에 기록하는 과정은 다음과 같다:
 *
 * 1. 아이노드 번호(ino)의 유효성을 검사한다.
 *    - ino 번호는 반드시 0보다 크고 슈퍼블록의 총 아이노드
 *      개수(inodes_count)보다 작아야 한다.
 *
 * 2. 기록할 아이노드 데이터의 디스크 내 위치(offset)를 계산한다.
 *    - 아이노드 테이블의 시작 위치(inode_table_start)를 블록 단위에서 바이트
 *      단위로 변환하고, 여기에 아이노드 번호(ino)와 아이노드의
 *      크기(sizeof(*inode))를 곱하여 정확한 위치를 얻는다.
 *
 * 3. 메모리 상의 아이노드 데이터를 계산된 위치에 디스크로 기록한다.
 *    - 기록 후, 실제로 기록된 바이트 수가 아이노드의 크기와 정확히 일치하는지
 *      확인한다.
 *    - 기록된 바이트 수가 일치하지 않거나 오류가 발생하면 적절한 오류 코드를
 *      반환한다.
 *
 * @param fd      디바이스 파일 디스크립터 (디스크 접근을 위한 식별자)
 * @param sb      슈퍼블록 정보 (아이노드 테이블 위치 정보 포함)
 * @param ino     디스크에 기록할 아이노드 번호
 * @param inode   메모리에 있는, 디스크에 기록될 아이노드 데이터 포인터
 *
 * @return 성공 시 0 반환, 실패 시 음수의 오류 코드 반환
 *         -EINVAL : 유효하지 않은 아이노드 번호(0이거나 범위 초과)
 *         -EIO    : 디스크 기록 과정에서 입출력 오류가 발생
 */
int inode_sync(int fd, const struct sfuse_super *sb, uint32_t ino,
               const struct sfuse_inode *inode) {
  /* [1단계] 아이노드 번호(ino)의 유효성 검사 */
  if (ino == 0 || ino >= sb->inodes_count)
    return -EINVAL; // 아이노드 번호가 유효하지 않을 경우

  /* [2단계] 아이노드 데이터를 기록할 디스크의 정확한 위치(offset) 계산 */
  off_t off = ((off_t)sb->inode_table_start * SFUSE_BLOCK_SIZE) +
              (ino * sizeof(*inode));
  /*
   * inode_table_start는 블록 번호를 나타내므로 바이트 단위로 변환하기 위해
   * SFUSE_BLOCK_SIZE를 곱한다.
   * 아이노드 번호(ino)에 아이노드 크기를 곱해 정확한 바이트 오프셋을 결정한다.
   */

  /* [3단계] 메모리의 아이노드를 디스크의 해당 위치에 기록 */
  ssize_t ret = disk_write(fd, inode, sizeof(*inode), off);
  if (ret < 0)
    return (int)ret; // 디스크 기록 오류가 발생했을 때 오류 코드 반환

  if ((size_t)ret != sizeof(*inode))
    return -EIO; // 기록된 데이터의 크기가 아이노드 크기와 다르면 입출력 오류
                 // 반환

  return 0; // 성공적으로 아이노드를 디스크에 기록함
}

/**
 * @brief 논리 블록 번호(Logical Block Number, LBN)를 물리 블록 번호(Physical
 * Block Number, PBN)로 변환한다.
 *
 * 파일은 여러 블록에 걸쳐 데이터를 저장하며, 파일 내에서의 블록 위치는
 * 논리적(LBN)으로 표현된다. 그러나 실제 디스크는 논리 블록을 직접적으로 알지
 * 못하므로, 물리적인 블록 번호(PBN)로 변환해 접근해야 한다. 이 함수는
 * 아이노드에 저장된 블록 주소를 참조하여 LBN을 PBN으로 변환해주는 역할을
 * 수행한다.
 *
 * LBN에서 PBN 변환 방법:
 *   - [Direct Block 처리]
 *     * LBN이 SFUSE_NDIR_BLOCKS(보통 12개)보다 작으면, direct 배열에서 직접
 *       찾아 반환한다.
 *   - [Single Indirect Block 처리]
 *     * Direct 블록으로 표현할 수 있는 범위를 넘어가면, Single indirect
 *       블록에서 찾는다.
 *     * Single indirect 블록이 가리키는 블록 하나에는 여러 개의 블록 주소를
 *       담을 수 있다.
 *   - [Double Indirect Block 처리]
 *     * Single indirect 블록 범위도 초과하면, Double indirect 블록을 참조해야
 *       한다.
 *     * Double indirect 블록은 두 단계의 포인터를 거쳐 주소를 참조한다.
 *
 * @param fd       디바이스 파일 디스크립터 (디스크 접근용)
 * @param sb       슈퍼블록 정보 (현재 사용하지 않음)
 * @param inode    대상 파일의 아이노드 포인터
 * @param lbn      변환하려는 논리 블록 번호
 * @param buf      블록 데이터를 읽기 위한 임시 버퍼 (크기: SFUSE_BLOCK_SIZE)
 * @param pbn_out  변환된 물리 블록 번호를 저장할 출력 포인터
 *
 * @return 성공 시 0, 실패 시 음수의 오류 코드 반환
 *         -ENOENT : 요청한 블록이 할당되지 않아 존재하지 않음
 *         -EIO    : 블록 데이터를 디스크에서 읽을 때 입출력 오류 발생
 */
int logical_to_physical(int fd, const struct sfuse_super *sb,
                        const struct sfuse_inode *inode, uint32_t lbn,
                        void *buf, uint32_t *pbn_out) {

  (void)sb; // 미사용 파라미터로 인한 컴파일러 경고 방지

  /* --- [1단계: Direct 블록 확인] --- */
  if (lbn < SFUSE_NDIR_BLOCKS) {
    // 요청한 논리 블록 번호가 Direct 블록의 범위 내에 있음
    *pbn_out = inode->direct[lbn]; // 직접적인 배열 참조로 즉시 얻을 수 있음
    return 0;
  }

  /* --- [2단계: Single Indirect 블록 확인] --- */
  lbn -= SFUSE_NDIR_BLOCKS; // Direct 블록의 범위를 초과했으므로, Single
                            // indirect 범위로 보정

  uint32_t entries = SFUSE_BLOCK_SIZE / sizeof(uint32_t);
  // 하나의 블록에 저장 가능한 블록 주소의 개수를 계산
  // (예: 4096 바이트 블록 / 4 바이트 주소 = 1024개의 블록 주소 저장 가능)

  if (lbn < entries) {
    // Single indirect 블록에서 찾을 수 있는 범위 내에 있음
    if (inode->indirect == 0)
      return -ENOENT; // Single indirect 블록 자체가 아직 할당되지 않은 상태

    // indirect 블록 번호가 존재하면, 해당 블록을 디스크에서 읽는다
    if (read_block(fd, inode->indirect, buf) < 0)
      return -EIO; // indirect 블록을 읽는 도중 오류가 발생하면 입출력 오류 반환

    // indirect 블록에서 블록 주소 배열을 가져옴
    uint32_t *ptrs = (uint32_t *)buf;
    *pbn_out = ptrs[lbn]; // indirect 블록의 주소 배열에서 요청한 논리 블록의
                          // 물리 블록 번호를 얻음
    return 0;
  }

  /* --- [3단계: Double Indirect 블록 확인] --- */
  lbn -= entries; // Single indirect 블록의 범위를 초과하였으므로, Double
                  // indirect 블록을 처리하기 위해 보정

  uint32_t idx1 = lbn / entries; // Double indirect 첫 번째 레벨의 인덱스 계산
  uint32_t idx2 = lbn % entries; // Double indirect 두 번째 레벨의 인덱스 계산

  if (inode->double_indirect == 0)
    return -ENOENT; // Double indirect 블록 자체가 할당되지 않은 상태

  // double indirect의 첫 번째 레벨 블록을 읽는다.
  if (read_block(fd, inode->double_indirect, buf) < 0)
    return -EIO; // 첫 번째 레벨 블록을 읽을 때 오류 발생 시 입출력 오류 반환

  uint32_t *ptrs1 = (uint32_t *)buf;
  uint32_t block2 = ptrs1[idx1]; // 두 번째 레벨 블록 번호를 얻음

  if (block2 == 0)
    return -ENOENT; // 두 번째 레벨 블록이 할당되지 않은 경우

  // 두 번째 레벨 블록을 디스크에서 읽는다.
  if (read_block(fd, block2, buf) < 0)
    return -EIO; // 두 번째 레벨 블록을 읽을 때 오류 발생 시 입출력 오류 반환

  uint32_t *ptrs2 = (uint32_t *)buf;
  *pbn_out = ptrs2[idx2]; // 최종적으로 변환된 물리 블록 번호를 얻음

  return 0; // 성공적으로 물리 블록 번호 변환 완료
}
