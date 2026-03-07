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

#ifndef BRLTTY_INCLUDED_PROLOGUE
#define BRLTTY_INCLUDED_PROLOGUE

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#undef CAN_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP
#if defined(__clang__) || (defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)))
#define CAN_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP
#endif /* #pragma GCC diagnostic push/pop */

#undef HAVE_BUILTIN_POPCOUNT
#undef HAVE_SYNC_SYNCHRONIZE

#ifdef __has_builtin
#if __has_builtin(__builtin_popcount)
#define HAVE_BUILTIN_POPCOUNT
#endif /* __has_builtin(__builtin_popcount) */

#if __has_builtin(__sync_synchronize)
#define HAVE_SYNC_SYNCHRONIZE
#endif /* __has_builtin(__sync_synchronize) */
#endif /* __has_builtin */

#ifndef HAVE_SYNC_SYNCHRONIZE
static inline void __sync_synchronize (void) {}
#endif /* HAVE_SYNC_SYNCHRONIZE */

#define CONCATENATE_1(a,b) a##b
#define CONCATENATE(a,b) CONCATENATE_1(a,b)

#define STRINGIFY_1(a) #a
#define STRINGIFY(a) STRINGIFY_1(a)

// only use in the global context
#define NULL_TERMINATED_STRING_ARRAY(...) (const char *const []){__VA_ARGS__, NULL}

#define MIN(a, b)  (((a) < (b))? (a): (b)) 
#define MAX(a, b)  (((a) > (b))? (a): (b)) 

#define ARRAY_COUNT(array) (sizeof((array)) / sizeof((array)[0]))
#define ARRAY_SIZE(pointer, count) ((count) * sizeof(*(pointer)))

#define IS_WITHIN_RANGE(index,start,end) (((index) >= (start)) && ((index) < (end)))
#define IS_WITHIN_BOUNDS(index,count) IS_WITHIN_RANGE((index), 0, (count))

#define SYMBOL_TYPE(name) name ## _t
#define SYMBOL_POINTER(name) static SYMBOL_TYPE(name) *name##_p = NULL;

#define VARIABLE_DECLARATION(variableName, variableType) \
  variableType variableName
#define VARIABLE_TYPEDEF(variableName, variableType) \
  typedef VARIABLE_DECLARATION(SYMBOL_TYPE(variableName), variableType)
#define VARIABLE_DECLARE(variableName, variableType) \
  VARIABLE_TYPEDEF(variableName, variableType); \
  extern SYMBOL_TYPE(variableName) variableName

#define FUNCTION_DECLARATION(functionName, returnType, argumentList) \
  returnType functionName argumentList
#define FUNCTION_TYPEDEF(functionName, returnType, argumentList) \
  typedef FUNCTION_DECLARATION(SYMBOL_TYPE(functionName), returnType, argumentList)
#define FUNCTION_DECLARE(functionName, returnType, argumentList) \
  FUNCTION_TYPEDEF(functionName, returnType, argumentList); \
  extern SYMBOL_TYPE(functionName) functionName

#ifdef HAVE_CONFIG_H
#ifdef FOR_BUILD
#include "forbuild.h"
#else /* FOR_BUILD */
#include "config.h"
#endif /* FOR_BUILD */
#endif /* HAVE_CONFIG_H */

#ifdef __ANDROID__
#ifndef __ANDROID_API__
#define __ANDROID_API__ 19
#endif /* __ANDROID_API__ */
#endif /* __ANDROID__ */

#if defined(__CYGWIN__) || defined(__MINGW32__)
#define WINDOWS

#ifndef HAVE_SDKDDKVER_H
#include <w32api.h>

#ifndef _WIN32_WINNT_NT4
#define _WIN32_WINNT_NT4 WindowsNT4
#endif /* _WIN32_WINNT_NT4 */

#ifndef _WIN32_WINNT_WIN95
#define _WIN32_WINNT_WIN95 Windows95
#endif /* _WIN32_WINNT_WIN95 */

#ifndef _WIN32_WINNT_WIN98
#define _WIN32_WINNT_WIN98 Windows98
#endif /* _WIN32_WINNT_WIN98 */

#ifndef _WIN32_WINNT_WINME
#define _WIN32_WINNT_WINME WindowsME
#endif /* _WIN32_WINNT_WINME */

