# BRLTTY as a GRUB Loadable Module — Design Document

## 1. Overview

This document describes the design for converting BRLTTY's existing GRUB support
from an incomplete, build-into-GRUB approach into a proper GRUB loadable module
(`.mod`). The goal is to provide braille display accessibility during the GRUB
boot menu, distributable by distros without patching GRUB.

### 1.1 Current State

BRLTTY has partial GRUB support consisting of platform backend files and a build
configuration script:

| File | Purpose | Status |
|------|---------|--------|
| `Programs/serial_grub.c` | Serial port I/O via GRUB's serial API | Pre-existing |
| `Programs/serial_grub.h` | Type mappings (BRLTTY types to GRUB types) | Pre-existing |
| `Programs/usb_grub.c` | USB backend via GRUB's USB API | Implemented |
| `Programs/dynld_grub.c` | Dynamic loading via GRUB's `grub_dl_*` functions | Pre-existing |
| `Programs/ports_grub.c` | I/O port access via `grub_inb`/`grub_outb` | Pre-existing |
| `Programs/charset_grub.c` | Minimal character set handling | Pre-existing |
| `Programs/grub_module.c` | GRUB module entry point (`GRUB_MOD_INIT`/`FINI`) | Implemented |
| `Programs/timing.c` | Time/timer support (GRUB paths) | Pre-existing |
| `Drivers/Screen/Grub/screen.c` | Screen driver with shadow terminal | Implemented |
| `cfg-grub` | Build configuration wrapper script | Updated |
| `configure.ac` | GRUB platform detection, compiler flags | Updated |

### 1.2 Target Architecture

```
                    GRUB loadable module (.mod)
                    ┌─────────────────────────────────┐
                    │  GRUB_MOD_INIT / GRUB_MOD_FINI  │
                    │                                  │
  Screen capture    │  Shadow Terminal ◄── grub_term   │  Braille I/O
  ┌─────────────┐   │  (text buffer)     output hook   │  ┌──────────────┐
  │ BRLTTY Grub │◄──│──────────────────────────────────│──│ BRLTTY USB   │
  │ Screen Drv  │   │                                  │  │ (usb_grub.c) │
  └─────────────┘   │  BRLTTY Core                     │  └──────┬───────┘
                    │  (navigation, key tables,        │         │
                    │   command processing)             │  ┌──────▼───────┐
  Key injection     │                                  │  │ GRUB USB API │
  ┌─────────────┐   │  grub_term_input ◄── braille    │  │ bulk_read()  │
  │ GRUB menu   │◄──│──────────────── key translation  │  │ bulk_write() │
  │ navigation  │   │                                  │  │ control_msg()│
  └─────────────┘   └─────────────────────────────────┘  └──────────────┘
```

Deployment:
- Drop `brltty.mod` into `/boot/grub/x86_64-efi/` (or `i386-pc/`)
- Preload via `GRUB_PRELOAD_MODULES="brltty"` in `/etc/default/grub`
- Or load on-demand: `insmod brltty` from GRUB command line


## 2. GRUB Module API

GRUB modules are relocatable ELF binaries with a standard structure:

```c
#include <grub/dl.h>
#include <grub/command.h>
#include <grub/term.h>

GRUB_MOD_LICENSE("GPLv3+");

GRUB_MOD_INIT(brltty)
{
    // Register terminal I/O, commands, etc.
}

GRUB_MOD_FINI(brltty)
{
    // Cleanup
}
```

### 2.1 Relevant GRUB Subsystem APIs

| Header | API | Used for |
|--------|-----|----------|
| `grub/term.h` | `grub_term_register_output/input` | Terminal interception & key injection |
| `grub/usb.h` | `grub_usb_iterate`, `grub_usb_bulk_read/write`, `grub_usb_control_msg` | USB braille device I/O |
| `grub/serial.h` | `grub_serial_find`, `grub_serial_register` | Serial braille devices |
| `grub/time.h` | `grub_get_time_ms`, `grub_millisleep` | Timing and delays |
| `grub/mm.h` | `grub_malloc`, `grub_free` | Memory management |
| `grub/misc.h` | String/memory operations | Utilities |
| `grub/command.h` | `grub_register_command` | GRUB commands |
| `grub/dl.h` | `grub_dl_load`, `grub_get_symbol` | Dynamic loading |
| `grub/cpu/io.h` | `grub_inb`, `grub_outb` | Hardware port access |

### 2.2 Build Requirements

There is no `grub-devel` package. Modules must be built against a **configured**
GRUB source tree. The GRUB source must have `./configure` run (but not
necessarily `make`) to generate:

