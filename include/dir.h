#ifndef SFUSE_DIR_H
#define SFUSE_DIR_H

#include "inode.h"
#include "super.h"
#include <stddef.h>
#include <stdint.h>

// 디렉터리 엔트리의 최대 이름 길이
#define SFUSE_NAME_LEN 255

/**
 * @brief 디렉터리 엔트리 구조체
 * ino: 저장된 아이노드 번호 (0이면 빈 슬롯)
 * name: NULL 종단 문자열 (최대 SFUSE_NAME_LEN)
 */
struct sfuse_dirent {
  uint32_t ino;
  char name[SFUSE_NAME_LEN + 1];
};

/**
 * @brief 디렉터리 블록들을 모두 읽어들임
 * @param fd   디바이스 FD
 * @param sb   슈퍼블록 정보
 * @param ino  디렉터리 아이노드 번호
 * @param buf  읽은 디렉터리 데이터(연속 블록) 저장버퍼
 * @return 0 성공, 음수 오류코드
 */
int dir_load(int fd, const struct sfuse_super *sb, uint32_t ino, void *buf);

/**
 * @brief 디렉터리에 새 엔트리 추가
 * @param fd        디바이스 FD
 * @param sb        슈퍼블록 정보
 * @param ino       부모 디렉터리 아이노드 번호
 * @param name      추가할 파일/디렉터리 이름
 * @param child_ino 새 엔트리의 아이노드 번호
 * @param block_map 블록 비트맵 버퍼
 * @param inode_map 아이노드 비트맵 버퍼
 * @param supermap  슈퍼블록 복사본(동기화용)
 * @return 0 성공, 음수 오류코드
 */
int dir_add_entry(int fd, const struct sfuse_super *sb, uint32_t ino,
                  const char *name, uint32_t child_ino, uint8_t *block_map,
                  uint8_t *inode_map, struct sfuse_super *supermap);

/**
 * @brief 디렉터리 엔트리 삭제
 * @param fd   디바이스 FD
 * @param sb   슈퍼블록 정보
 * @param ino  부모 디렉터리 아이노드 번호
 * @param name 삭제할 엔트리 이름
 * @return 0 성공, 음수 오류코드
 */
int dir_remove_entry(int fd, const struct sfuse_super *sb, uint32_t ino,
                     const char *name);

#endif // SFUSE_DIR_H