#ifndef _WIN32_WINNT_WIN2K
#define _WIN32_WINNT_WIN2K Windows2000
#endif /* _WIN32_WINNT_WIN2K */

#ifndef _WIN32_WINNT_WINXP
#define _WIN32_WINNT_WINXP WindowsXP
#endif /* _WIN32_WINNT_WINXP */

#ifndef _WIN32_WINNT_WS03
#define _WIN32_WINNT_WS03 Windows2003
#endif /* _WIN32_WINNT_WS03 */

#ifndef _WIN32_WINNT_VISTA
#define _WIN32_WINNT_VISTA WindowsVista
#endif /* _WIN32_WINNT_VISTA */
#endif /* HAVE_SDKDDKVER_H */

#ifndef _WIN32_WINNT
#define _WIN32_WINNT _WIN32_WINNT_WINXP
#endif /* _WIN32_WINNT */

#ifndef WINVER
#define WINVER _WIN32_WINNT
#endif /* WINVER */
#endif /* WINDOWS */

#ifdef WINDOWS
#ifdef __MINGW32__
#ifndef __MINGW64__
#ifndef __USE_W32_SOCKETS
#define __USE_W32_SOCKETS
#endif /* __USE_W32_SOCKETS */
#endif /* __MINGW64__ */

#include <winsock2.h>
#include <ws2tcpip.h>
#endif /* __MINGW32__ */

#include <windows.h>
#include <winerror.h>

#ifdef __MINGW32__
#ifndef __MINGW64__
#include <_mingw.h>
#endif /* __MINGW64__ */
#endif /* __MINGW32__ */
#endif /* WINDOWS */

/*
 * The (poorly named) macro "interface" is unfortunately defined within
 * Windows headers. Fortunately, though, it's also only ever used within
 * them so it's safe to undefine it here.
 */
#ifdef interface
#undef interface
#endif /* interface */

#include <sys/types.h>
#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include <inttypes.h>

#ifdef __MINGW32__
#if (__MINGW32_MAJOR_VERSION < 3) || ((__MINGW32_MAJOR_VERSION == 3) && (__MINGW32_MINOR_VERSION < 10))
extern int gettimeofday (struct timeval *tvp, void *tzp);
#endif /* gettimeofday */

#if !defined(__MINGW64_VERSION_MAJOR) && ((__MINGW32_MAJOR_VERSION < 3) || ((__MINGW32_MAJOR_VERSION == 3) && (__MINGW32_MINOR_VERSION < 15)))
extern void usleep (int usec);
#endif /* usleep */
#endif /* __MINGW32__ */

#ifdef __MINGW64__
#ifdef __clang__
static inline int 
ffs (int i) {
  if (i == 0) return 0;
  int bit = 1;

  while (!(i & 1)) {
    i >>= 1;
    bit += 1;
  }

  return bit;
}
#endif /* __clang__ */
#endif /* __MINGW64__ */

#ifdef GRUB_RUNTIME
#undef NESTED_FUNC_ATTR
#define NESTED_FUNC_ATTR __attribute__((__regparm__(1)))

/* missing needed standard integer definitions */
#define INT16_MAX 0X7FFF
#define UINT16_MAX 0XFFFF
#define UINT8_MAX 0XFF
#define INT32_MAX 0X7FFFFFFF
#define UINT16_C(i) ((uint16_t)(i))
#define UINT32_C(i) (i ## U)
#define UINT64_C(i) (i ## ULL)
#define UINTMAX_C(i) (i ## ULL)
#define PRIuGRUB_UINT16_T "u"
#define PRIuGRUB_UINT8_T "u"

/* inttypes.h format macros — GRUB's posix_wrap inttypes.h doesn't provide these */
#define PRId8  "d"
#define PRIu8  "u"
#define PRIx8  "x"
#define PRIX8  "X"
#define PRId16 "d"
#define PRIu16 "u"
#define PRIx16 "x"
#define PRIX16 "X"
#define PRId32 "d"
#define PRIu32 "u"
#define PRIx32 "x"
#define PRIX32 "X"
#define PRId64 "lld"
#define PRIu64 "llu"
#define PRIx64 "llx"
#define PRIX64 "llX"
#define PRIdMAX "lld"
#define PRIuMAX "llu"
#define PRIxMAX "llx"
#define PRIdPTR __INTPTR_FMTd__
#define PRIuPTR __INTPTR_FMTu__
#define PRIxPTR __INTPTR_FMTx__

