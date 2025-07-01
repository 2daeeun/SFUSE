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
    fprintf(stderr, "Usage: %s <device> <mountpoint> [options]\n", argv[0]);
    return EXIT_FAILURE;
  }

  const char *dev_path = argv[1];
  const char *mountpoint = argv[2];

  int backing_fd = open(dev_path, O_RDWR | O_SYNC);
  if (backing_fd < 0) {
    perror("[SFUSE] Failed to open block device");
    return EXIT_FAILURE;
  }

  struct stat st;
  if (fstat(backing_fd, &st) < 0 || !S_ISBLK(st.st_mode)) {
    fprintf(stderr, "[SFUSE] %s is not a block device\n", dev_path);
    close(backing_fd);
    return EXIT_FAILURE;
  }

  struct sfuse_fs *fs = malloc(sizeof(*fs));
  if (!fs) {
    perror("[SFUSE] Failed to allocate fs context");
    close(backing_fd);
    return EXIT_FAILURE;
  }
  memset(fs, 0, sizeof(*fs));
  fs->backing_fd = backing_fd;

  struct fuse_args args = FUSE_ARGS_INIT(0, NULL);
  fuse_opt_add_arg(&args, argv[0]);
  fuse_opt_add_arg(&args, mountpoint);

  // 필수 FUSE 옵션 추가 (fsname과 subtype은 필수!)
  fuse_opt_add_arg(&args, "-o");
  char fsname[256];
  snprintf(fsname, sizeof(fsname), "fsname=%s", dev_path);
  fuse_opt_add_arg(&args, fsname);
  fuse_opt_add_arg(&args, "-o");
  fuse_opt_add_arg(&args, "subtype=sfuse");
  fuse_opt_add_arg(&args, "-o");
  fuse_opt_add_arg(&args, "allow_other");
  fuse_opt_add_arg(&args, "-o");
  fuse_opt_add_arg(&args, "default_permissions");

  int ret = fuse_main(args.argc, args.argv, &sfuse_ops, fs);
  fuse_opt_free_args(&args);
  close(backing_fd);
  free(fs);
  return ret;
}