- `config.h` — included by `grub/types.h`
- `include/grub/cpu` symlink — points to the target CPU's header directory
  (e.g., `grub/x86_64` or `grub/i386`)
- `include/grub/machine` symlink — points to the platform-specific headers
  (e.g., `grub/x86_64/efi`)

The GRUB source can be obtained from:
- `git clone https://git.savannah.gnu.org/git/grub.git`
- Or via distro SRPM: `dnf download --source grub2`

See `Documents/README.Grub` for detailed build instructions.

### 2.3 Module Build Process

GRUB modules go through a multi-step build:

1. **Compile** — Source files are compiled with `-ffreestanding -nostdinc
   -nostdlib` plus GRUB header include paths. The native system GCC is used
   with `-m32` (for `i386-pc`/`i386-efi`) or `-m64` (for `x86_64-efi`).
   No cross-compiler is required.

2. **Link** — Object files are linked with `-nostdlib -Wl,-r` into a
   relocatable ELF object (`.module`).

3. **Post-process** — GRUB's `genmod.sh` adds `.modname` and `.moddeps`
   ELF sections, then strips to keep only `grub_mod_init` and
   `grub_mod_fini` symbols, producing the final `.mod`.

### 2.4 Compiler Flags

The `elf*` host case in `configure.ac` sets:

```
CPPFLAGS += -DGRUB_RUNTIME -DGRUB_FILE=__FILE__ -DNESTED_FUNC_ATTR=
            -ffreestanding -nostdinc -nostdlib
            -isystem $(gcc -print-file-name=include)
            -I grub-root/grub-core/lib/posix_wrap
            -I grub-root/include
```

Key points:
- `-nostdinc` removes all default system include paths
- `-isystem` adds back GCC's own built-in headers (`stdint.h`, `stddef.h`, etc.)
- GRUB's `posix_wrap` directory provides minimal POSIX header stubs
  (`limits.h`, `string.h`, `stdlib.h`, etc.)
- GRUB's `include` directory provides the GRUB API headers
- `-DGRUB_RUNTIME` gates GRUB-specific code paths throughout BRLTTY

### 2.5 Platform Configuration

The `cfg-grub` script handles platform selection:

- **Auto-detection**: scans `/boot/grub*/` for platform directories
- **Explicit override**: `--with-grub-platform=x86_64-efi` (or `i386-pc`,
  `i386-efi`)
- Distro packagers should always specify the platform explicitly, since they
  build for multiple targets

The platform determines:
- Compiler flags: `-m64` for `x86_64-*`, `-m32` for `i386-*`
- Which GRUB headers are used (pointer sizes, ABI)

All braille drivers are built as internal (statically linked) because GRUB
has no shared library loader. The `--enable-standalone-programs` flag
prevents any attempt to load `.so` files at runtime.


## 3. Screen Capture — Shadow Terminal

### 3.1 Problem

GRUB's terminal output API is write-only. There is no `readchar()` function to
read back screen contents. The `gfxterm` module maintains a private
`virtual_screen.text_buffer` but does not export it.

### 3.2 Solution: Terminal Output Wrapper

Register a wrapper `grub_term_output` that interposes on the active terminal,
capturing all output into a shadow buffer while forwarding to the real terminal.

The `grub_term_output` interface provides **pre-parsed, structured calls** — no
terminal escape sequence interpretation is needed:

```c
struct grub_term_output {
    void (*putchar)       (term, const struct grub_unicode_glyph *c);
    void (*gotoxy)        (term, struct grub_term_coordinate pos);
    void (*cls)           (term);
    void (*setcolorstate) (term, grub_term_color_state state);
    void (*setcursor)     (term, int on);
    struct grub_term_coordinate (*getwh) (term);
    struct grub_term_coordinate (*getxy) (term);
    void (*refresh)       (term);
};
```

All escape sequence parsing happens upstream in GRUB's `terminfo.c` layer. By
the time `putchar` is called, we receive a `grub_unicode_glyph` with the actual
codepoint. Cursor movement is already resolved into explicit `gotoxy()` calls.

### 3.3 Shadow Terminal Implementation

The shadow terminal (`Drivers/Screen/Grub/screen.c`) maintains a buffer of
`ShadowCell` structures (character + VGA color attribute) indexed by row and
column:

```
shadow_putchar(glyph):
    buffer[cursor_y][cursor_x] = { .character = glyph->base, .color = current_color }
    advance cursor (handle line wrap, scroll)
    forward to real_term->putchar(glyph)

shadow_gotoxy(pos):
    cursor_x = pos.x; cursor_y = pos.y
    forward to real_term->gotoxy(pos)

shadow_cls():
    clear entire shadow buffer
    forward to real_term->cls()

shadow_setcolorstate(state):
    current_color = state (mapped to VGA attribute byte)
    forward to real_term->setcolorstate(state)

shadow_getwh() / shadow_getxy() / shadow_refresh():
    forward to real_term
```