/* missing errno codes — map to GRUB error codes */
#ifndef ENOENT
#define ENOENT GRUB_ERR_FILE_NOT_FOUND
#endif
#ifndef ENOSYS
#define ENOSYS GRUB_ERR_NOT_IMPLEMENTED_YET
#endif
#ifndef EAGAIN
#define EAGAIN GRUB_ERR_TIMEOUT
#endif
#ifndef EIO
#define EIO GRUB_ERR_IO
#endif
#ifndef ENODEV
#define ENODEV GRUB_ERR_UNKNOWN_DEVICE
#endif
#ifndef EBUSY
#define EBUSY GRUB_ERR_FILE_READ_ERROR
#endif
#ifndef EACCES
#define EACCES GRUB_ERR_ACCESS_DENIED
#endif
#ifndef EEXIST
#define EEXIST GRUB_ERR_FILE_NOT_FOUND
#endif
#ifndef EINTR
#define EINTR GRUB_ERR_TIMEOUT
#endif
#ifndef EROFS
#define EROFS GRUB_ERR_ACCESS_DENIED
#endif
#ifndef INT16_MIN
#define INT16_MIN (-32768)
#endif

/* to get gettext() declared */
#define GRUB_POSIX_GETTEXT_DOMAIN "brltty"

/* disable the use of floating-point operations */
#define NO_FLOAT

/* Types used by various BRLTTY source files */
typedef __PTRDIFF_TYPE__ off_t;
typedef int pid_t;
typedef unsigned int uid_t;
typedef unsigned int gid_t;
typedef unsigned long ino_t;
typedef __INTPTR_TYPE__ intptr_t;
typedef volatile int sig_atomic_t;

/* ffs — find first set bit (used by brl_dots.h) */
#define ffs(x) __builtin_ffs(x)

/* POSIX string functions not in GRUB's posix_wrap */
extern int strncmp (const char *s1, const char *s2, __SIZE_TYPE__ n);
extern char *strrchr (const char *s, int c);
extern __SIZE_TYPE__ strcspn (const char *s, const char *reject);
extern __SIZE_TYPE__ strspn (const char *s, const char *accept);
extern int vsnprintf (char *str, __SIZE_TYPE__ size, const char *fmt, __builtin_va_list ap);
extern char *strpbrk (const char *s, const char *accept);
extern int strncasecmp (const char *s1, const char *s2, __SIZE_TYPE__ n);
extern int atoi (const char *nptr);

/* POSIX functions provided by system_grub.c */
extern char *getenv (const char *name);
extern void exit (int status) __attribute__((noreturn));
extern char *strdup (const char *s);
extern char *strtok (char *str, const char *delim);
extern const char *strerror (int errnum);
extern void qsort (void *base, __SIZE_TYPE__ nmemb, __SIZE_TYPE__ size,
                   int (*compar)(const void *, const void *));
extern void *bsearch (const void *key, const void *base,
                      __SIZE_TYPE__ nmemb, __SIZE_TYPE__ size,
                      int (*compar)(const void *, const void *));

/* I/O stubs — GRUB's posix_wrap stdio.h defines FILE but not these */
typedef struct grub_file *FILE_ptr;
extern int fclose (FILE_ptr stream);
extern FILE_ptr fopen (const char *path, const char *mode);
extern __SIZE_TYPE__ fread (void *ptr, __SIZE_TYPE__ size, __SIZE_TYPE__ nmemb, FILE_ptr stream);
extern __SIZE_TYPE__ fwrite (const void *ptr, __SIZE_TYPE__ size, __SIZE_TYPE__ nmemb, FILE_ptr stream);
extern int fprintf (FILE_ptr stream, const char *fmt, ...) __attribute__((format(__printf__, 2, 3)));
extern int feof (FILE_ptr stream);
extern int ferror (FILE_ptr stream);
extern int fflush (FILE_ptr stream);
extern int fgetc (FILE_ptr stream);
extern char *fgets (char *s, int size, FILE_ptr stream);
extern int fputs (const char *s, FILE_ptr stream);
extern int fputc (int c, FILE_ptr stream);
extern int fileno (FILE_ptr stream);

