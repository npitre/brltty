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

### 5.4 Module Entry Point

`Programs/grub_module.c` provides the GRUB module hooks:

```c
GRUB_MOD_INIT(brltty)
{
  brlttyConstruct(argc, argv);  // argv = {"brltty", "-q", "-n"}
}

GRUB_MOD_FINI(brltty)
{
  brlttyDestruct();
}
```

`brlttyConstruct()` initializes the BRLTTY core, which loads the screen driver
(triggering shadow terminal installation) and probes for braille devices.
`brlttyDestruct()` shuts everything down and restores the original terminal.


## 6. Freestanding Environment — POSIX Compatibility

### 6.1 The Problem

BRLTTY's core source files assume a POSIX hosted environment. Under GRUB's
freestanding build (`-ffreestanding -nostdinc -nostdlib`), many standard
headers and library functions are unavailable.

GRUB provides partial POSIX compatibility through its `posix_wrap` directory
(`grub-core/lib/posix_wrap/`), which supplies minimal versions of:
`limits.h`, `string.h`, `stdlib.h`, `stdint.h`, `stdio.h`, `ctype.h`,
`errno.h`, `wchar.h`, `wctype.h`, `unistd.h`, `assert.h`, `locale.h`,
`inttypes.h`.

### 6.2 Missing Headers

The following standard headers are not provided by GRUB's `posix_wrap`
and are included by BRLTTY core files:

| Header | Used by | Required for |
|--------|---------|-------------|
| `signal.h` | `brltty.c` | Signal handlers (not relevant in GRUB) |
| `time.h` | `core.c`, `timing_types.h` | `struct timespec` (BRLTTY has GRUB alternatives) |
| `fcntl.h` | `program.c`, `messages.c`, `log.c` | File operations (not relevant in GRUB) |

### 6.3 Missing Functions

GRUB's `posix_wrap` headers declare some POSIX functions but not all that
BRLTTY uses. Functions missing at compile time:

| Category | Functions | Used by |
|----------|-----------|---------|
| File I/O | `fopen`, `fclose`, `fflush`, `fwrite`, `fputs`, `fputc`, `ferror` | `cmdline.c`, `cmdput.c` |
| stdio | `stdin`, `stdout`, `vprintf` | `cmdput.c`, `cmdline.c` |
| String | `strdup`, `strtok`, `strerror` | `cmdline.c`, `cmdargs.c` |
| Search | `qsort`, `bsearch` | `cmdline.c` |
| Environment | `getenv` | `cmdline.c` |
| Option parsing | `getopt`, `optarg`, `optind`, `opterr`, `optopt` | `cmdline.c` |
| Process | `exit` | `cmdbase.c`, `cmdput.c` |
| Error codes | `ENOENT`, `ENOSYS` | `cmdline.c`, `pid.c` |
| Integer limits | `UINT16_MAX` | `cmdput.c` |

### 6.4 Macro Conflicts

- `ARRAY_SIZE` — both GRUB (`grub/misc.h`) and BRLTTY define this macro
  with different signatures. Needs conditional definition.

### 6.5 Resolution Strategy

These issues fall into two categories:

**Dead code paths** — Many of the missing functions are used in code paths
that will never execute under GRUB (config file parsing, stdin processing,
environment variables, option parsing with `getopt`). These can be guarded
with `#ifndef GRUB_RUNTIME` to compile them out.

**Missing stubs** — Functions that are called from code paths that do
execute under GRUB need either:
- GRUB-specific implementations (e.g., `exit()` -> `grub_fatal()`)
- Stub implementations that satisfy the linker
- Additional `posix_wrap`-style headers in BRLTTY's own tree

The preferred approach is `#ifdef GRUB_RUNTIME` guards in the BRLTTY source,
keeping changes minimal and localized. BRLTTY already uses this pattern
extensively in `Programs/timing.c`.


## 7. Implementation Status

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

### Remaining Work

- **POSIX compatibility** (Section 6) — Add `#ifdef GRUB_RUNTIME` guards
  to core source files to compile out POSIX-dependent code paths that are
  not relevant in GRUB. This is the primary blocker for a successful build.

- **Testing** — Verify in QEMU with USB passthrough and on real hardware.

- **Makefile `brltty.mod` target** — The `genmod.sh` post-processing step
  (adding `.modname`/`.moddeps` sections, stripping symbols) needs
  finalization.


## 8. Key Source Files Reference

### BRLTTY (this repository)

| File | Role |
|------|------|
| `cfg-grub` | Build configuration script |
| `configure.ac` (lines 283-298) | GRUB platform detection, compiler flags |
| `Headers/prologue.h` (lines 509-521) | `GRUB_RUNTIME` conditional fixes |
| `Programs/grub_module.c` | GRUB module entry point |
| `Programs/serial_grub.c` | Serial I/O via GRUB serial API |
| `Programs/serial_grub.h` | GRUB serial type mappings |
| `Programs/usb_grub.c` | USB backend via GRUB USB API |
| `Programs/dynld_grub.c` | Dynamic loading |
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
