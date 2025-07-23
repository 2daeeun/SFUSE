// File: src/ops.c
#define _GNU_SOURCE
#include <fcntl.h>

#include "bitmap.h"
#include "block.h"
#include "dir.h"
#include "fs.h"
#include "inode.h"
#include <dirent.h>
#include <errno.h>
#include <fuse3/fuse.h>
#include <stdio.h>
#include <stdlib.h>
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
  uint32_t ino;
  struct sfuse_inode inode;

  int res = fs_getattr(fs, path, stbuf);
  if (res < 0)
    return res;

  res = fs_resolve_path(fs, path, &ino);
  if (res < 0)
    return res;

  // inode_load 호출 수정 (정확한 매개변수 전달)
  res = inode_load(fs->backing_fd, &fs->sb, ino, &inode);
  if (res < 0)
    return res;

  // inode 번호 설정
  stbuf->st_ino = ino;

  // 디렉터리 및 파일 링크 수 설정 (기본값 유지)
  if (S_ISDIR(stbuf->st_mode))
    stbuf->st_nlink = 2;
  else
    stbuf->st_nlink = 1;

  // 블록 크기 및 블록 수 설정
  stbuf->st_blksize = SFUSE_BLOCK_SIZE;
  stbuf->st_blocks = (stbuf->st_size + SFUSE_BLOCK_SIZE - 1) / SFUSE_BLOCK_SIZE;

  return 0;
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
  struct sfuse_fs *fs = fuse_get_context()->private_data;
  uint32_t ino;
  int res = fs_resolve_path(fs, path, &ino);
  if (res < 0)
    return res;

  fi->fh = ino;

  // 명확한 direct_io 처리 추가
  if (fi->flags & O_DIRECT) {
    fi->direct_io = 1;
  }

  return 0;
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

  printf("[DEBUG] sfuse_write_cb(path=%s, size=%zu, offset=%lld)\n", path, size,
         (long long)offset);

  int res = fs_write(fs, path, buf, size, offset);

  printf("[DEBUG] sfuse_write_cb -> res=%d\n", res);
  return res;
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

  // 파일 생성
  int ret = fs_create(fs, path, mode, fi);
  if (ret < 0)
    return ret;

  // fs_create 내부에서 fi->fh를 설정했다고 가정
  // 만약 fs_create가 fi->fh를 설정하지 않았다면, 여기서 직접 inode 번호를
  // 찾아야 함 안전하게 보장하려면 아래를 사용:
  uint32_t ino;
  if (fs_resolve_path(fs, path, &ino) == 0)
    fi->fh = ino;

  return 0;
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
  struct sfuse_fs *fs = fuse_get_context()->private_data;
  uint32_t ino = fi->fh;

  // dirty 상태인 inode 디스크 동기화
  int ret = inode_sync_if_dirty(fs->backing_fd, &fs->sb, ino);
  if (ret < 0)
    return ret;

  // 데이터 동기화
  if (fsync(fs->backing_fd) < 0)
    return -errno;

  return 0;
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
  struct sfuse_fs *fs = fuse_get_context()->private_data;
  uint32_t ino = fi->fh;

  // dirty 상태인 inode 디스크 동기화
  int ret = inode_sync_if_dirty(fs->backing_fd, &fs->sb, ino);
  if (ret < 0)
    return ret;

  if (datasync) {
    if (fdatasync(fs->backing_fd) < 0)
      return -errno;
  } else {
    if (fsync(fs->backing_fd) < 0)
      return -errno;
  }

  return 0;
}

// getxattr, listxattr, setxattr, removexattr 지원하지 않음 명시적 처리

static int sfuse_getxattr_cb(const char *path, const char *name, char *value,
                             size_t size) {
  (void)path;
  (void)value;
  (void)size;

  if (strcmp(name, "security.capability") == 0)
    return -ENODATA; // filebench 기대값

  return -ENODATA;
}

static int sfuse_listxattr_cb(const char *path, char *list, size_t size) {
  (void)path;
  if (list == NULL)
    return 0;
  if (size > 0)
    list[0] = '\0';
  return 0;
}

static int sfuse_setxattr_cb(const char *path, const char *name,
                             const char *value, size_t size, int flags) {
  return -EOPNOTSUPP;
}

static int sfuse_removexattr_cb(const char *path, const char *name) {
  return -EOPNOTSUPP;
}

// 파일 시스템 통계(statfs) 반환 콜백
static int sfuse_statfs_cb(const char *path, struct statvfs *stbuf) {
  struct sfuse_fs *fs = (struct sfuse_fs *)fuse_get_context()->private_data;
  memset(stbuf, 0, sizeof(struct statvfs));
  stbuf->f_bsize = SFUSE_BLOCK_SIZE;
  stbuf->f_frsize = SFUSE_BLOCK_SIZE;
  stbuf->f_blocks = fs->sb.total_blocks;
  stbuf->f_bfree = fs->sb.free_blocks;
  stbuf->f_bavail = fs->sb.free_blocks;
  stbuf->f_files = fs->sb.total_inodes;
  stbuf->f_ffree = fs->sb.free_inodes;
  stbuf->f_favail = fs->sb.free_inodes;
  stbuf->f_namemax = SFUSE_NAME_MAX;
  return 0;
}

static void *sfuse_init_cb(struct fuse_conn_info *conn,
                           struct fuse_config *cfg) {
  (void)cfg;

  // writeback_cache 지원 활성화
  if (conn->capable & FUSE_CAP_WRITEBACK_CACHE)
    conn->want |= FUSE_CAP_WRITEBACK_CACHE;

  return fuse_get_context()->private_data;
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

  // 새로 추가된 콜백
  ops.getxattr = sfuse_getxattr_cb;
  ops.listxattr = sfuse_listxattr_cb;
  ops.setxattr = sfuse_setxattr_cb;
  ops.removexattr = sfuse_removexattr_cb;
  ops.statfs = sfuse_statfs_cb;
  ops.init = sfuse_init_cb; // init에서 write-back 캐시 활성화

  return &ops;
}