/* Standard streams — defined in system_grub.c */
extern FILE_ptr stdin;
extern FILE_ptr stdout;
extern FILE_ptr stderr;

/* POSIX I/O */
extern int close (int fd);
extern __PTRDIFF_TYPE__ write (int fd, const void *buf, __SIZE_TYPE__ count);
extern __PTRDIFF_TYPE__ read (int fd, void *buf, __SIZE_TYPE__ count);

/* Time functions — stubs in system_grub.c */
typedef long time_t;
extern time_t time (time_t *tloc);

/* Command-line parsing stubs */
extern int getopt (int argc, char *const argv[], const char *optstring);
extern char *optarg;
extern int optind, opterr, optopt;

/* Locale stubs */
#define LC_ALL 0
#define LC_CTYPE 0
extern char *setlocale (int category, const char *locale);

/* Additional POSIX stubs */
extern int vprintf (const char *fmt, __builtin_va_list ap);
extern void srand (unsigned int seed);
extern int unlink (const char *path);
extern int rename (const char *oldpath, const char *newpath);
extern int pipe (int pipefd[2]);
extern FILE_ptr fdopen (int fd, const char *mode);
extern FILE_ptr freopen (const char *path, const char *mode, FILE_ptr stream);

/* Constants */
#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

/* stdio buffering constants */
#define _IONBF 2
#define _IOLBF 1
#define _IOFBF 0
extern int setvbuf (FILE_ptr stream, char *buf, int mode, __SIZE_TYPE__ size);

/* select() stubs — GRUB has no fd multiplexing */
typedef struct { unsigned long fds_bits[1]; } fd_set;
#define FD_ZERO(set) ((set)->fds_bits[0] = 0)
#define FD_SET(fd, set) ((set)->fds_bits[0] |= (1UL << (fd)))
#define FD_CLR(fd, set) ((set)->fds_bits[0] &= ~(1UL << (fd)))
#define FD_ISSET(fd, set) ((set)->fds_bits[0] & (1UL << (fd)))
struct timeval;
extern int select (int nfds, fd_set *r, fd_set *w, fd_set *e, struct timeval *t);
#ifndef UINT32_MAX
#define UINT32_MAX 0XFFFFFFFFU
#endif
#ifndef EPIPE
#define EPIPE GRUB_ERR_IO
#endif
typedef unsigned long dev_t;

/* Format macros that depend on __INTPTR_FMTd__ */
#ifndef __INTPTR_FMTd__
#define PRIdPTR "ld"
#undef PRIuPTR
#define PRIuPTR "lu"
#undef PRIxPTR
#define PRIxPTR "lx"
#endif
#define PRIXPTR PRIxPTR
#define PRIi32 "d"

/* GRUB's ARRAY_SIZE takes 1 argument (element count of a static array),
 * BRLTTY's takes 2 arguments (byte size = count * sizeof(*pointer)).
 * Use a variadic macro to support both calling conventions, since GRUB
 * headers may be included later and use the 1-arg form. */
#undef ARRAY_SIZE
#define ARRAY_SIZE_1(array) (sizeof(array) / sizeof((array)[0]))
#define ARRAY_SIZE_2(pointer, count) ((count) * sizeof(*(pointer)))
#define ARRAY_SIZE_SELECT(_1, _2, NAME, ...) NAME
#define ARRAY_SIZE(...) ARRAY_SIZE_SELECT(__VA_ARGS__, ARRAY_SIZE_2, ARRAY_SIZE_1)(__VA_ARGS__)

/* GRUB's posix_wrap provides wchar.h but configure's test may fail
 * in the freestanding environment. Force HAVE_WCHAR_H so prologue.h
 * includes wchar.h instead of defining wcs* as conflicting macros. */
#ifndef HAVE_WCHAR_H
#define HAVE_WCHAR_H 1
#endif
#endif /* GRUB_RUNTIME */

#if defined(__MSDOS__)
#undef WCHAR_MAX

#elif defined(HAVE_WCHAR_H)
#include <wchar.h>
#include <wctype.h>
/* GRUB's wchar.h doesn't define WCHAR_MAX — ensure it's set so
 * the #ifdef WCHAR_MAX block below is entered instead of the
 * #else block which would redefine mbrtowc/wcrtomb as stubs. */
