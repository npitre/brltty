/*
 * BRLTTY - A background process providing access to the console screen (when in
 *          text mode) for a blind person using a refreshable braille display.
 *
 * Copyright (C) 1995-2026 by The BRLTTY Developers.
 *
 * BRLTTY comes with ABSOLUTELY NO WARRANTY.
 *
 * This is free software, placed under the terms of the
 * GNU Lesser General Public License, as published by the Free Software
 * Foundation; either version 2.1 of the License, or (at your option) any
 * later version. Please see the file LICENSE-LGPL for details.
 *
 * Web Page: http://brltty.app/
 *
 * This software is maintained by Dave Mielke <dave@mielke.cc>.
 */

#include "prologue.h"

#include <grub/env.h>
#include <grub/misc.h>
#include <grub/mm.h>
#include <grub/time.h>
#include <grub/file.h>

#include "system.h"

/* ─── Environment variables ─────────────────────────────────────────────
 * Map to GRUB's own environment, allowing configuration via grub.cfg:
 *   set BRLTTY_BRAILLE_DRIVER=hw
 */
char *
getenv (const char *name) {
  return (char *)grub_env_get(name);
}

/* ─── Process control ──────────────────────────────────────────────────
 * GRUB has no process model.
 */
void
exit (int status) {
  grub_fatal("brltty: exit(%d)", status);
  __builtin_unreachable();
}

/* ─── String functions ─────────────────────────────────────────────────
 * Not provided by GRUB's posix_wrap.
 */
char *
strdup (const char *s) {
  grub_size_t len = grub_strlen(s) + 1;
  char *dup = grub_malloc(len);
  if (dup) grub_memcpy(dup, s, len);
  return dup;
}

char *
strtok (char *str, const char *delim) {
  static char *next;

  if (str) next = str;
  if (!next) return NULL;

  while (*next && grub_strchr(delim, *next)) next++;
  if (!*next) { next = NULL; return NULL; }

  char *start = next;

  while (*next && !grub_strchr(delim, *next)) next++;
  if (*next) *next++ = '\0';
  else next = NULL;

  return start;
}

const char *
strerror (int errnum) {
  (void)errnum;
  return grub_errmsg;
}

int
strncmp (const char *s1, const char *s2, grub_size_t n) {
  return grub_strncmp(s1, s2, n);
}

char *
strrchr (const char *s, int c) {
  const char *last = NULL;
  while (*s) {
    if (*s == (char)c) last = s;
    s++;
  }
  if ((char)c == '\0') return (char *)s;
  return (char *)last;
}

grub_size_t
strcspn (const char *s, const char *reject) {
  const char *p = s;
  while (*p) {
    if (grub_strchr(reject, *p)) break;
    p++;
  }
  return p - s;
}

grub_size_t
strspn (const char *s, const char *accept) {
  const char *p = s;
  while (*p) {
    if (!grub_strchr(accept, *p)) break;
    p++;
  }
  return p - s;
}

char *
strpbrk (const char *s, const char *accept) {
  while (*s) {
    if (grub_strchr(accept, *s)) return (char *)s;
    s++;
  }
  return NULL;
}

int
strncasecmp (const char *s1, const char *s2, grub_size_t n) {
  return grub_strncasecmp(s1, s2, n);
}

int
vsnprintf (char *str, grub_size_t size, const char *fmt, __builtin_va_list ap) {
  return grub_vsnprintf(str, size, fmt, ap);
}

int
atoi (const char *nptr) {
  return (int)grub_strtol(nptr, NULL, 10);
}

/* ─── Sorting and searching ────────────────────────────────────────────
 * Standard algorithms needed by cmdline.c and others.
 */
void
qsort (void *base, grub_size_t nmemb, grub_size_t size,
       int (*compar)(const void *, const void *)) {
  unsigned char *array = base;
  grub_size_t gap;

  for (gap = nmemb / 2; gap > 0; gap /= 2) {
    grub_size_t i;

    for (i = gap; i < nmemb; i++) {
      unsigned char tmp[size];
      grub_memcpy(tmp, array + i * size, size);

      grub_size_t j = i;
      while (j >= gap && compar(array + (j - gap) * size, tmp) > 0) {
        grub_memcpy(array + j * size, array + (j - gap) * size, size);
        j -= gap;
      }

      grub_memcpy(array + j * size, tmp, size);
    }
  }
}

