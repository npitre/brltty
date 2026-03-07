/* Minimal fcntl.h stub for GRUB freestanding environment.
 * BRLTTY includes this for file descriptor operations that
 * are not available in GRUB. Provides only macro stubs;
 * function implementations are in system_grub.c. */

#ifndef BRLTTY_GRUB_FCNTL_H
#define BRLTTY_GRUB_FCNTL_H

#define O_RDONLY  0
#define O_WRONLY  1
#define O_RDWR   2
#define O_CREAT  0100
#define O_TRUNC  01000
#define O_APPEND 02000
#define O_ACCMODE 3

/* open() stub — fd-based I/O doesn't exist in GRUB */
static inline int open (const char *path, int flags, ...) {
  (void)path; (void)flags;
  return -1;
}

/* close() is declared as extern in prologue.h and implemented in system_grub.c */

#endif /* BRLTTY_GRUB_FCNTL_H */