#if defined(GRUB_RUNTIME) && !defined(WCHAR_MAX)
#define WCHAR_MAX 0x7FFFFFFF
#endif
#endif /* HAVE_WCHAR_H */

#ifdef __MSDOS__
#include <stdarg.h>

extern int snprintf (char *str, size_t size, const char *format, ...);
extern int vsnprintf (char *str, size_t size, const char *format, va_list ap);

#define lstat(file_name, buf) stat(file_name, buf)
#endif /* __MSDOS__ */

#ifdef __MINGW32__
typedef HANDLE FileDescriptor;
#define INVALID_FILE_DESCRIPTOR INVALID_HANDLE_VALUE
#define PRIfd "p"
#define closeFileDescriptor(fd) CloseHandle(fd)

typedef SOCKET SocketDescriptor;
#define INVALID_SOCKET_DESCRIPTOR INVALID_SOCKET
#define PRIsd "d"
#define closeSocketDescriptor(sd) closesocket(sd)
#else /* __MINGW32__ */
typedef int FileDescriptor;
#define INVALID_FILE_DESCRIPTOR -1
#define PRIfd "d"
#define closeFileDescriptor(fd) close(fd)

typedef int SocketDescriptor;
#define INVALID_SOCKET_DESCRIPTOR -1
#define PRIsd "d"
#define closeSocketDescriptor(sd) close(sd)
#endif /* __MINGW32__ */

#ifdef WINDOWS
#define getSystemError() GetLastError()

#ifdef __CYGWIN__
#include <sys/cygwin.h>

#define getSocketError() errno
#define setErrno(error) errno = cygwin_internal(CW_GET_ERRNO_FROM_WINERROR, (error))
#else /* __CYGWIN__ */
#define getSocketError() WSAGetLastError()
#define setErrno(error) errno = win_toErrno((error))

#ifndef WIN_ERRNO_STORAGE_CLASS
#define WIN_ERRNO_STORAGE_CLASS
extern
#endif /* WIN_ERRNO_STORAGE_CLASS */
WIN_ERRNO_STORAGE_CLASS int win_toErrno (DWORD error);
#endif /* __CYGWIN__ */

#else /* WINDOWS */
#define getSystemError() errno
#define getSocketError() errno

#define setErrno(error)
#endif /* WINDOWS */

#define setSystemErrno() setErrno(getSystemError())
#define setSocketErrno() setErrno(getSocketError())

#if defined(__MINGW64__)
#define PRIsize "llu"
#define PRIssize "lld"

#elif defined(__MINGW32__)
#define PRIsize "u"
#define PRIssize "d"

#else /* format for size_t and ssize_t */
#define PRIsize "zu"
#define PRIssize "zd"
#endif /* format for size_t and ssize_t */

#if defined(__CYGWIN__)
#define PRIkey "llX"
#elif defined(__FreeBSD__)
#define PRIkey "lX"
#elif defined(__OpenBSD__)
#define PRIkey "lX"
#else /* format for key_t */
#define PRIkey PRIX32
#endif /* format for key_t */

#undef WCSTOK_HAS_END_ARGUMENT
#if !(defined(__MINGW32__) && defined(__i386__))
#define WCSTOK_HAS_END_ARGUMENT
#endif /* WCSTOK_HAS_END_ARGUMENT */

#ifdef WCHAR_MAX
#define WC_C(wc) L##wc
#define WS_C(ws) L##ws
#define PRIwc "lc"
#define PRIws "ls"
#define iswLatin1(wc) ((wc) < 0X100)

#ifdef GRUB_RUNTIME
/* GRUB's wchar.h provides mbrtowc/wcrtomb/wcscoll but not the
 * full set of wide-char functions. Provide simple versions that
 * handle ASCII/Latin-1 (sufficient for GRUB's use case). */
#include <string.h>
#include <ctype.h>

#define WEOF ((wint_t)-1)

#define iswcntrl(wc) iscntrl((int)(wc))
#define iswalpha(wc) isalpha((int)(wc))
#define iswdigit(wc) isdigit((int)(wc))
#define iswalnum(wc) isalnum((int)(wc))
#define iswgraph(wc) isgraph((int)(wc))
#define iswlower(wc) islower((int)(wc))
#define iswprint(wc) isprint((int)(wc))
#define iswpunct(wc) ispunct((int)(wc))
#define iswspace(wc) isspace((int)(wc))
#define iswupper(wc) isupper((int)(wc))
#define iswxdigit(wc) isxdigit((int)(wc))