void *
bsearch (const void *key, const void *base, grub_size_t nmemb,
         grub_size_t size, int (*compar)(const void *, const void *)) {
  const unsigned char *array = base;
  grub_size_t lo = 0;
  grub_size_t hi = nmemb;

  while (lo < hi) {
    grub_size_t mid = lo + (hi - lo) / 2;
    int cmp = compar(key, array + mid * size);

    if (cmp < 0) hi = mid;
    else if (cmp > 0) lo = mid + 1;
    else return (void *)(array + mid * size);
  }

  return NULL;
}

/* ─── Time functions ───────────────────────────────────────────────────
 * Minimal stubs — GRUB has grub_get_time_ms() for real timing.
 */
static struct tm grub_static_tm;

time_t
time (time_t *tloc) {
  time_t t = (time_t)(grub_get_time_ms() / 1000);
  if (tloc) *tloc = t;
  return t;
}

struct tm *
localtime (const time_t *timep) {
  grub_memset(&grub_static_tm, 0, sizeof(grub_static_tm));
  (void)timep;
  return &grub_static_tm;
}

struct tm *
gmtime (const time_t *timep) {
  return localtime(timep);
}

grub_size_t
strftime (char *s, grub_size_t max, const char *format, const struct tm *tm) {
  (void)format; (void)tm;
  if (max > 0) s[0] = '\0';
  return 0;
}

/* ─── POSIX I/O stubs ─────────────────────────────────────────────────
 * Minimal FILE operations for code that must compile but won't
 * do real file I/O yet. Real GRUB file access uses grub_file_*.
 */
typedef struct grub_file *FILE_ptr;

FILE_ptr stdin = NULL;
FILE_ptr stdout = NULL;
FILE_ptr stderr = NULL;

FILE_ptr
fopen (const char *path, const char *mode) {
  (void)mode;
  grub_file_t f = grub_file_open(path, GRUB_FILE_TYPE_CAT);
  return (FILE_ptr)f;
}

int
fclose (FILE_ptr stream) {
  if (stream) grub_file_close((grub_file_t)stream);
  return 0;
}

grub_size_t
fread (void *ptr, grub_size_t size, grub_size_t nmemb, FILE_ptr stream) {
  if (!stream) return 0;
  grub_ssize_t ret = grub_file_read((grub_file_t)stream, ptr, size * nmemb);
  if (ret < 0) return 0;
  return (grub_size_t)ret / size;
}

grub_size_t
fwrite (const void *ptr, grub_size_t size, grub_size_t nmemb, FILE_ptr stream) {
  (void)ptr; (void)size; (void)stream;
  /* GRUB has no general file write capability */
  return nmemb;
}

int
fprintf (FILE_ptr stream, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  /* Route stderr/stdout to GRUB console, ignore others */
  if (stream == stdout || stream == stderr || stream == NULL) {
    /* grub_vprintf doesn't exist in all versions, use grub_printf via buffer */
    char buf[256];
    int ret = grub_vsnprintf(buf, sizeof(buf), fmt, ap);
    grub_printf("%s", buf);
    va_end(ap);
    return ret;
  }
  va_end(ap);
  return 0;
}

int
feof (FILE_ptr stream) {
  if (!stream) return 1;
  grub_file_t f = (grub_file_t)stream;
  return (f->offset >= f->size);
}

int
ferror (FILE_ptr stream) {
  (void)stream;
  return (grub_errno != GRUB_ERR_NONE);
}

int
fflush (FILE_ptr stream) {
  (void)stream;
  return 0;
}

int
fgetc (FILE_ptr stream) {
  unsigned char c;
  if (fread(&c, 1, 1, stream) == 1) return c;
  return -1; /* EOF */
}

char *
fgets (char *s, int size, FILE_ptr stream) {
  int i;
  for (i = 0; i < size - 1; i++) {
    int c = fgetc(stream);
    if (c == -1) {
      if (i == 0) return NULL;
      break;
    }
    s[i] = (char)c;
    if (c == '\n') { i++; break; }
  }
  s[i] = '\0';
  return s;
}

