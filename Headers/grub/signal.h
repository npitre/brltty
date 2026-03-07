/* Minimal signal.h stub for GRUB freestanding environment.
 * GRUB has no signal handling. */

#ifndef BRLTTY_GRUB_SIGNAL_H
#define BRLTTY_GRUB_SIGNAL_H

/* sig_atomic_t is defined in prologue.h for GRUB_RUNTIME */
typedef void (*sighandler_t)(int);

#define SIG_DFL ((sighandler_t)0)
#define SIG_IGN ((sighandler_t)1)
#define SIG_ERR ((sighandler_t)-1)

#define SIGTERM 15
#define SIGINT   2
#define SIGHUP   1
#define SIGUSR1 10
#define SIGUSR2 12
#define SIGPIPE 13
#define SIGCHLD 17
#define SIGALRM 14
#define SIGCONT 18
#define SIGIO   29

static inline sighandler_t signal (int sig, sighandler_t handler) {
  (void)sig; (void)handler;
  return SIG_ERR;
}

static inline int raise (int sig) {
  (void)sig;
  return -1;
}

/* sigset_t and related operations */
typedef unsigned long sigset_t;

#define sigemptyset(set) (*(set) = 0, 0)
#define sigfillset(set) (*(set) = ~0UL, 0)
#define sigaddset(set, sig) (*(set) |= (1UL << (sig)), 0)
#define sigdelset(set, sig) (*(set) &= ~(1UL << (sig)), 0)
#define sigismember(set, sig) ((*(set) >> (sig)) & 1)

#define SIG_BLOCK   0
#define SIG_UNBLOCK 1
#define SIG_SETMASK 2

static inline int sigprocmask (int how, const sigset_t *set, sigset_t *oldset) {
  (void)how; (void)set; (void)oldset;
  return 0;
}

#endif /* BRLTTY_GRUB_SIGNAL_H */