#define towlower(wc) tolower((int)(wc))
#define towupper(wc) toupper((int)(wc))

static inline size_t wcslen (const wchar_t *s) {
  size_t n = 0; while (s[n]) n++; return n;
}
static inline wchar_t *wmemcpy (wchar_t *d, const wchar_t *s, size_t n) {
  size_t i; for (i = 0; i < n; i++) d[i] = s[i]; return d;
}
static inline wchar_t *wmemmove (wchar_t *d, const wchar_t *s, size_t n) {
  if (d < s) { size_t i; for (i = 0; i < n; i++) d[i] = s[i]; }
  else if (d > s) { size_t i = n; while (i--) d[i] = s[i]; }
  return d;
}
static inline wchar_t *wmemset (wchar_t *s, wchar_t c, size_t n) {
  size_t i; for (i = 0; i < n; i++) s[i] = c; return s;
}
static inline int wmemcmp (const wchar_t *s1, const wchar_t *s2, size_t n) {
  size_t i; for (i = 0; i < n; i++) {
    if (s1[i] != s2[i]) return (s1[i] < s2[i]) ? -1 : 1;
  }
  return 0;
}
static inline wchar_t *wmemchr (const wchar_t *s, wchar_t c, size_t n) {
  size_t i; for (i = 0; i < n; i++) if (s[i] == c) return (wchar_t *)(s + i);
  return 0;
}
static inline int wcsncmp (const wchar_t *s1, const wchar_t *s2, size_t n) {
  size_t i; for (i = 0; i < n && s1[i] && s2[i]; i++) {
    if (s1[i] != s2[i]) return (s1[i] < s2[i]) ? -1 : 1;
  }
  if (i == n) return 0;
  return (s1[i] < s2[i]) ? -1 : (s1[i] > s2[i]) ? 1 : 0;
}
static inline wchar_t *wcschr (const wchar_t *s, wchar_t c) {
  while (*s) { if (*s == c) return (wchar_t *)s; s++; }
  return (c == 0) ? (wchar_t *)s : 0;
}
static inline wchar_t *wcsrchr (const wchar_t *s, wchar_t c) {
  const wchar_t *last = 0;
  while (*s) { if (*s == c) last = s; s++; }
  return (wchar_t *)((c == 0) ? s : last);
}
static inline wchar_t *wcscpy (wchar_t *d, const wchar_t *s) {
  wchar_t *r = d; while ((*d++ = *s++)); return r;
}
static inline wchar_t *wcsncpy (wchar_t *d, const wchar_t *s, size_t n) {
  size_t i; for (i = 0; i < n && s[i]; i++) d[i] = s[i];
  for (; i < n; i++) d[i] = 0;
  return d;
}
static inline wchar_t *wcstok (wchar_t *s, const wchar_t *d, wchar_t **p) {
  if (s) *p = s;
  if (!*p) return 0;
  wchar_t *start = *p;
  while (*start && wcschr(d, *start)) start++;
  if (!*start) { *p = 0; return 0; }
  wchar_t *end = start;
  while (*end && !wcschr(d, *end)) end++;
  if (*end) { *end = 0; *p = end + 1; } else *p = 0;
  return start;
}
static inline int swprintf (wchar_t *s, size_t n, const wchar_t *fmt, ...) {
  if (n > 0) s[0] = 0;
  (void)fmt;
  return 0;
}
static inline wint_t fgetwc (void *stream) {
  (void)stream;
  return WEOF;
}
#endif /* GRUB_RUNTIME */

#else /* HAVE_WCHAR_H */
#include <string.h>
#include <ctype.h>

#define wchar_t unsigned char
#define wint_t int

#define WEOF EOF
#define WCHAR_MAX UINT8_MAX

#define wmemchr(source,character,count) memchr((const char *)(source), (char)(character), (count))
#define wmemcmp(source1,source2,count) memcmp((const char *)(source1), (const char *)(source2), (count))
#define wmemcpy(target,source,count) memcpy((char *)(target), (const char *)(source), (count))
#define wmemmove(target,source,count) memmove((char *)(target), (const char *)(source), (count))
#define wmemset(target,character,count) memset((char *)(target), (char)(character), (count))