int
fputs (const char *s, FILE_ptr stream) {
  if (stream == stdout || stream == stderr || stream == NULL) {
    grub_printf("%s", s);
  }
  return 0;
}

/* ─── POSIX I/O (fd-based) ────────────────────────────────────────────
 * File descriptors don't exist in GRUB.
 */
int
close (int fd) {
  (void)fd;
  return -1;
}

grub_ssize_t
write (int fd, const void *buf, grub_size_t count) {
  (void)fd;
  /* If fd 1 or 2 (stdout/stderr), print to console */
  if (fd == 1 || fd == 2) {
    const char *s = buf;
    grub_size_t i;
    for (i = 0; i < count; i++) grub_printf("%c", s[i]);
    return (grub_ssize_t)count;
  }
  return -1;
}

grub_ssize_t
read (int fd, void *buf, grub_size_t count) {
  (void)fd; (void)buf; (void)count;
  return -1;
}

/* ─── Command-line parsing ────────────────────────────────────────────
 * Minimal getopt() — just enough for brlttyConstruct()'s option parsing.
 * Handles short options with and without required arguments (e.g. "-l debug").
 * Does not handle optional arguments, option bundling, or "--" termination.
 */
char *optarg = NULL;
int optind = 1;
int opterr = 1;
int optopt = '?';

int
getopt (int argc, char *const argv[], const char *optstring) {
  const char *p;

  optarg = NULL;

  if (optind >= argc || argv[optind] == NULL)
    return -1;

  if (argv[optind][0] != '-' || argv[optind][1] == '\0')
    return -1;

  optopt = argv[optind][1];

  p = grub_strchr(optstring, optopt);
  if (p == NULL) {
    optind++;
    return '?';
  }

  if (p[1] == ':') {
    /* Option takes an argument. */
    if (argv[optind][2] != '\0') {
      /* Argument attached: -lDEBUG */
      optarg = (char *)&argv[optind][2];
      optind++;
    } else if (optind + 1 < argc) {
      /* Argument is the next argv element: -l DEBUG */
      optind++;
      optarg = (char *)argv[optind];
      optind++;
    } else {
      optind++;
      return '?';
    }
  } else {
    optind++;
  }

  return optopt;
}

/* ─── Locale stubs ────────────────────────────────────────────────────
 * GRUB has no locale support.
 */
char *
setlocale (int category, const char *locale) {
  (void)category; (void)locale;
  return (char *)"C";
}

int
fputc (int c, FILE_ptr stream) {
  if (stream == stdout || stream == stderr || stream == NULL) {
    grub_printf("%c", c);
    return c;
  }
  return -1;
}

int
fileno (FILE_ptr stream) {
  (void)stream;
  return -1;
}

int
vprintf (const char *fmt, __builtin_va_list ap) {
  char buf[256];
  int ret = grub_vsnprintf(buf, sizeof(buf), fmt, ap);
  grub_printf("%s", buf);
  return ret;
}

void
srand (unsigned int seed) {
  (void)seed;
}

int
unlink (const char *path) {
  (void)path;
  return -1;
}

int
rename (const char *oldpath, const char *newpath) {
  (void)oldpath; (void)newpath;
  return -1;
}

int
pipe (int pipefd[2]) {
  (void)pipefd;
  return -1;
}

FILE_ptr
fdopen (int fd, const char *mode) {
  (void)fd; (void)mode;
  return NULL;
}

FILE_ptr
freopen (const char *path, const char *mode, FILE_ptr stream) {
  (void)path; (void)mode; (void)stream;
  return NULL;
}

int
setvbuf (FILE_ptr stream, char *buf, int mode, grub_size_t size) {
  (void)stream; (void)buf; (void)mode; (void)size;
  return 0;
}

int
select (int nfds, fd_set *r, fd_set *w, fd_set *e, struct timeval *t) {
  (void)nfds; (void)r; (void)w; (void)e; (void)t;
  return -1;
}

void
initializeSystemObject (void) {
}
