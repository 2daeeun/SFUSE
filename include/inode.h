// inode.h: 아이노드 구조체 및 함수 선언
#ifndef SFUSE_INODE_H
#define SFUSE_INODE_H

#include "super.h" // struct sfuse_super, SFUSE_BLOCK_SIZE
#include <stdint.h>
#include <sys/types.h> // mode_t, uid_t, gid_t

// Direct 블록 포인터 수
#define SFUSE_NDIR_BLOCKS 12

/**
 * @brief 아이노드 구조체
 */
struct sfuse_inode {
  mode_t mode;                        /* 파일 타입 및 권한 */
  uid_t uid;                          /* 소유자 UID */
  gid_t gid;                          /* 소유자 GID */
  uint32_t size;                      /* 파일 크기 */
  uint32_t atime;                     /* 마지막 접근 시간 */
  uint32_t mtime;                     /* 마지막 수정 시간 */
  uint32_t ctime;                     /* 상태 변경 시간 */
  uint32_t direct[SFUSE_NDIR_BLOCKS]; /* Direct 블록 포인터 */
  uint32_t indirect;                  /* Single indirect 블록 포인터 */
  uint32_t double_indirect;           /* Double indirect 블록 포인터 */
  uint32_t links;                     /* 링크 수 */
};

/**
 * @brief 디스크에서 아이노드 읽기
 * @param fd      디바이스 파일 디스크립터
 * @param sb      슈퍼블록 정보
 * @param ino     읽을 아이노드 번호
 * @param inode   결과 저장 버퍼
 * @return 0 성공, 음수 오류코드
 */
int inode_load(int fd, const struct sfuse_super *sb, uint32_t ino,
               struct sfuse_inode *inode);

/**
 * @brief 디스크에 아이노드 기록
 * @param fd      디바이스 파일 디스크립터
 * @param sb      슈퍼블록 정보
 * @param ino     기록할 아이노드 번호
 * @param inode   기록할 아이노드
 * @return 0 성공, 음수 오류코드
 */
int inode_sync(int fd, const struct sfuse_super *sb, uint32_t ino,
               const struct sfuse_inode *inode);

/**
 * @brief 새 아이노드 초기화
 * @param sb     슈퍼블록 정보 (unused)
 * @param ino    아이노드 번호 (unused)
 * @param mode   파일 타입 및 권한
 * @param uid    소유자 UID
 * @param gid    소유자 GID
 * @param inode  초기화할 아이노드 구조체
 */
void fs_init_inode(const struct sfuse_super *sb, uint32_t ino, mode_t mode,
                   uid_t uid, gid_t gid, struct sfuse_inode *inode);

/**
 * @brief 논리 블록 인덱스를 물리 블록 번호로 변환
 * @param fd      디바이스 파일 디스크립터
 * @param sb      슈퍼블록 정보
 * @param inode   대상 아이노드
 * @param lbn     논리 블록 인덱스
 * @param buf     블록 읽기용 버퍼 (SFUSE_BLOCK_SIZE 크기)
 * @param pbn_out 변환된 물리 블록 번호 반환
 * @return 0 성공, 음수 오류코드
 */
int logical_to_physical(int fd, const struct sfuse_super *sb,
                        const struct sfuse_inode *inode, uint32_t lbn,
                        void *buf, uint32_t *pbn_out);

#endif // SFUSE_INODE_H