On `construct()`, the driver removes the real terminal from GRUB's active
output list, inserts the shadow terminal wrapper in its place, and registers
a `brltty_keys` input terminal. On `destruct()`, it restores the original
terminal.

### 3.4 BRLTTY Screen Driver

`Drivers/Screen/Grub/screen.c` reads the shadow buffer:

| BaseScreen method | Implementation |
|-------------------|----------------|
| `describe()` | Return rows/cols from `shadow_getwh()`, cursor from shadow state |
| `readCharacters()` | Copy from shadow buffer into `ScreenCharacter` array |
| `insertKey()` | Push key into ring buffer, returned by `getkey()` |
| `poll()` | Always returns 1 (content changes on every GRUB output) |
| `currentVirtualTerminal()` | Returns 1 (single screen) |


## 4. USB Braille Device Support

### 4.1 Implementation

`Programs/usb_grub.c` implements BRLTTY's platform USB functions using GRUB's
USB API. This enables all USB braille displays that BRLTTY supports.

### 4.2 BRLTTY's USB I/O Stack

The full call chain:

```
Braille driver (e.g. HumanWare)
  -> writeBraillePacket() / readBraillePacket()      Programs/brl_base.c
    -> gioWriteData() / gioReadByte()                 Programs/gio.c
      -> writeUsbData() / readUsbData()               Programs/gio_usb.c
        -> usbWriteData() / usbReadData()             Programs/usb.c
          -> usbWriteEndpoint() / usbReadEndpoint()   Programs/usb_grub.c
            -> grub_usb_bulk_read/write()              GRUB USB API
```

Everything above `usb_grub.c` is platform-independent.

### 4.3 Function Mapping: usb_grub.c to GRUB API

| usb_grub.c function | GRUB API | Notes |
|----------------------|----------|-------|
| `usbFindDevice()` | `grub_usb_iterate()` + descriptor match | Core enumeration |
| `usbSetConfiguration()` | `grub_usb_set_configuration()` | Direct mapping |
| `usbClaimInterface()` | No-op | No competing drivers in GRUB |
| `usbReleaseInterface()` | No-op | |
| `usbClearHalt()` | `grub_usb_clear_halt()` | Direct mapping |
| `usbControlTransfer()` | `grub_usb_control_msg()` | Direct mapping |
| `usbReadEndpoint()` | `grub_usb_bulk_read_extended()` | Has timeout parameter |
| `usbWriteEndpoint()` | `grub_usb_bulk_write()` | Direct mapping |
| `usbReadDeviceDescriptor()` | Copy from `dev->descdev` | Already populated by GRUB |
| `usbSubmitRequest()` | `grub_usb_bulk_read_background()` | Async read |
| `usbReapResponse()` | `grub_usb_check_transfer()` | Check async completion |
| `usbCancelRequest()` | `grub_usb_cancel_transfer()` | Cancel async |
| `usbDisableAutosuspend()` | No-op | No power management in bootloader |
| `usbResetDevice()` | No-op (not available in GRUB) | Minor gap |
| `usbSetAlternative()` | Raw control transfer | Minor gap |
| `usbAllocateEndpointExtension()` | Minimal bookkeeping | Trivial |
| `usbDeallocateEndpointExtension()` | `grub_free()` | Trivial |
| `usbDeallocateDeviceExtension()` | `grub_free()` | Trivial |
| `usbForgetDevices()` | Cleanup | Trivial |
| `usbMonitorInputEndpoint()` | Return 0 (polling model) | No async monitoring |

### 4.4 GRUB's Existing USB Infrastructure

GRUB already has a full USB stack:

| Component | Location |
|-----------|----------|
| Core USB framework | `grub-core/bus/usb/usb.c` |
| UHCI host controller | `grub-core/bus/usb/uhci.c` |
| OHCI host controller | `grub-core/bus/usb/ohci.c` |
| EHCI host controller | `grub-core/bus/usb/ehci.c` |
| USB hub support | `grub-core/bus/usb/usbhub.c` |
| USB transfers | `grub-core/bus/usb/usbtrans.c` |
| FTDI USB-serial | `grub-core/bus/usb/serial/ftdi.c` |
| PL2303 USB-serial | `grub-core/bus/usb/serial/pl2303.c` |


## 5. Event Loop and Timing

### 5.1 GRUB's Execution Model

GRUB is single-threaded with a cooperative polling loop. The menu loop
(`grub-core/normal/menu.c:run_menu`) is:

