// SPDX-License-Identifier: GPL-2.0
// SFUSE main entry with automatic format option (-F)

#include "fs.h" // fs_initialize, fs_teardown
#include <errno.h>
#include <fuse3/fuse.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Forward declaration for FUSE operations provider
struct fuse_operations *sfuse_get_operations(void);

// Force-format flag: if true, unformatted images will be initialized
bool g_force_format = false;

int main(int argc, char *argv[]) {
  // 1) Handle optional -F flag to force format
  bool force = false;
  int idx = 1;
  while (idx < argc && strcmp(argv[idx], "-F") == 0) {
    force = true;
    idx++;
  }
  g_force_format = force;

  // 2) Check required arguments after -F
  if (argc - idx < 2) {
    fprintf(stderr, "Usage: %s [-F] <image> <mountpoint> [FUSE options]\n",
            argv[0]);
    return EXIT_FAILURE;
  }

  // 3) Extract image path and mountpoint
  const char *image_path = argv[idx];
  const char *mountpoint = argv[idx + 1];
  int remaining = argc - (idx + 2);

  // 4) Initialize SFUSE filesystem
  int err = 0;
  struct sfuse_fs *fs = fs_initialize(image_path, &err);
  if (!fs) {
    if (err == EINVAL) {
      fprintf(stderr,
              "SFUSE: 이미지가 VSFS 형식이 아닙니다. 포맷하려면 -F 옵션을 "
              "사용하세요. (err=%d)\n",
              err);
    } else {
      fprintf(stderr, "SFUSE: 파일시스템 초기화 실패 (err=%d)\n", err);
    }
    return EXIT_FAILURE;
  }

  // 5) Build FUSE argument list
  int fuse_argc = 2 + remaining; // program name + mountpoint + extra options
  char **fuse_argv = malloc(sizeof(char *) * fuse_argc);
  if (!fuse_argv) {
    perror("malloc");
    fs_teardown(fs);
    return EXIT_FAILURE;
  }
  int f = 0;
  fuse_argv[f++] = strdup("sfuse");
  fuse_argv[f++] = strdup(mountpoint);
  for (int i = 0; i < remaining; ++i) {
    fuse_argv[f++] = strdup(argv[idx + 2 + i]);
  }

  struct fuse_args fuse_args = FUSE_ARGS_INIT(fuse_argc, fuse_argv);

  // 6) Run FUSE main loop
  int ret =
      fuse_main(fuse_args.argc, fuse_args.argv, sfuse_get_operations(), fs);

  // 7) Teardown and cleanup
  fs_teardown(fs);
  for (int i = 0; i < fuse_argc; ++i)
    free(fuse_argv[i]);
  free(fuse_argv);
  fuse_opt_free_args(&fuse_args);

  return ret;
}
