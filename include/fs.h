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
  int backing_fd;                 /**< 블록 디바이스 파일 디스크립터 */
  struct sfuse_superblock sb;     /**< 슈퍼블록 정보 */
  struct sfuse_bitmaps *bmaps;    /**< 아이노드/데이터 블록 할당 비트맵 */
  struct sfuse_inode_block *itbl; /**< 아이노드 테이블 포인터 */
};

/**
 * @brief 파일 시스템 초기화
 *
 * 지정된 블록 디바이스를 열고, 슈퍼블록을 검사하여
 * 유효하지 않으면 자동으로 VSFS로 포맷합니다.
 *
 * @param device_path 디바이스 파일 경로 (예: /dev/nvme0n1p7)
 * @param error_out   오류 코드를 저장할 포인터 (0: 성공, 음수: 실패)
 * @return 초기화된 sfuse_fs 포인터, 실패 시 NULL
 */
struct sfuse_fs *fs_initialize(const char *device_path, int *error_out);

/**
 * @brief 파일 시스템 정리
 *
 * fs_initialize()로 생성된 컨텍스트를 해제합니다.
 *
 * @param fs 해제할 파일 시스템 컨텍스트
 */
void fs_teardown(struct sfuse_fs *fs);

/**
 * @brief 경로를 inode 번호로 변환
 *
 * 파일 경로 문자열을 inode 번호로 변환합니다.
 *
 * @param fs      파일 시스템 컨텍스트
 * @param path    변환할 경로 문자열
 * @param out_ino 변환된 inode 번호 저장 위치
 * @return 성공 시 0, 실패 시 음수 오류 코드
 */
int fs_resolve_path(struct sfuse_fs *fs, const char *path, uint32_t *out_ino);

/* —————— FUSE 고수준 연동 함수 —————— */

/**
 * @brief 파일/디렉터리 속성 조회 (getattr)
 */
int fs_getattr(struct sfuse_fs *fs, const char *path, struct stat *stbuf);

/**
 * @brief 접근 권한 검사 (access)
 */
int fs_access(struct sfuse_fs *fs, const char *path, int mask);

/**
 * @brief 디렉터리 내용 읽기 (readdir)
 */
int fs_readdir(struct sfuse_fs *fs, const char *path, void *buf,
               fuse_fill_dir_t filler, off_t offset);

/**
 * @brief 파일 열기 (open)
 */
int fs_open(struct sfuse_fs *fs, const char *path, struct fuse_file_info *fi);

/**
 * @brief 파일 읽기 (read)
 */
int fs_read(struct sfuse_fs *fs, const char *path, char *buf, size_t size,
            off_t offset);

/**
 * @brief 파일 쓰기 (write)
 */
int fs_write(struct sfuse_fs *fs, const char *path, const char *buf,
             size_t size, off_t offset);

/**
 * @brief 파일 생성 (create)
 */
int fs_create(struct sfuse_fs *fs, const char *path, mode_t mode,
              struct fuse_file_info *fi);

/**
 * @brief 디렉터리 생성 (mkdir)
 */
int fs_mkdir(struct sfuse_fs *fs, const char *path, mode_t mode);

/**
 * @brief 파일 삭제 (unlink)
 */
int fs_unlink(struct sfuse_fs *fs, const char *path);

/**
 * @brief 디렉터리 삭제 (rmdir)
 */
int fs_rmdir(struct sfuse_fs *fs, const char *path);

/**
 * @brief 이름 변경 (rename)
 */
int fs_rename(struct sfuse_fs *fs, const char *from, const char *to);

/**
 * @brief 파일 크기 조정 (truncate)
 */
int fs_truncate(struct sfuse_fs *fs, const char *path, off_t size);

/**
 * @brief 시간 속성 변경 (utimens)
 */
int fs_utimens(struct sfuse_fs *fs, const char *path,
               const struct timespec tv[2]);

/**
 * @brief FUSE flush 요청 처리 (flush)
 */
int fs_flush(struct sfuse_fs *fs, const char *path, struct fuse_file_info *fi);

/**
 * @brief FUSE fsync 요청 처리 (fsync)
 */
int fs_fsync(struct sfuse_fs *fs, const char *path, int datasync,
             struct fuse_file_info *fi);

/**
 * @brief SFUSE용 FUSE operations 테이블 반환
 */
struct fuse_operations *sfuse_get_operations(void);

#endif /* SFUSE_FS_H */