```c
while (1) {
    key = grub_getkey_noblock();   // polls all registered term inputs
    // handle key or timeout
    grub_cpu_idle();
}
```

`grub_getkey_noblock()` iterates all registered `grub_term_input` terminals,
calling each one's `getkey()` method. Returns `GRUB_TERM_NO_KEY` if nothing
available.

### 5.2 BRLTTY's Async Framework

BRLTTY's main wait loop (`Programs/async_wait.c:asyncAwaitCondition`) does:

1. **Check alarms** — fire any expired timer callbacks
2. **Check tasks** — run pending task callbacks
3. **Check I/O** — poll for input with remaining timeout
4. If nothing happened -> `approximateDelay(timeout)` (calls `grub_millisleep()`
   on GRUB)

All timing uses `grub_get_time_ms()` — already implemented in
`Programs/timing.c` for the GRUB platform.

### 5.3 Integration: BRLTTY Poll via getkey()

The screen driver registers a `grub_term_input` whose `getkey()` method
drives BRLTTY's event loop:

```c
static int
brlttyInput_getkey (struct grub_term_input *term) {
  brlttyWait(0);    // one non-blocking pass: fire alarms, poll I/O
  return popKey();  // return injected key or GRUB_TERM_NO_KEY
}
```

`brlttyWait(0)` performs one non-blocking iteration through the async
framework — firing expired alarms (display refresh, keepalive), processing
pending USB I/O, and returning immediately. This is the same cooperative
polling approach used on DOS.

```
GRUB menu loop
  -> grub_getkey_noblock()
    -> keyboard terminal getkey()       -> poll keyboard
    -> serial terminal getkey()         -> poll serial
    -> brltty terminal getkey()
        -> brlttyWait(0)
            -> fire expired alarms (display refresh, keepalive)
            -> poll braille USB input
            -> update braille display with shadow buffer content
        -> popKey()
            -> return injected GRUB keycode or GRUB_TERM_NO_KEY
```

### 5.4 Module Entry Point and Command Registration

GRUB's `insmod` command does not pass arguments to modules — `grub_mod_init()`
receives only a `grub_dl_t` module handle. To accept configuration,
`GRUB_MOD_INIT` registers a `brltty` GRUB command, and the actual
initialization happens when that command is invoked.

```c
static grub_err_t
grub_cmd_brltty (grub_command_t cmd, int argc, char *argv[])
{
  // argc/argv come from the GRUB command line, e.g.:
  //   brltty -b hw -q -n
  brlttyConstruct(argc, argv);
  return GRUB_ERR_NONE;
}

GRUB_MOD_INIT(brltty)
{
  grub_register_command("brltty", grub_cmd_brltty,
                        "[OPTIONS]", "Start BRLTTY braille support.");
}

GRUB_MOD_FINI(brltty)
{
  brlttyDestruct();
  grub_unregister_command(...);
}
```

This gives users and distro packagers full control via `grub.cfg`:

```
# Load the module and USB stack
insmod usb
insmod uhci
insmod ohci
insmod ehci
insmod brltty

# Start BRLTTY with options
brltty -b hw -q -n
```

Or from the GRUB command line interactively:

```
grub> insmod brltty
grub> brltty --braille-driver=hw
```

GRUB environment variables provide an alternative configuration channel.
`GRUB_MOD_INIT` can read variables via `grub_env_get()` for auto-start
scenarios where the user wants BRLTTY to start immediately on module load
without a separate command:

```
# In grub.cfg:
set brltty_args="-b hw -q -n"
insmod brltty
```


## 6. Configuration and File Access

### 6.1 GRUB's File API

GRUB has a full filesystem abstraction that can read files from any mounted
partition. The API (`grub/file.h`) provides:

```c
grub_file_t grub_file_open  (const char *name, enum grub_file_type type);
grub_ssize_t grub_file_read (grub_file_t file, void *buf, grub_size_t len);
grub_err_t   grub_file_close(grub_file_t file);
grub_off_t   grub_file_seek (grub_file_t file, grub_off_t offset);
grub_off_t   grub_file_size (const grub_file_t file);
```

File paths use GRUB's device syntax: `(hd0,gpt2)/etc/brltty.conf` or
paths relative to GRUB's `$prefix` variable (typically `/boot/grub`).

### 6.2 File Access for BRLTTY

This enables loading real configuration files, key tables, and text tables
from disk at boot time. BRLTTY's file I/O functions (`Programs/file.c`)
can be given `GRUB_RUNTIME` implementations that wrap the GRUB file API:

