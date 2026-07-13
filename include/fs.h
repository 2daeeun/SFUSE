/**
 * @file include/fs.h
 * @brief SFUSE 파일 시스템의 핵심 구조 및 연산 정의
 */

#ifndef SFUSE_FS_H
#define SFUSE_FS_H

#include "super.h"
#include <stdint.h>

/**
 * @def SFUSE_ROOT_INO
 * @brief 파일 시스템 루트 디렉터리의 고정된 아이노드(inode) 번호 (일반적으로 1)
 *
 * @details
 * SFUSE 파일 시스템의 루트 디렉터리('/')를 나타내기 위한 아이노드 번호로,
 * 파일 시스템 내부에서 모든 경로 탐색 및 해석의 출발점(entry point)으로
 * 활용된다. 루트 아이노드는 파일 시스템 초기화 시 반드시 생성되어야 하며,
 * 아이노드 테이블 내에서 항상 아이노드 번호 1번에 위치한다.
 *
 * @note 대부분의 Unix 기반 파일 시스템에서는 관례적으로 루트 디렉터리의
 * 아이노드 번호를 1로 고정한다. 이러한 고정 번호를 사용하면 루트 디렉터리를
 * 참조할 때 추가적인 탐색 과정 없이 즉각적으로 접근할 수 있어 파일 시스템 성능
 * 및 효율성을 높이는 데 기여한다.
 */
#define SFUSE_ROOT_INO 1

/**
 * @struct sfuse_fs
 * @brief SFUSE 파일 시스템의 전역 컨텍스트를 나타내는 구조체
 *
 * @details
 * SFUSE 파일 시스템의 핵심 상태와 데이터를 관리하는 중심 구조체이다.
 * 블록 디바이스 접근을 위한 파일 디스크립터(backing_fd)를 저장하며,
 * 슈퍼블록(sb)을 통해 파일 시스템의 메타데이터를 관리한다.
 * 또한 블록과 아이노드의 할당 상태를 각각 비트맵 형태(block_map, inode_map)로
 * 저장하여, 데이터 블록과 아이노드의 효율적인 관리와 할당을 지원한다. 이
 * 구조체는 파일 시스템의 마운트 시 초기화되어 FUSE 연산 수행 시 전역적으로
 * 사용된다.
 */
struct sfuse_fs {
  int backing_fd;        /**< 블록 디바이스의 파일 디스크립터 */
  struct sfuse_super sb; /**< 슈퍼블록 구조체 */
  uint8_t *block_map;    /**< 블록 비트맵 버퍼 포인터 */
  uint8_t *inode_map;    /**< 아이노드 비트맵 버퍼 포인터 */
};

/**
 * @brief FUSE 콜백 함수 내에서 전역 파일 시스템 컨텍스트를 얻는다.
 *
 * @return 파일 시스템의 전역 컨텍스트(struct sfuse_fs*) 포인터
 */
struct sfuse_fs *get_fs_context(void);

/**
 * @brief SFUSE 파일 시스템을 초기화하고 슈퍼블록 및 비트맵을 로드한다.
 *
 * @param backing_fd 초기화할 블록 디바이스의 파일 디스크립터
 *
 * @return 성공 시 0, 실패 시 음수 오류 코드
 */
int fs_initialize(int backing_fd);

/**
 * @brief SFUSE 파일 시스템을 정리하고 언마운트한다.
 *
 * @param private_data get_fs_context()가 반환한 파일 시스템 전역 컨텍스트
 * 포인터
 */
void fs_destroy(void *private_data);

/**
 * @brief 주어진 경로를 아이노드 번호로 변환한다.
 *
 * @param fs 파일 시스템의 전역 컨텍스트
 * @param path 변환할 경로 문자열
 * @param ino_out 결과 아이노드 번호를 저장할 포인터
 *
 * @return 성공 시 0, 실패 시 음수 오류 코드
 */
int fs_resolve_path(struct sfuse_fs *fs, const char *path, uint32_t *ino_out);

/**
 * @brief 전체 경로를 부모 디렉터리의 아이노드와 마지막 구성요소의 이름으로
 * 분할한다.
 *
 * 예: "/foo/bar" → 부모 디렉터리 아이노드와 "bar"를 분리하여 반환한다.
 * 반환된 이름은 동적으로 할당되므로 사용 후 free()로 메모리를 해제해야 한다.
 *
 * @param fullpath 분할할 전체 경로 문자열
 * @param parent_ino 부모 디렉터리 아이노드 번호를 저장할 포인터
 *
 * @return 마지막 구성요소의 이름을 담은 문자열 (동적 할당)
 */
char *fs_split_path(const char *fullpath, uint32_t *parent_ino);

#endif /* SFUSE_FS_H */
