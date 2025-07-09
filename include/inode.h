/**
 * @file include/inode.h
 * @brief 아이노드(inode) 구조체 정의 및 관리 함수 선언
 *
 * 이 파일은 SFUSE 파일 시스템에서 파일 메타데이터를 관리하는 아이노드의
 * 구조체를 정의하고, 아이노드를 디스크로부터 읽거나 쓰는 기능과 논리 블록
 * 번호를 물리 블록 번호로 변환하는 함수들을 제공한다.
 */

#ifndef SFUSE_INODE_H
#define SFUSE_INODE_H

#include "super.h" ///< struct sfuse_super, SFUSE_BLOCK_SIZE 정의
#include <stdint.h>
#include <sys/types.h> ///< mode_t, uid_t, gid_t 정의

/// Direct 블록 포인터 수 (12개)
#define SFUSE_NDIR_BLOCKS 12

/**
 * @struct sfuse_inode
 * @brief 파일의 메타데이터를 관리하는 아이노드 구조체
 *
 * 아이노드는 파일의 권한, 소유자, 크기, 타임스탬프, 블록 포인터 등의
 * 메타데이터를 저장한다.
 */
struct sfuse_inode {
  mode_t mode;                        ///< 파일 타입 및 접근 권한
  uid_t uid;                          ///< 파일 소유자의 사용자 ID
  gid_t gid;                          ///< 파일 소유자의 그룹 ID
  uint32_t size;                      ///< 파일 크기 (바이트 단위)
  uint32_t atime;                     ///< 마지막 접근 시간 (Access Time)
  uint32_t mtime;                     ///< 마지막 수정 시간 (Modification Time)
  uint32_t ctime;                     ///< 상태 변경 시간 (Change Time)
  uint32_t direct[SFUSE_NDIR_BLOCKS]; ///< 직접 참조 블록 포인터 배열
  uint32_t indirect;                  ///< Single Indirect 블록 포인터
  uint32_t double_indirect;           ///< Double Indirect 블록 포인터
  uint32_t links;                     ///< 파일에 연결된 링크 수
};

/**
 * @brief 새로운 아이노드를 기본 값으로 초기화한다.
 *
 * 새 아이노드는 주어진 모드, 사용자 ID, 그룹 ID로 초기화되며,
 * 나머지 필드는 기본값(예: 크기 0, 링크 수 1)으로 설정된다.
 *
 * @param sb     슈퍼블록 정보 포인터 (현재 사용되지 않음)
 * @param ino    아이노드 번호 (현재 사용되지 않음)
 * @param mode   새 파일의 타입 및 권한
 * @param uid    파일 소유자의 사용자 ID
 * @param gid    파일 소유자의 그룹 ID
 * @param inode  초기화할 아이노드 포인터
 */
void fs_init_inode(const struct sfuse_super *sb, uint32_t ino, mode_t mode,
                   uid_t uid, gid_t gid, struct sfuse_inode *inode);

/**
 * @brief 디스크에서 지정된 아이노드를 읽어 메모리에 로드한다.
 *
 * @param fd      디바이스 파일 디스크립터
 * @param sb      슈퍼블록 정보 포인터
 * @param ino     읽어올 아이노드 번호
 * @param inode   읽은 아이노드를 저장할 버퍼 포인터
 * @return 성공 시 0, 실패 시 음수의 오류 코드 반환
 */
int inode_load(int fd, const struct sfuse_super *sb, uint32_t ino,
               struct sfuse_inode *inode);

/**
 * @brief 메모리에 저장된 아이노드 정보를 디스크에 기록한다.
 *
 * @param fd      디바이스 파일 디스크립터
 * @param sb      슈퍼블록 정보 포인터
 * @param ino     기록할 아이노드 번호
 * @param inode   디스크에 저장할 아이노드 정보 포인터
 * @return 성공 시 0, 실패 시 음수의 오류 코드 반환
 */
int inode_sync(int fd, const struct sfuse_super *sb, uint32_t ino,
               const struct sfuse_inode *inode);

/**
 * @brief 논리 블록 번호(Logical Block Number)를 물리 블록 번호(Physical Block
 * Number)로 변환한다.
 *
 * 파일 내의 논리적 블록 위치를 실제 디스크 상의 물리적 블록 위치로 변환하여
 * 데이터를 접근할 수 있도록 한다.
 *
 * @param fd       디바이스 파일 디스크립터
 * @param sb       슈퍼블록 정보 포인터
 * @param inode    대상 아이노드 포인터
 * @param lbn      변환할 논리 블록 번호
 * @param buf      임시로 사용할 블록 크기의 버퍼 (크기는 SFUSE_BLOCK_SIZE)
 * @param pbn_out  결과로 변환된 물리 블록 번호를 저장할 포인터
 * @return 성공 시 0, 실패 시 음수의 오류 코드 반환
 */
int logical_to_physical(int fd, const struct sfuse_super *sb,
                        const struct sfuse_inode *inode, uint32_t lbn,
                        void *buf, uint32_t *pbn_out);

#endif // SFUSE_INODE_H