| BRLTTY operation | GRUB implementation |
|------------------|---------------------|
| Open file | `grub_file_open(path, GRUB_FILE_TYPE_NONE)` |
| Read file | `grub_file_read(file, buf, len)` |
| Close file | `grub_file_close(file)` |
| Get file size | `grub_file_size(file)` |
| Seek | `grub_file_seek(file, offset)` |
| File exists check | `grub_file_open()` + `grub_file_close()` |

Note: GRUB's file API is read-only — writing (e.g., saving preferences) is
not possible, which is expected in a bootloader context.

### 6.3 Configuration File Locations

BRLTTY can look for configuration files relative to GRUB's `$prefix`:

```
$prefix/brltty.conf          # e.g. /boot/grub/brltty.conf
$prefix/brltty/Input/        # key table files
$prefix/brltty/Text/         # text table files
```

Alternatively, paths can be specified as arguments to the `brltty` command:

```
brltty -f (hd0,gpt2)/etc/brltty.conf
```

### 6.4 Environment Variables

BRLTTY reads configuration from environment variables named
`BRLTTY_<OPTION>` (e.g., `BRLTTY_BRAILLE_DRIVER`, `BRLTTY_TEXT_TABLE`).
Under Linux, these are standard environment variables read via `getenv()`.

GRUB has its own environment accessible via `grub_env_get()` /
`grub_env_set()`. Under `GRUB_RUNTIME`, BRLTTY's `getenv()` calls can
map directly to `grub_env_get()`, making the same variable names work:

```
# In grub.cfg:
set BRLTTY_BRAILLE_DRIVER=hw
set BRLTTY_TEXT_TABLE=en_US
insmod brltty
brltty
```

This is implemented by providing a `getenv()` wrapper under `GRUB_RUNTIME`
that calls `grub_env_get()`. The variable name convention is preserved —
no GRUB-specific renaming is needed.

### 6.5 What Cannot Work

Some BRLTTY features inherently require a hosted OS and should be compiled
out under `GRUB_RUNTIME`:

