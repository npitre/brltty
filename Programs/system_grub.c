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

#include "system.h"

/* Environment variables — map to GRUB's own environment.
 * This allows configuration via grub.cfg:
 *   set BRLTTY_BRAILLE_DRIVER=hw
 */
char *
getenv (const char *name) {
  return (char *)grub_env_get(name);
}

/* Process control — GRUB has no process model. */
void
exit (int status) {
  grub_fatal("brltty: exit(%d)", status);
  __builtin_unreachable();
}

/* String functions not provided by GRUB's posix_wrap. */
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

  /* skip leading delimiters */
  while (*next && grub_strchr(delim, *next)) next++;
  if (!*next) { next = NULL; return NULL; }

  char *start = next;

  /* find end of token */
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

/* Sorting and searching — standard algorithms needed by cmdline.c. */
void
qsort (void *base, grub_size_t nmemb, grub_size_t size,
       int (*compar)(const void *, const void *)) {
  /* Shell sort — simple, no recursion, no extra allocation. */
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

void
initializeSystemObject (void) {
}
