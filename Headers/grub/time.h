/* POSIX time.h stub for GRUB freestanding environment.
 * Provides struct tm, time_t, and other POSIX time types that
 * BRLTTY headers reference. Also chains to GRUB's real
 * grub/time.h for grub_get_time_ms() and grub_millisleep(). */

#ifndef BRLTTY_GRUB_TIME_H
#define BRLTTY_GRUB_TIME_H

#include <grub/types.h>

typedef grub_int64_t time_t;

struct timespec {
  time_t tv_sec;
  long   tv_nsec;
};

struct timeval {
  time_t tv_sec;
  long   tv_usec;
};

struct tm {
  int tm_sec;
  int tm_min;
  int tm_hour;
  int tm_mday;
  int tm_mon;
  int tm_year;
  int tm_wday;
  int tm_yday;
  int tm_isdst;
};

extern struct tm *localtime (const time_t *timep);
extern struct tm *gmtime (const time_t *timep);
extern grub_size_t strftime (char *s, grub_size_t max, const char *format, const struct tm *tm);

/* Include GRUB's real grub/time.h for grub_get_time_ms() and
 * grub_millisleep(). Use relative path from Headers/grub/ to
 * grub-root/include/grub/ to avoid the -I../Headers shadowing. */
#include "../../grub-root/include/grub/time.h"

#endif /* BRLTTY_GRUB_TIME_H */