#define wcscasecmp(source1,source2) strcasecmp((const char *)(source1), (const char *)(source2))
#define wcsncasecmp(source1,source2,count) strncasecmp((const char *)(source1), (const char *)(source2), (count))
#define wcscat(target,source) strcat((char *)(target), (const char *)(source))
#define wcsncat(target,source,count) strncat((char *)(target), (const char *)(source), (count))
#define wcscmp(source1,source2) strcmp((const char *)(source1), (const char *)(source2))
#define wcsncmp(source1,source2,count) strncmp((const char *)(source1), (const char *)(source2), (count))
#define wcscpy(target,source) strcpy((char *)(target), (const char *)(source))
#define wcsncpy(target,source,count) strncpy((char *)(target), (const char *)(source), (count))
#define wcslen(source) strlen((const char *)(source))
#define wcsnlen(source,count) strnlen((const char *)(source), (count))

#define wcschr(source,character) ((wchar_t *)strchr((const char *)(source), (char)(character)))
#define wcscoll(source1,source2) strcoll((const char *)(source1), (const char *)(source2))
#define wcscspn(source,reject) strcspn((const char *)(source), (const char *)(reject))
#define wcsdup(source) strdup((const char *)(source))
#define wcspbrk(source,accept) strpbrk((const char *)(source), (const char *)(accept))
#define wcsrchr(source,character) strrchr((const char *)(source), (char)(character))
#define wcsspn(source,accept) strspn((const char *)(source), (const char *)(accept))
#define wcsstr(source,substring) strstr((const char *)(source), (const char *)(substring))
#define wcswcs(source,substring) strstr((const char *)(source), (const char *)(substring))
#define wcsxfrm(target,source,count) strxfrm((char *)(target), (const char *)(source), (count))
#define wcstoul(nptr, endptr, base) strtoul(((const char *)(nptr)), ((char **)(endptr)), (base))

#define wcstol(source,end,base) strtol((const char *)(source), (char **)(end), (base))
#define wcstoll(source,end,base) strtoll((const char *)(source), (char **)(end), (base))

#define wcstok(target,delimiters) ((wchar_t *)strtok(((char *)(target)), ((const char *)(delimiters))))
#undef WCSTOK_HAS_END_ARGUMENT

#define iswalnum(character) isalnum((int)(character))
#define iswalpha(character) isalpha((int)(character))
#define iswblank(character) isblank((int)(character))
#define iswcntrl(character) iscntrl((int)(character))
#define iswdigit(character) isdigit((int)(character))
#define iswgraph(character) isgraph((int)(character))
#define iswlower(character) islower((int)(character))
#define iswprint(character) isprint((int)(character))
#define iswpunct(character) ispunct((int)(character))
#define iswspace(character) isspace((int)(character))
#define iswupper(character) isupper((int)(character))
#define iswxdigit(character) isxdigit((int)(character))

#define towlower(character) tolower((int)(character))
#define towupper(character) toupper((int)(character))

#define swprintf(target,count,source,...) snprintf((char *)(target), (count), (const char *)(source), ## __VA_ARGS__)
#define vswprintf(target,count,source,args) vsnprintf((char *)(target), (count), (const char *)(source), (args))

typedef unsigned char mbstate_t;

static inline size_t
mbrtowc (wchar_t *pwc, const char *s, size_t n, mbstate_t *ps) {
  if (!s) return 0;
  if (!n) return 0;
  if (pwc) *pwc = *s & 0XFF;
  if (!*s) return 0;
  return 1;
}

static inline size_t
wcrtomb (char *s, wchar_t wc, mbstate_t *ps) {
  if (s) *s = wc;
  return 1;
}

static inline int
mbsinit (const mbstate_t *ps) {
  return 1;
}

#define WC_C(wc) ((wchar_t)wc)
#define WS_C(ws) ((const wchar_t *)ws)
#define PRIwc "c"
#define PRIws "s"
#define iswLatin1(wc) (1)
#endif /* HAVE_WCHAR_H */

