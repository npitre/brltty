/* Minimal sys/stat.h stub for GRUB freestanding environment.
 * GRUB has no filesystem stat operations. */

#ifndef BRLTTY_GRUB_SYS_STAT_H
#define BRLTTY_GRUB_SYS_STAT_H

typedef unsigned int mode_t;

#define S_IFMT   0170000
#define S_IFDIR  0040000
#define S_IFREG  0100000
#define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)

struct stat {
  unsigned long st_dev;
  unsigned long st_ino;
  mode_t st_mode;
  __SIZE_TYPE__ st_size;
};

static inline int stat (const char *path, struct stat *buf) {
  (void)path; (void)buf;
  return -1;
}

static inline int fstat (int fd, struct stat *buf) {
  (void)fd; (void)buf;
  return -1;
}

#endif /* BRLTTY_GRUB_SYS_STAT_H */
