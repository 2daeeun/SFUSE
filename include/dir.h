/**
 * @file include/dir.h
 * @brief 파일 시스템의 디렉터리 관리 기능 정의
 *
 * 디렉터리의 생성, 읽기, 추가 및 삭제와 같은 관리 작업을 처리하는 함수와 구조체
 * 정의. 디렉터리 엔트리는 파일이나 하위 디렉터리를 나타낸다.
 */

#ifndef SFUSE_DIR_H
#define SFUSE_DIR_H

#include "inode.h"
#include "super.h"
#include <stddef.h>
#include <stdint.h>

// 디렉터리 엔트리의 최대 이름 길이
#define SFUSE_NAME_LEN 255

/**
 * @struct sfuse_dirent
 * @brief 디렉터리 엔트리 정보를 저장하는 구조체
 *
 * 각 디렉터리 엔트리는 연결된 아이노드 번호와 해당 엔트리의 이름을 포함한다.
 * 빈 엔트리는 아이노드 번호(ino)가 0이다.
 */
struct sfuse_dirent {
  uint32_t ino; /**< 디렉터리 엔트리에 연결된 아이노드 번호 (0이면 빈 슬롯) */
  char name[SFUSE_NAME_LEN + 1]; /**< NULL 종단 문자열 형태의 엔트리 이름 (최대
                                    SFUSE_NAME_LEN) */
};

/**
 * @brief 디렉터리의 모든 블록을 디스크에서 읽어들인다.
 *
 * 지정된 아이노드 번호(디렉터리)에 해당하는 데이터를 연속적인 블록으로 읽어서
 * 버퍼에 저장한다.
 *
 * @param fd   디바이스 파일 디스크립터
 * @param sb   슈퍼블록 정보 구조체 포인터
 * @param ino  디렉터리 아이노드 번호
 * @param buf  읽은 디렉터리 데이터(연속 블록)를 저장할 버퍼
 * @return 성공 시 0 반환, 실패 시 음수의 오류 코드 반환
 */
int dir_load(int fd, const struct sfuse_super *sb, uint32_t ino, void *buf);

/**
 * @brief 디렉터리에 새로운 엔트리를 추가한다.
 *
 * 부모 디렉터리에 파일 또는 디렉터리를 나타내는 엔트리를 추가하며,
 * 추가 과정에서 블록 및 아이노드 비트맵과 슈퍼블록을 적절히 갱신한다.
 *
 * @param fd        디바이스 파일 디스크립터
 * @param sb        슈퍼블록 정보 구조체 포인터
 * @param ino       부모 디렉터리의 아이노드 번호
 * @param name      추가할 파일 또는 디렉터리의 이름
 * @param child_ino 추가될 엔트리에 연결될 새로운 아이노드 번호
 * @param block_map 블록 비트맵 버퍼 (사용된 블록 관리용)
 * @param inode_map 아이노드 비트맵 버퍼 (사용된 아이노드 관리용)
 * @param supermap  갱신 후 슈퍼블록의 변경 사항 동기화를 위한 슈퍼블록 복사본
 * @return 성공 시 0 반환, 실패 시 음수의 오류 코드 반환
 */
int dir_add_entry(int fd, const struct sfuse_super *sb, uint32_t ino,
                  const char *name, uint32_t child_ino, uint8_t *block_map,
                  uint8_t *inode_map, struct sfuse_super *supermap);

/**
 * @brief 디렉터리에서 지정된 이름의 엔트리를 삭제한다.
 *
 * 부모 디렉터리에서 지정된 이름의 엔트리를 삭제하여 파일이나 하위 디렉터리를
 * 제거한다. 삭제 시 해당 엔트리가 차지한 블록과 아이노드는 별도의 처리가
 * 필요하다.
 *
 * @param fd   디바이스 파일 디스크립터
 * @param sb   슈퍼블록 정보 구조체 포인터
 * @param ino  부모 디렉터리의 아이노드 번호
 * @param name 삭제할 디렉터리 엔트리의 이름
 * @return 성공 시 0 반환, 실패 시 음수의 오류 코드 반환
 */
int dir_remove_entry(int fd, const struct sfuse_super *sb, uint32_t ino,
                     const char *name);

#endif // SFUSE_DIR_H
