/* Minimal search.h stub for GRUB freestanding environment.
 * Provides tfind/tsearch/twalk stubs. BRLTTY's brltty-lsinc.c
 * is a standalone tool not needed in GRUB, but it gets compiled. */

#ifndef BRLTTY_GRUB_SEARCH_H
#define BRLTTY_GRUB_SEARCH_H

typedef enum { preorder, postorder, endorder, leaf } VISIT;

/* These tree functions are not usable in GRUB — stub them out. */
static inline void *tfind (const void *key, void *const *rootp,
                           int (*compar)(const void *, const void *)) {
  (void)key; (void)rootp; (void)compar;
  return 0;
}

static inline void *tsearch (const void *key, void **rootp,
                             int (*compar)(const void *, const void *)) {
  (void)key; (void)rootp; (void)compar;
  return 0;
}

#endif /* BRLTTY_GRUB_SEARCH_H */