- Standard input processing (`stdin`)
- Preferences file writing (GRUB's file API is read-only)
- Locale and internationalization


## 7. Freestanding Environment — POSIX Compatibility

### 7.1 The Problem

BRLTTY's core source files assume a POSIX hosted environment. Under GRUB's
freestanding build (`-ffreestanding -nostdinc -nostdlib`), many standard
headers and library functions are unavailable.

GRUB provides partial POSIX compatibility through its `posix_wrap` directory
(`grub-core/lib/posix_wrap/`), which supplies minimal versions of:
`limits.h`, `string.h`, `stdlib.h`, `stdint.h`, `stdio.h`, `ctype.h`,
`errno.h`, `wchar.h`, `wctype.h`, `unistd.h`, `assert.h`, `locale.h`,
`inttypes.h`.

### 7.2 Stub Headers (`Headers/grub/`)

The `Headers/grub/` directory provides POSIX-compatible stub headers for
headers not covered by GRUB's `posix_wrap`. These are found via `-I../Headers`
in the include path:

| Header | Provides |
|--------|----------|
| `Headers/grub/time.h` | `time_t`, `struct timespec`, `struct timeval`, `struct tm`; chains to GRUB's real `grub/time.h` via relative path |
| `Headers/grub/signal.h` | Empty stub (types moved to `prologue.h` to avoid conflicts) |
| `Headers/grub/fcntl.h` | `O_RDONLY`, `O_WRONLY`, `O_RDWR`, `O_CREAT`, `O_TRUNC`; `open()` stub |
| `Headers/grub/termios.h` | `struct termios`, flag constants (`ECHO`, `ICANON`, etc.), `tcgetattr`/`tcsetattr` stubs |
| `Headers/grub/strings.h` | Empty stub (functions declared in `prologue.h`) |
| `Headers/grub/sys/stat.h` | `struct stat` with `st_dev`, `st_ino`, `st_mode`, `st_size`; `stat()`/`fstat()` stubs |
| `Headers/grub/sys/ioctl.h` | `TIOCGWINSZ`, `struct winsize`, `ioctl()` stub |
| `Headers/grub/search.h` | `hsearch`/`hcreate`/`hdestroy` stubs |

**Header shadowing caveat**: `-I../Headers` causes `#include <grub/time.h>`
to find our stub before GRUB's real `grub/time.h`. The stub chains to the
real header via relative path: `#include "../../grub-root/include/grub/time.h"`.
`#include_next` was tried but failed ("No such file or directory") because
the real header isn't on the same include path.

### 7.3 System Package (`Programs/system_grub.c`)

All POSIX function implementations live in `Programs/system_grub.c`, which
maps standard C functions to GRUB equivalents:

| Category | Functions | Implementation |
|----------|-----------|----------------|
| String | `strdup`, `strtok`, `strerror`, `strncmp`, `strrchr`, `strcspn`, `strspn`, `strpbrk`, `strncasecmp` | Native implementations using `grub_*` primitives |
| stdio (FILE) | `fopen`, `fclose`, `fread`, `fwrite`, `fprintf`, `feof`, `ferror`, `fflush`, `fgetc`, `fgets`, `fputs`, `fputc`, `fileno` | `fopen` → `grub_file_open`, `fread` → `grub_file_read`, stdout/stderr → `grub_printf` |
| stdio (fd) | `close`, `write`, `read` | fd 1/2 → `grub_printf`, others return -1 |
| Formatting | `vsnprintf`, `vprintf`, `atoi` | Delegate to `grub_vsnprintf` / `grub_strtol` |
| Sorting | `qsort`, `bsearch` | Shell sort and binary search implementations |
| Time | `time`, `localtime`, `gmtime`, `strftime` | `time` → `grub_get_time_ms()/1000`, others are minimal stubs |
| Environment | `getenv` | → `grub_env_get()` |
| Process | `exit` | → `grub_fatal()` |
| Option parsing | `getopt` (+ `optarg`, `optind`, `opterr`, `optopt` globals) | Returns -1 (GRUB uses `grub_register_command`) |
| Locale | `setlocale` | Returns `"C"` |
| Stubs | `srand`, `unlink`, `rename`, `pipe`, `fdopen`, `freopen`, `setvbuf`, `select` | No-op or return -1 |

Globals: `stdin = NULL`, `stdout = NULL`, `stderr = NULL` — console output
is detected by checking for these sentinel values.

### 7.4 Prologue Declarations (`Headers/prologue.h` GRUB_RUNTIME block)

The `GRUB_RUNTIME` block in `prologue.h` (the universal header included by
every `.c` file) provides:

- **Types**: `intptr_t`, `ino_t`, `dev_t`, `sig_atomic_t`, `off_t`, `pid_t`,
  `uid_t`, `gid_t`, `FILE_ptr` typedef, `fd_set`
- **Format macros**: `PRId8` through `PRIxPTR`, `PRIXPTR`, `PRIi32`
- **Errno codes**: `EAGAIN`, `EIO`, `ENODEV`, `EBUSY`, `EACCES`, `EEXIST`,
  `EINTR`, `EROFS`, `EPIPE` (supplementing GRUB's `EINVAL`/`ENOMEM`/`ENOENT`)
- **Limits**: `UINT16_C`, `INT16_MIN`, `UINT32_MAX`
- **Constants**: `STDIN_FILENO`/`STDOUT_FILENO`/`STDERR_FILENO`,
  `_IONBF`/`_IOLBF`/`_IOFBF`, `LC_ALL`/`LC_CTYPE`
- **Macros**: `FD_ZERO`/`FD_SET`/`FD_CLR`/`FD_ISSET`, `ffs()` → `__builtin_ffs()`
- **Function declarations**: All functions implemented in `system_grub.c`
- **Feature guards**: `#undef HAVE_SIGNAL_H`, `#undef HAVE_FCHDIR`,
  `#undef HAVE_SELECT`, `HAVE_DECL_LOCALTIME_R 0`
- **Wide-char functions**: `wcslen`, `wmemcpy`, `wmemmove`, `wmemset`,
  `wmemcmp`, `wmemchr`, `wcsncmp`, `wcschr`, `wcsrchr`, `wcscpy`, `wcsncpy`,
  `wcstok`, `swprintf`, `fgetwc`, and `isw*`/`tow*` macros (GRUB's `wchar.h`
  provides `mbrtowc`/`wcrtomb` but not these)

### 7.5 ARRAY_SIZE Conflict Resolution

GRUB defines `ARRAY_SIZE(array)` as a 1-argument macro returning the element
count. BRLTTY uses `ARRAY_SIZE(pointer, count)` as a 2-argument macro
returning the byte size. Since both forms appear in the same compilation
units (GRUB headers are included via `posix_wrap`), a variadic dispatch
macro handles both:

```c
#undef ARRAY_SIZE
#define ARRAY_SIZE_1(array) (sizeof(array) / sizeof((array)[0]))
#define ARRAY_SIZE_2(pointer, count) ((count) * sizeof(*(pointer)))
#define ARRAY_SIZE_SELECT(_1, _2, NAME, ...) NAME
#define ARRAY_SIZE(...) ARRAY_SIZE_SELECT(__VA_ARGS__, ARRAY_SIZE_2, ARRAY_SIZE_1)(__VA_ARGS__)
```

### 7.6 NO_FLOAT — Floating-Point Exclusion

GRUB forbids FPU use: the bootloader does not save/restore FPU state across
context switches, does not guarantee FPU initialization, and does not link
a soft-float library. All floating-point code must be compiled out.

BRLTTY uses `#define NO_FLOAT` (set in the `GRUB_RUNTIME` block of
`prologue.h`) with `#ifndef NO_FLOAT` guards in:

| File | Guarded content |
|------|-----------------|
| `Headers/color_types.h` | `HSVColor`, `HLSColor` struct definitions |
| `Headers/color.h` | All float-using color functions (`rgbToHsv`, `hsvToRgb`, etc.) |
| `Headers/cmdargs.h` | `parseFloat`, `parseDegrees`, `parsePercent` |
| `Programs/color.c` | Float-based color conversion implementations |
| `Programs/color_internal.h` | `HSVComponentRange`, `HSVColorEntry` |
| `Programs/cmdargs.c` | Float parsing implementations |
| `Programs/scr.c` | RGB color name path (VGA path remains available) |

GRUB's native color model is VGA 16-color (single byte: bits 3:0 = foreground,
bits 6:4 = background). BRLTTY's `vgaColorName()` provides integer-only
name lookup, so float math is not needed for color naming in GRUB.

### 7.7 HAVE_WCHAR_H / WCHAR_MAX Forcing

GRUB's `posix_wrap` includes `wchar.h` (providing `mbrtowc`, `wcrtomb`,
`mbsinit`, `wcscoll`), but `configure` cannot detect it in freestanding
mode. Two defines are forced in `prologue.h`:

