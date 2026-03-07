/* Minimal sys/ioctl.h stub for GRUB freestanding environment.
 * GRUB has no ioctl operations. */

#ifndef BRLTTY_GRUB_SYS_IOCTL_H
#define BRLTTY_GRUB_SYS_IOCTL_H

#define TIOCGWINSZ 0x5413

struct winsize {
  unsigned short ws_row;
  unsigned short ws_col;
  unsigned short ws_xpixel;
  unsigned short ws_ypixel;
};

static inline int ioctl (int fd, unsigned long request, ...) {
  (void)fd; (void)request;
  return -1;
}

#endif /* BRLTTY_GRUB_SYS_IOCTL_H */
