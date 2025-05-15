// File: include/fs.h
#ifndef SFUSE_FS_H
#define SFUSE_FS_H

#ifndef FUSE_USE_VERSION
#define FUSE_USE_VERSION 31
#endif
#include "super.h"
#include <fuse.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

/* 다른 파일에서 정의된 구조체들을 미리 선언 */
struct sfuse_bitmaps;
struct sfuse_inode_block;

/* 파일 시스템 컨텍스트 구조체 */
struct sfuse_fs {
  int backing_fd;              /* 디스크 이미지 파일의 파일 디스크립터 */
  struct sfuse_superblock sb;  /* 메모리에 로드된 슈퍼블록 */
  struct sfuse_bitmaps *bmaps; /* 메모리에 로드된 inode/블록 비트맵 */
  struct sfuse_inode_block *inode_table; /* 메모리에 로드된 inode 테이블 */
};

/* 파일 시스템 초기화 및 해제 함수 */
struct sfuse_fs *fs_initialize(const char *image_path,
                               int *error_out); /* 파일 시스템 초기화 */
void fs_teardown(struct sfuse_fs *fs);          /* 파일 시스템 자원 해제 */
int fs_resolve_path(struct sfuse_fs *fs, const char *path,
                    uint32_t *out_ino); /* 경로를 inode 번호로 해석 */

/* FUSE 핸들러에서 호출하는 고수준 파일 시스템 연산 함수 */
int fs_getattr(struct sfuse_fs *fs, const char *path,
               struct stat *stbuf); /* 파일 속성 조회 */
int fs_readdir(struct sfuse_fs *fs, const char *path, void *buf,
               fuse_fill_dir_t filler, off_t offset); /* 디렉터리 읽기 */
int fs_open(struct sfuse_fs *fs, const char *path,
            struct fuse_file_info *fi); /* 파일 열기 */
int fs_read(struct sfuse_fs *fs, const char *path, char *buf, size_t size,
            off_t offset); /* 파일 읽기 */
int fs_write(struct sfuse_fs *fs, const char *path, const char *buf,
             size_t size, off_t offset); /* 파일 쓰기 */
int fs_create(struct sfuse_fs *fs, const char *path, mode_t mode,
              struct fuse_file_info *fi); /* 새 파일 생성 */
int fs_mkdir(struct sfuse_fs *fs, const char *path,
             mode_t mode);                            /* 디렉터리 생성 */
int fs_unlink(struct sfuse_fs *fs, const char *path); /* 파일 삭제 */
int fs_rmdir(struct sfuse_fs *fs, const char *path);  /* 디렉터리 삭제 */
int fs_rename(struct sfuse_fs *fs, const char *from,
              const char *to); /* 파일/디렉터리 이름 변경 */
int fs_truncate(struct sfuse_fs *fs, const char *path,
                off_t size); /* 파일 크기 조정 */
int fs_utimens(struct sfuse_fs *fs, const char *path,
               const struct timespec tv[2]); /* 파일 시간 정보 변경 */
int fs_flush(struct sfuse_fs *fs, const char *path,
             struct fuse_file_info *fi); /* 파일 flush (버퍼 비움) */
int fs_fsync(struct sfuse_fs *fs, const char *path, int datasync,
             struct fuse_file_info *fi); /* 파일 동기화 */

/* FUSE operation 테이블 반환 */
struct fuse_operations *sfuse_get_operations(void);

#endif /* SFUSE_FS_H */
