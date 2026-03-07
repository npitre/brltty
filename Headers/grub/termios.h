/* Minimal termios.h stub for GRUB freestanding environment.
 * GRUB has no terminal control interface. */

#ifndef BRLTTY_GRUB_TERMIOS_H
#define BRLTTY_GRUB_TERMIOS_H

typedef unsigned int tcflag_t;
typedef unsigned char cc_t;
typedef unsigned int speed_t;

#define NCCS 32

struct termios {
  tcflag_t c_iflag;
  tcflag_t c_oflag;
  tcflag_t c_cflag;
  tcflag_t c_lflag;
  cc_t c_cc[NCCS];
};

/* c_iflag bits */
#define IGNBRK  0000001
#define BRKINT  0000002
#define IGNPAR  0000004
#define PARMRK  0000010
#define INPCK   0000020
#define ISTRIP  0000040
#define INLCR   0000100
#define IGNCR   0000200
#define ICRNL   0000400
#define IXON    0002000
#define IXOFF   0010000

/* c_cflag bits */
#define CSIZE   0000060
#define CS8     0000060
#define CSTOPB  0000100
#define CREAD   0000200
#define PARENB  0000400
#define HUPCL   0002000
#define CLOCAL  0004000

/* c_lflag bits */
#define ISIG    0000001
#define ICANON  0000002
#define ECHO    0000010
#define ECHOE   0000020
#define ECHOK   0000040
#define ECHONL  0000100
#define IEXTEN  0100000

/* c_cc indexes */
#define VMIN    6
#define VTIME   5
#define VINTR   0
#define VEOF    4

/* tcsetattr actions */
#define TCSANOW   0
#define TCSADRAIN 1
#define TCSAFLUSH 2

#ifndef _POSIX_VDISABLE
#define _POSIX_VDISABLE 0
#endif

static inline int tcgetattr (int fd, struct termios *t) {
  (void)fd; (void)t;
  return -1;
}

static inline int tcsetattr (int fd, int action, const struct termios *t) {
  (void)fd; (void)action; (void)t;
  return -1;
}

static inline speed_t cfgetispeed (const struct termios *t) {
  (void)t;
  return 0;
}

static inline int cfsetispeed (struct termios *t, speed_t speed) {
  (void)t; (void)speed;
  return 0;
}

#endif /* BRLTTY_GRUB_TERMIOS_H */