1. `HAVE_WCHAR_H` — forced in the first `GRUB_RUNTIME` block (before the
   `#ifdef HAVE_WCHAR_H` decision point that controls wchar inclusion)
2. `WCHAR_MAX` — forced immediately after `wchar.h` inclusion (GRUB's
   `wchar.h` doesn't define it, but BRLTTY's `prologue.h` uses it to
   gate wide-character code paths)

### 7.8 Other Build Compatibility Fixes

- **strtol/strtoul API mismatch**: GRUB declares `const char **end` vs
  standard C's `char **end`. Suppressed with `-Wno-incompatible-pointer-types`
  in `cfg-grub`.
- **`__linux__` in freestanding mode**: GCC always defines `__linux__` on Linux
  hosts, even with `-ffreestanding`. `Programs/brltty-ttb.c` guards
  `linux/kd.h` inclusion with `&& !defined(GRUB_RUNTIME)`.
- **`MonitorEntry` typedef**: `Programs/async_io.c` has conditional typedefs
  for MinGW/poll/select platforms. Added `#ifndef ASYNC_CAN_MONITOR_IO`
  fallback stub for GRUB.
- **Dynamic symbol lookup**: `Programs/dynld_grub.c`'s `findSharedSymbol()`
  stubbed to return 0 — all drivers are internal, no shared library loading.


## 8. Implementation Status

### Completed

- **Module entry point** (`Programs/grub_module.c`) —
  `GRUB_MOD_INIT`/`GRUB_MOD_FINI` calling `brlttyConstruct()`/`brlttyDestruct()`

- **Screen driver** (`Drivers/Screen/Grub/screen.c`) —
  Shadow terminal buffer, terminal output wrapper (putchar/gotoxy/cls/
  setcolorstate), key injection ring buffer, BRLTTY-to-GRUB key translation,
  `brlttyWait(0)` integration in `getkey()`

- **USB backend** (`Programs/usb_grub.c`) —
  Full implementation mapping BRLTTY's USB functions to GRUB's USB API

- **Build configuration** (`cfg-grub`) —
  Platform auto-detection and explicit override (`--with-grub-platform=`),
  native GCC toolchain with `-m32`/`-m64`, all tools overridden to avoid
  cross-compiler requirement, all drivers built as internal

- **Build system** (`configure.ac`) —
  Corrected include path ordering (`-nostdinc` before `-isystem`/`-I`),
  GRUB root and posix_wrap include paths

- **Makefile rules** (`Programs/Makefile.in`) —
  `brltty.module` and `brltty.mod` build targets

- **Documentation** —
  `Documents/README.Grub` (build instructions),
  `Documents/GRUB-Module-Design.md` (this document)

- **POSIX compatibility layer** (Section 7) —
  All BRLTTY core source files compile cleanly (0 errors, 0 warnings) under
  GRUB's freestanding environment. Implemented via:
  - `Headers/grub/` stub headers (time.h, signal.h, fcntl.h, termios.h, etc.)
  - `Programs/system_grub.c` (30+ POSIX function implementations)
  - `Headers/prologue.h` GRUB_RUNTIME block (types, macros, declarations,
    wide-char functions)
  - `NO_FLOAT` guards in color/cmdargs code
  - Variadic `ARRAY_SIZE` macro for GRUB/BRLTTY compatibility

