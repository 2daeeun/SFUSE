// File: include/fs.h

#ifndef SFUSE_FS_H
#define SFUSE_FS_H

#include "super.h"
#include <fuse3/fuse.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

/* 전방 선언 */
struct sfuse_bitmaps;
struct sfuse_inode_block;

/**
 * @brief SFUSE 파일 시스템 컨텍스트 구조체
 */
struct sfuse_fs {
  int backing_fd;                        /**< 디스크 이미지 파일 디스크립터 */
  struct sfuse_superblock sb;            /**< 슈퍼블록 정보 */
  struct sfuse_bitmaps *bmaps;           /**< 아이노드/블록 비트맵 포인터 */
  struct sfuse_inode_block *inode_table; /**< 아이노드 테이블 블록들 */
};

/**
 * @brief 파일 시스템 초기화
 * @param image_path 디스크 이미지 파일 경로
 * @param error_out 오류 코드를 저장할 포인터
 * @return 초기화된 sfuse_fs 포인터 또는 NULL
 */
struct sfuse_fs *fs_initialize(const char *image_path, int *error_out);

/**
 * @brief 파일 시스템 정리
 * @param fs 해제할 파일 시스템 컨텍스트
 */
void fs_teardown(struct sfuse_fs *fs);

/**
 * @brief 경로를 inode 번호로 변환
 * @param fs 파일 시스템 컨텍스트
 * @param path 변환할 경로 문자열
 * @param out_ino 변환된 inode 번호 저장 위치
 * @return 성공 시 0, 실패 시 음수 오류 코드
 */
int fs_resolve_path(struct sfuse_fs *fs, const char *path, uint32_t *out_ino);

/* 고수준 FUSE 연동 함수 선언 */

/**
 * @brief 파일/디렉터리 속성 조회
 * @param fs 파일 시스템 컨텍스트
 * @param path 대상 경로
 * @param stbuf 출력할 stat 구조체 포인터
 * @return 성공 시 0, 실패 시 음수 오류 코드
 */
int fs_getattr(struct sfuse_fs *fs, const char *path, struct stat *stbuf);

/**
 * @brief 접근 권한 검사
 * @param fs 파일 시스템 컨텍스트
 * @param path 대상 경로
 * @param mask 검사할 접근 마스크
 * @return 성공 시 0, 실패 시 음수 오류 코드
 */
int fs_access(struct sfuse_fs *fs, const char *path, int mask);

/**
 * @brief 디렉터리 내용 읽기
 * @param fs 파일 시스템 컨텍스트
 * @param path 디렉터리 경로
 * @param buf FUSE가 제공하는 버퍼
 * @param filler 디렉터리 엔트리 추가 콜백
 * @param offset 읽기 오프셋
 * @return 성공 시 0, 실패 시 음수 오류 코드
 */
int fs_readdir(struct sfuse_fs *fs, const char *path, void *buf,
               fuse_fill_dir_t filler, off_t offset);

/**
 * @brief 파일 열기
 * @param fs 파일 시스템 컨텍스트
 * @param path 파일 경로
 * @param fi FUSE 파일 정보 구조체
 * @return 성공 시 0, 실패 시 음수 오류 코드
 */
int fs_open(struct sfuse_fs *fs, const char *path, struct fuse_file_info *fi);

/**
 * @brief 파일 읽기
 * @param fs 파일 시스템 컨텍스트
 * @param path 파일 경로
 * @param buf 읽은 데이터를 저장할 버퍼
 * @param size 읽을 바이트 수
 * @param offset 읽기 오프셋
 * @return 읽은 바이트 수 또는 음수 오류 코드
 */
int fs_read(struct sfuse_fs *fs, const char *path, char *buf, size_t size,
            off_t offset);

/**
 * @brief 파일 쓰기
 * @param fs 파일 시스템 컨텍스트
 * @param path 파일 경로
 * @param buf 쓸 데이터 버퍼
 * @param size 쓸 바이트 수
 * @param offset 쓰기 오프셋
 * @return 쓴 바이트 수 또는 음수 오류 코드
 */
int fs_write(struct sfuse_fs *fs, const char *path, const char *buf,
             size_t size, off_t offset);

/**
 * @brief 파일 생성
 * @param fs 파일 시스템 컨텍스트
 * @param path 생성할 파일 경로
 * @param mode 생성 모드 (퍼미션)
 * @param fi FUSE 파일 정보 구조체
 * @return 성공 시 0, 실패 시 음수 오류 코드
 */
int fs_create(struct sfuse_fs *fs, const char *path, mode_t mode,
              struct fuse_file_info *fi);

/**
 * @brief 디렉터리 생성
 * @param fs 파일 시스템 컨텍스트
 * @param path 생성할 디렉터리 경로
 * @param mode 생성 모드 (퍼미션)
 * @return 성공 시 0, 실패 시 음수 오류 코드
 */
int fs_mkdir(struct sfuse_fs *fs, const char *path, mode_t mode);

/**
 * @brief 파일 삭제
 * @param fs 파일 시스템 컨텍스트
 * @param path 삭제할 파일 경로
 * @return 성공 시 0, 실패 시 음수 오류 코드
 */
int fs_unlink(struct sfuse_fs *fs, const char *path);

/**
 * @brief 디렉터리 삭제
 * @param fs 파일 시스템 컨텍스트
 * @param path 삭제할 디렉터리 경로
 * @return 성공 시 0, 실패 시 음수 오류 코드
 */
int fs_rmdir(struct sfuse_fs *fs, const char *path);

/**
 * @brief 파일 또는 디렉터리 이름 변경
 * @param fs 파일 시스템 컨텍스트
 * @param from 원본 경로
 * @param to 대상 경로
 * @return 성공 시 0, 실패 시 음수 오류 코드
 */
int fs_rename(struct sfuse_fs *fs, const char *from, const char *to);

/**
 * @brief 파일 크기 조정 (truncate)
 * @param fs 파일 시스템 컨텍스트
 * @param path 대상 파일 경로
 * @param size 새로운 파일 크기
 * @return 성공 시 0, 실패 시 음수 오류 코드
 */
int fs_truncate(struct sfuse_fs *fs, const char *path, off_t size);

/**
 * @brief 파일의 시간 속성 변경 (utimens)
 * @param fs 파일 시스템 컨텍스트
 * @param path 대상 경로
 * @param tv [0]=접근 시간, [1]=변경 시간
 * @return 성공 시 0, 실패 시 음수 오류 코드
 */
int fs_utimens(struct sfuse_fs *fs, const char *path,
               const struct timespec tv[2]);

/**
 * @brief FUSE에서 플러시 요청 처리
 * @param fs 파일 시스템 컨텍스트
 * @param path 대상 파일 경로
 * @param fi FUSE 파일 정보 구조체
 * @return 성공 시 0, 실패 시 음수 오류 코드
 */
int fs_flush(struct sfuse_fs *fs, const char *path, struct fuse_file_info *fi);

/**
 * @brief FUSE에서 fsync 요청 처리
 * @param fs 파일 시스템 컨텍스트
 * @param path 대상 파일 경로
 * @param datasync 데이터 sync 플래그
 * @param fi FUSE 파일 정보 구조체
 * @return 성공 시 0, 실패 시 음수 오류 코드
 */
int fs_fsync(struct sfuse_fs *fs, const char *path, int datasync,
             struct fuse_file_info *fi);

/**
 * @brief SFUSE용 FUSE operations 구조체 반환
 * @return 설정된 fuse_operations 포인터
 */
struct fuse_operations *sfuse_get_operations(void);

#endif /* SFUSE_FS_H */
