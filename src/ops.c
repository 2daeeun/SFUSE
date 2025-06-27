// File: src/ops.c

#include "fs.h"
#include <dirent.h>
#include <errno.h>
#include <fuse3/fuse.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

/**
 * @brief 파일/디렉터리 속성 조회 콜백
 *
 * @param path 조회할 경로 문자열
 * @param stbuf 결과를 저장할 stat 구조체 포인터
 * @param fi    FUSE 파일 정보 구조체 (사용되지 않음)
 * @return fs_getattr 반환값
 */
static int sfuse_getattr_cb(const char *path, struct stat *stbuf,
                            struct fuse_file_info *fi) {
  (void)fi;
  struct sfuse_fs *fs = (struct sfuse_fs *)fuse_get_context()->private_data;
  return fs_getattr(fs, path, stbuf);
}

/**
 * @brief access 콜백
 *
 * @param path 검사할 경로 문자열
 * @param mask 접근 마스크 (R_OK, W_OK, X_OK)
 * @return fs_access 반환값
 */
static int sfuse_access_cb(const char *path, int mask) {
  struct sfuse_fs *fs = fuse_get_context()->private_data;
  return fs_access(fs, path, mask);
}

/**
 * @brief 디렉터리 목록 조회 콜백
 *
 * @param path    디렉터리 경로 문자열
 * @param buf     FUSE가 제공하는 버퍼 포인터
 * @param filler  디렉터리 엔트리 추가 콜백
 * @param offset  읽기 시작 오프셋
 * @param fi      FUSE 파일 정보 구조체 (사용되지 않음)
 * @param flags   readdir 플래그 (사용되지 않음)
 * @return fs_readdir 반환값
 */
static int sfuse_readdir_cb(const char *path, void *buf, fuse_fill_dir_t filler,
                            off_t offset, struct fuse_file_info *fi,
                            enum fuse_readdir_flags flags) {
  (void)fi;
  (void)flags;
  struct sfuse_fs *fs = (struct sfuse_fs *)fuse_get_context()->private_data;
  return fs_readdir(fs, path, buf, filler, offset);
}

/**
 * @brief 파일 열기 콜백
 *
 * @param path 열 경로 문자열
 * @param fi   FUSE 파일 정보 구조체
 * @return fs_open 반환값
 */
static int sfuse_open_cb(const char *path, struct fuse_file_info *fi) {
  struct sfuse_fs *fs = (struct sfuse_fs *)fuse_get_context()->private_data;
  return fs_open(fs, path, fi);
}

/**
 * @brief 파일 읽기 콜백
 *
 * @param path   읽을 파일 경로 문자열
 * @param buf    데이터를 저장할 버퍼 포인터
 * @param size   요청할 바이트 수
 * @param offset 파일 내 읽기 시작 오프셋
 * @param fi     FUSE 파일 정보 구조체 (사용되지 않음)
 * @return fs_read 반환값
 */
static int sfuse_read_cb(const char *path, char *buf, size_t size, off_t offset,
                         struct fuse_file_info *fi) {
  struct sfuse_fs *fs = (struct sfuse_fs *)fuse_get_context()->private_data;
  return fs_read(fs, path, buf, size, offset);
}

/**
 * @brief 파일 쓰기 콜백
 *
 * @param path   쓸 파일 경로 문자열
 * @param buf    기록할 데이터 버퍼 포인터
 * @param size   기록할 바이트 수
 * @param offset 파일 내 쓰기 시작 오프셋
 * @param fi     FUSE 파일 정보 구조체 (사용되지 않음)
 * @return fs_write 반환값
 */
static int sfuse_write_cb(const char *path, const char *buf, size_t size,
                          off_t offset, struct fuse_file_info *fi) {
  struct sfuse_fs *fs = (struct sfuse_fs *)fuse_get_context()->private_data;
  return fs_write(fs, path, buf, size, offset);
}

/**
 * @brief 파일 생성 콜백
 *
 * @param path 생성할 파일 경로 문자열
 * @param mode 생성할 파일 모드
 * @param fi   FUSE 파일 정보 구조체
 * @return fs_create 반환값
 */
static int sfuse_create_cb(const char *path, mode_t mode,
                           struct fuse_file_info *fi) {
  struct sfuse_fs *fs = (struct sfuse_fs *)fuse_get_context()->private_data;
  return fs_create(fs, path, mode, fi);
}

/**
 * @brief 디렉터리 생성 콜백
 *
 * @param path 생성할 디렉터리 경로 문자열
 * @param mode 생성할 디렉터리 모드
 * @return fs_mkdir 반환값
 */