#ifdef WORDS_BIGENDIAN
#define CHARSET_ENDIAN_SUFFIX "BE"
#else /* WORDS_BIGENDIAN */
#define CHARSET_ENDIAN_SUFFIX "LE"
#endif /* WORDS_BIGENDIAN */

#define WCHAR_CHARSET ("UCS-" SIZEOF_WCHAR_T_STR CHARSET_ENDIAN_SUFFIX)

#ifndef HAVE_MEMPCPY
static inline void *
mempcpy (void *dest, const void *src, size_t size) {
  extern void *memcpy (void *dest, const void *src, size_t size);
  char *address = memcpy(dest, src, size);
  return address + size;
}
#endif /* HAVE_MEMPCPY */

#ifndef HAVE_WMEMPCPY
#define wmempcpy(dest,src,count) (wmemcpy((dest), (src), (count)) + (count))
#endif /* HAVE_WMEMPCPY */

#ifndef WRITABLE_DIRECTORY
#define WRITABLE_DIRECTORY ""
#endif /* WRITABLE_DIRECTORY */

#ifdef HAVE_VAR_ATTRIBUTE_PACKED
#define PACKED __attribute__((packed))
#else /* HAVE_VAR_ATTRIBUTE_PACKED */
#define PACKED
#endif /* HAVE_VAR_ATTRIBUTE_PACKED */

#ifdef HAVE_FUNC_ATTRIBUTE_FORMAT
#define PRINTF(fmt,var) __attribute__((format(__printf__, fmt, var)))
#else /* HAVE_FUNC_ATTRIBUTE_FORMAT */
#define PRINTF(fmt,var)
#endif /* HAVE_FUNC_ATTRIBUTE_FORMAT */

#ifdef HAVE_FUNC_ATTRIBUTE_FORMAT_ARG
#define FORMAT_ARG(n) __attribute__((format_arg((n))))
#else /* HAVE_FUNC_ATTRIBUTE_FORMAT_ARG */
#define FORMAT_ARG(n)
#endif /* HAVE_FUNC_ATTRIBUTE_FORMAT_ARG */

#ifdef HAVE_FUNC_ATTRIBUTE_NORETURN
#define NORETURN __attribute__((noreturn))
#else /* HAVE_FUNC_ATTRIBUTE_NORETURN */
#define NORETURN
#endif /* HAVE_FUNC_ATTRIBUTE_NORETURN */

#ifdef HAVE_FUNC_ATTRIBUTE_UNUSED
#define UNUSED __attribute__((unused))
#else /* HAVE_FUNC_ATTRIBUTE_UNUSED */
#define UNUSED
#endif /* HAVE_FUNC_ATTRIBUTE_UNUSED */

#ifdef ENABLE_I18N_SUPPORT
#include <libintl.h>
#else /* ENABLE_I18N_SUPPORT */
extern char *gettext (const char *text) FORMAT_ARG(1);

extern char *ngettext (
  const char *singular, const char *plural, unsigned long int count
) FORMAT_ARG(1) FORMAT_ARG(2);
#endif /* ENABLE_I18N_SUPPORT */
#define strtext(string) string

#ifndef USE_PKG_BEEP_NONE
#define HAVE_BEEP_SUPPORT
#endif /* USE_PKG_BEEP_NONE */

#ifndef USE_PKG_PCM_NONE
#define HAVE_PCM_SUPPORT
#endif /* USE_PKG_PCM_NONE */

#ifndef USE_PKG_MIDI_NONE
#define HAVE_MIDI_SUPPORT
#endif /* USE_PKG_MIDI_NONE */

#ifndef USE_PKG_FM_NONE
#define HAVE_FM_SUPPORT
#endif /* USE_PKG_FM_NONE */

/* configure is still making a few mistakes with respect to the grub environment */
#ifdef GRUB_RUNTIME

/* some headers exist but probably shouldn't */
#undef HAVE_SIGNAL_H

/* localtime_r doesn't exist in GRUB — let timing.c define an inline
 * wrapper around our localtime() stub. */
#undef HAVE_DECL_LOCALTIME_R
#define HAVE_DECL_LOCALTIME_R 0

/* AC_CHECK_FUNC() is checking local libraries - these are the errors that matter */
#undef HAVE_FCHDIR
#undef HAVE_SELECT
#endif /* GRUB_RUNTIME */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* BRLTTY_INCLUDED_PROLOGUE */
