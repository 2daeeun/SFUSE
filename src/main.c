#include "fs.h"
#include "ops.h"
#include <errno.h>
#include <fcntl.h>
#include <fuse.h>
#include <fuse_opt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
  if (argc < 3) {
    fprintf(stderr, "사용법: %s <device> <mountpoint> [options]\n", argv[0]);
    return EXIT_FAILURE;
  }

  const char *dev_path = argv[1];
  const char *mountpoint = argv[2];

  int backing_fd = open(dev_path, O_RDWR | O_SYNC);
  if (backing_fd < 0) {
    perror("디바이스 열기 실패");
    return EXIT_FAILURE;
  }

  struct stat st;
  if (fstat(backing_fd, &st) < 0 || !S_ISBLK(st.st_mode)) {
    fprintf(stderr, "%s는 올바른 블록 디바이스가 아닙니다.\n", dev_path);
    close(backing_fd);
    return EXIT_FAILURE;
  }

  struct sfuse_fs *fs = calloc(1, sizeof(*fs));
  if (!fs) {
    perror("메모리 할당 실패");
    close(backing_fd);
    return EXIT_FAILURE;
  }
  fs->backing_fd = backing_fd;

  struct fuse_args args = FUSE_ARGS_INIT(0, NULL);
  fuse_opt_add_arg(&args, argv[0]);    // 프로그램 이름
  fuse_opt_add_arg(&args, mountpoint); // 마운트 포인트

  // 명령줄에서 전달된 옵션 정확히 FUSE로 전달
  for (int i = 3; i < argc; i++) {
    fuse_opt_add_arg(&args, argv[i]);
  }

  // 명확히 필요한 기본 옵션 추가
  fuse_opt_add_arg(&args, "-o");
  fuse_opt_add_arg(&args, "allow_other");
  fuse_opt_add_arg(&args, "-o");
  fuse_opt_add_arg(&args, "default_permissions");

  // 디버깅 옵션을 항상 켜고 싶다면(선택 사항)
  // fuse_opt_add_arg(&args, "-d");

  int ret = fuse_main(args.argc, args.argv, &sfuse_ops, fs);

  fuse_opt_free_args(&args);
  close(backing_fd);
  free(fs);

  return ret;
}
