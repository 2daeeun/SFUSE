// main.c: SFUSE 메인 진입점 (수정본)
#include "fs.h"
#include "ops.h" // sfuse_ops 선언
#include <errno.h>
#include <fcntl.h>
#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
  struct fuse_args args = FUSE_ARGS_INIT(argc, argv);

  // 블록 디바이스 모드와 블록 크기(4KB) 강제 설정
  fuse_opt_add_arg(&args, "-o");
  fuse_opt_add_arg(&args, "fuseblk");
  fuse_opt_add_arg(&args, "-o");
  fuse_opt_add_arg(&args, "blksize=4096");

  // 사용법 검사: 최소 인자로 디바이스와 마운트 포인트 필요
  if (args.argc < 3) {
    fprintf(stderr, "Usage: %s <device> <mountpoint> [FUSE options]\n",
            argv[0]);
    return EXIT_FAILURE;
  }

  const char *dev_path = args.argv[1];
  int backing_fd = open(dev_path, O_RDWR | O_SYNC);
  if (backing_fd < 0) {
    perror("[SFUSE] Failed to open block device");
    return EXIT_FAILURE;
  }

  // 블록 디바이스 여부 확인
  struct stat st;
  if (fstat(backing_fd, &st) < 0 || !S_ISBLK(st.st_mode)) {
    fprintf(stderr, "[SFUSE] %s is not a block device\n", dev_path);
    close(backing_fd);
    return EXIT_FAILURE;
  }

  // 파일 시스템 초기화: 슈퍼블록 로드/포맷, 비트맵/아이노드 테이블 초기화
  if (fs_initialize(backing_fd) < 0) {
    fprintf(stderr, "[SFUSE] FS initialization failed on %s\n", dev_path);
    close(backing_fd);
    return EXIT_FAILURE;
  }

  // FUSE 메인 루프 진입 (sfuse_ops는 ops.c에서 정의됨)
  struct sfuse_fs *fs = get_fs_context();
  fs->backing_fd = backing_fd;
  int ret = fuse_main(args.argc, args.argv, &sfuse_ops, fs);

  // 자원 정리
  close(backing_fd);
  fuse_opt_free_args(&args);
  return ret;
}