### Remaining Work

- **Build `.mod` file** — Build system changes to produce a GRUB `.mod` file
  instead of a standalone executable. The `genmod.sh` post-processing step
  (adding `.modname`/`.moddeps` sections, stripping symbols) needs
  finalization. Currently reaches the link stage with only expected GRUB
  runtime symbol references unresolved.

- **Command-based entry point** (Section 5.4) — Update `Programs/grub_module.c`
  to register a `brltty` GRUB command via `grub_register_command()` instead of
  calling `brlttyConstruct()` directly from `GRUB_MOD_INIT`. This enables
  passing arguments (e.g., `brltty -b hw -q`) and supports auto-start via
  GRUB environment variables.

- **GRUB file I/O wrappers** (Section 6) — Implement `GRUB_RUNTIME` paths
  in BRLTTY's file I/O code (`Programs/file.c`) wrapping `grub_file_open()` /
  `grub_file_read()` / `grub_file_close()`. This enables loading `brltty.conf`,
  key tables, and text tables from disk.

- **Testing** — Verify in QEMU with USB passthrough and on real hardware.


## 9. Key Source Files Reference

### BRLTTY (this repository)

| File | Role |
|------|------|
| `cfg-grub` | Build configuration script |
| `configure.ac` (lines 283-298) | GRUB platform detection, compiler flags |
| `Headers/prologue.h` | `GRUB_RUNTIME` block: types, macros, declarations, wide-char functions |
| `Headers/grub/time.h` | POSIX time types + chain to GRUB's real `grub/time.h` |
| `Headers/grub/signal.h` | Empty stub (types in prologue.h) |
| `Headers/grub/fcntl.h` | File control constants and `open()` stub |
| `Headers/grub/termios.h` | Terminal I/O types and stubs |
| `Headers/grub/strings.h` | Empty stub (functions in prologue.h) |
| `Headers/grub/search.h` | Hash table stubs |
| `Headers/grub/sys/stat.h` | `struct stat` and stubs |
| `Headers/grub/sys/ioctl.h` | `TIOCGWINSZ`, `struct winsize` |
| `Programs/system_grub.c` | POSIX function implementations (30+ functions) |
| `Programs/grub_module.c` | GRUB module entry point |
| `Programs/serial_grub.c` | Serial I/O via GRUB serial API |
| `Programs/serial_grub.h` | GRUB serial type mappings |
| `Programs/usb_grub.c` | USB backend via GRUB USB API |
| `Programs/dynld_grub.c` | Dynamic loading (stubbed — all drivers internal) |
| `Programs/ports_grub.c` | I/O port access |
| `Programs/charset_grub.c` | Character set handling |
| `Programs/timing.c` | GRUB time functions (`grub_get_time_ms`, etc.) |
| `Programs/usb.c` | Platform-independent USB layer |
| `Programs/usb_devices.c` | USB vendor/product ID database |
| `Programs/gio.c` / `Programs/gio_usb.c` | Generic I/O abstraction |
| `Programs/brl_base.c` | Braille packet read/write |
| `Programs/async_wait.c` | Async event loop |
| `Programs/core.c` | Main update cycle |
| `Drivers/Screen/Grub/screen.c` | Screen driver with shadow terminal |

### GRUB source tree

| File | Role |
|------|------|
| `include/grub/term.h` | Terminal I/O interface |
| `include/grub/usb.h` | USB device API |
| `include/grub/usbserial.h` | USB-serial framework |
| `include/grub/serial.h` | Serial port interface |
| `include/grub/time.h` | `grub_get_time_ms`, `grub_millisleep` |
| `include/grub/dl.h` | Module loading API |
| `include/grub/command.h` | Command registration |
| `include/grub/mm.h` | Memory management |
| `grub-core/lib/posix_wrap/` | Minimal POSIX header stubs |
| `grub-core/term/gfxterm.c` | Graphics terminal (has private `virtual_screen`) |
| `grub-core/term/i386/pc/vga_text.c` | VGA text terminal |
| `grub-core/term/i386/pc/console.c` | BIOS console terminal |
| `grub-core/kern/term.c` | `grub_getkey`, terminal iteration |
| `grub-core/bus/usb/usb.c` | USB core framework |
| `grub-core/bus/usb/serial/ftdi.c` | FTDI USB-serial driver |
| `grub-core/normal/menu.c` | Menu display and input loop |