static int sfuse_mkdir_cb(const char *path, mode_t mode) {
  struct sfuse_fs *fs = (struct sfuse_fs *)fuse_get_context()->private_data;
  return fs_mkdir(fs, path, mode);
}

/**
 * @brief 파일 삭제 콜백
 *
 * @param path 삭제할 파일 경로 문자열
 * @return fs_unlink 반환값
 */
static int sfuse_unlink_cb(const char *path) {
  struct sfuse_fs *fs = (struct sfuse_fs *)fuse_get_context()->private_data;
  return fs_unlink(fs, path);
}

/**
 * @brief 디렉터리 삭제 콜백
 *
 * @param path 삭제할 디렉터리 경로 문자열
 * @return fs_rmdir 반환값
 */
static int sfuse_rmdir_cb(const char *path) {
  struct sfuse_fs *fs = (struct sfuse_fs *)fuse_get_context()->private_data;
  return fs_rmdir(fs, path);
}

/**
 * @brief 이름 변경 콜백
 *
 * @param from 원본 경로 문자열
 * @param to   대상 경로 문자열
 * @param flags 리네임 플래그 (사용되지 않음)
 * @return fs_rename 반환값
 */
static int sfuse_rename_cb(const char *from, const char *to,
                           unsigned int flags) {
  (void)flags;
  struct sfuse_fs *fs = (struct sfuse_fs *)fuse_get_context()->private_data;
  return fs_rename(fs, from, to);
}

/**
 * @brief 파일 크기 변경 콜백
 *
 * @param path 대상 파일 경로 문자열
 * @param size 새 크기 (바이트 단위)
 * @param fi   FUSE 파일 정보 구조체 (사용되지 않음)
 * @return fs_truncate 반환값
 */
static int sfuse_truncate_cb(const char *path, off_t size,
                             struct fuse_file_info *fi) {
  (void)fi;
  struct sfuse_fs *fs = (struct sfuse_fs *)fuse_get_context()->private_data;
  return fs_truncate(fs, path, size);
}

/**
 * @brief 파일/디렉터리 타임스탬프 갱신 콜백
 *
 * @param path 대상 경로 문자열
 * @param tv   tv[0]=접근 시간, tv[1]=수정 시간
 * @param fi   FUSE 파일 정보 구조체 (사용되지 않음)
 * @return fs_utimens 반환값
 */
static int sfuse_utimens_cb(const char *path, const struct timespec tv[2],
                            struct fuse_file_info *fi) {
  (void)fi;
  struct sfuse_fs *fs = (struct sfuse_fs *)fuse_get_context()->private_data;
  return fs_utimens(fs, path, tv);
}

/**
 * @brief flush 콜백
 *
 * @param path 대상 경로 문자열 (사용되지 않음)
 * @param fi   FUSE 파일 정보 구조체
 * @return fs_flush 반환값
 */
static int sfuse_flush_cb(const char *path, struct fuse_file_info *fi) {
  (void)path;
  struct sfuse_fs *fs = (struct sfuse_fs *)fuse_get_context()->private_data;
  return fs_flush(fs, path, fi);
}

/**
 * @brief fsync 콜백
 *
 * @param path    대상 경로 문자열 (사용되지 않음)
 * @param datasync 데이터만 동기화할지 여부
 * @param fi       FUSE 파일 정보 구조체
 * @return fs_fsync 반환값
 */
static int sfuse_fsync_cb(const char *path, int datasync,
                          struct fuse_file_info *fi) {
  (void)path;
  struct sfuse_fs *fs = (struct sfuse_fs *)fuse_get_context()->private_data;
  return fs_fsync(fs, path, datasync, fi);
}

/**
 * @brief FUSE operations 테이블 반환
 *
 * 생성된 콜백 함수들을 ops 구조체에 등록하여 반환한다.
 *
 * @return 설정된 fuse_operations 구조체 포인터
 */
struct fuse_operations *sfuse_get_operations(void) {
  static struct fuse_operations ops;
  memset(&ops, 0, sizeof(ops));
  ops.getattr = sfuse_getattr_cb;
  ops.access = sfuse_access_cb;
  ops.readdir = sfuse_readdir_cb;
  ops.open = sfuse_open_cb;
  ops.read = sfuse_read_cb;
  ops.write = sfuse_write_cb;
  ops.create = sfuse_create_cb;
  ops.mkdir = sfuse_mkdir_cb;
  ops.unlink = sfuse_unlink_cb;
  ops.rmdir = sfuse_rmdir_cb;
  ops.rename = sfuse_rename_cb;
  ops.truncate = sfuse_truncate_cb;
  ops.flush = sfuse_flush_cb;
  ops.fsync = sfuse_fsync_cb;
  ops.utimens = sfuse_utimens_cb;
  return &ops;
}
