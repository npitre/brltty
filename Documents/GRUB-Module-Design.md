# BRLTTY as a GRUB Loadable Module — Design Document

## 1. Overview

This document describes the design for converting BRLTTY's existing GRUB support
from an incomplete, build-into-GRUB approach into a proper GRUB loadable module
(`.mod`). The goal is to provide braille display accessibility during the GRUB
boot menu, distributable by distros without patching GRUB.

### 1.1 Current State

BRLTTY has partial GRUB support consisting of 6 source files:

| File | Purpose |
|------|---------|
| `Programs/serial_grub.c` | Serial port I/O via GRUB's serial API |
| `Programs/serial_grub.h` | Type mappings (BRLTTY types to GRUB types) |
| `Programs/usb_grub.c` | USB stub — all functions return "unsupported" |
| `Programs/dynld_grub.c` | Dynamic loading via GRUB's `grub_dl_*` functions |
| `Programs/ports_grub.c` | I/O port access via `grub_inb`/`grub_outb` |
| `Programs/charset_grub.c` | Minimal character set handling |
| `cfg-grub` | Build configuration wrapper script |

Additionally, `Drivers/Screen/Grub/screen.c` exists but is an empty skeleton.

Key problems with the current approach:

- **Not a loadable module**: compiled as a standalone i386-elf binary with
  `--host=i386-elf -ffreestanding -nostdinc -nostdlib`. No `GRUB_MOD_INIT`/
  `GRUB_MOD_FINI` entry points. Must be linked into a custom GRUB build.
- **USB completely stubbed out**: `usb_grub.c` returns "unsupported" for every
  function, making all USB braille displays non-functional.
- **Screen driver is empty**: `Drivers/Screen/Grub/screen.c` only calls
  `initializeRealScreen()` with no actual implementation.
- **No documentation**: no build instructions, no integration guide.
- **Limited drivers**: only serial braille drivers (`lt`, `tt`, `vd`) enabled.

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

There is no `grub-devel` package. Modules must be built against the GRUB source
tree. Two approaches:

1. **In-tree**: add module to `grub-core/Makefile.core.def`, run
   `./autogen.sh && ./configure && make`
2. **Out-of-tree**: compile with GRUB source headers on the include path, using
   the same compiler flags GRUB uses (from its `config.h` and build system)

The GRUB source can be obtained from:
- `git clone https://git.savannah.gnu.org/git/grub.git`
- Or via distro SRPM: `dnf download --source grub2`


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
    current_color = state
    forward to real_term->setcolorstate(state)

shadow_getwh() / shadow_getxy() / shadow_refresh():
    forward to real_term
```

This is architecturally similar to `screen(1)` or `tmux` — it sits between GRUB
and the real terminal, maintaining its own screen buffer. BRLTTY already has a
tmux screen driver (`Drivers/Screen/TerminalEmulator/`) using the same concept.

### 3.4 BRLTTY Screen Driver

`Drivers/Screen/Grub/screen.c` reads the shadow buffer:

| BaseScreen method | Implementation |
|-------------------|----------------|
| `describe()` | Return rows/cols from `shadow_getwh()`, cursor from shadow state |
| `readCharacters()` | Copy from shadow buffer into `ScreenCharacter` array |
| `insertKey()` | Inject key into GRUB's input queue |
| `poll()` | Always returns 1 (content changes on every GRUB output) |
| `currentVirtualTerminal()` | Returns 1 (single screen) |


## 4. USB Braille Device Support

### 4.1 Current State

`Programs/usb_grub.c` stubs out all 17 platform USB functions with
`logUnsupportedFunction()`. This is the only file that needs implementation to
enable USB braille displays.

### 4.2 BRLTTY's USB I/O Stack

The full call chain on Linux is:

```
Braille driver (e.g. HumanWare)
  → writeBraillePacket() / readBraillePacket()      Programs/brl_base.c
    → gioWriteData() / gioReadByte()                 Programs/gio.c
      → writeUsbData() / readUsbData()               Programs/gio_usb.c
        → usbWriteData() / usbReadData()             Programs/usb.c
          → usbWriteEndpoint() / usbReadEndpoint()   Programs/usb_linux.c
            → ioctl(USBDEVFS_BULK)                    Linux kernel
```

Everything above `usb_grub.c` is platform-independent. Implementing this one
file enables **all 134 USB braille devices** that BRLTTY supports.

### 4.3 Function Mapping: usb_grub.c to GRUB API

| usb_grub.c function | GRUB API | Notes |
|----------------------|----------|-------|
| `usbFindDevice()` | `grub_usb_iterate()` + descriptor match | Core enumeration. Walk GRUB's USB devices, match vendor/product from BRLTTY's `UsbChannelDefinition` tables |
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

The USB API provides:
- `grub_usb_iterate()` — enumerate all connected devices
- `grub_usb_device_initialize()` — read device descriptors
- `grub_usb_set_configuration()` — select configuration
- `grub_usb_bulk_read()` / `grub_usb_bulk_write()` — data transfer
- `grub_usb_bulk_read_extended()` — with timeout
- `grub_usb_bulk_read_background()` / `grub_usb_check_transfer()` — async
- `grub_usb_control_msg()` — control transfers
- `grub_usb_clear_halt()` — endpoint reset
- `grub_usb_register_attach_hook_class()` — hotplug notification

### 4.5 USB Braille Device Landscape

From BRLTTY's `Programs/usb_devices.c` (134 total device entries):

| USB Vendor ID | Count | Manufacturer | Transport type |
|---------------|-------|--------------|----------------|
| `0x0904` | 35 | Baum | Raw USB bulk endpoints |
| `0x1FE4` | 25 | HandyTech | HID or FTDI (`0x0403`) |
| `0xC251` | 17 | EuroBraille | Raw USB bulk endpoints |
| `0x1C71` | 15 | HumanWare | Raw USB bulk or HID |
| `0x0403` | 13 | FTDI chip | USB-serial (FTDI) |
| `0x0798` | 5 | Alva | Raw USB bulk endpoints |
| Others | 24 | Various | Mixed |

FTDI-based devices (`0x0403`) could work through GRUB's existing
`usbserial_ftdi.mod` + BRLTTY's `serial_grub.c` without any USB implementation
work. The rest require the `usb_grub.c` implementation.


## 5. Target Device: HumanWare Brailliant BI 40

### 5.1 USB Details

- **Vendor:Product** — `1C71:C005`
- **Driver** — `hw` (HumanWare), `Drivers/Braille/HumanWare/braille.c`
- **Protocol** — HumanWare serial protocol over USB bulk endpoints
- **Configuration** — 1, **Interface** — 1, **Alternative** — 0
- **Input endpoint** — 2 (bulk), **Output endpoint** — 3 (bulk)
- **Serial parameters** — 115200 baud, 8 bits, even parity

### 5.2 Protocol

The HumanWare serial protocol uses ESC-framed packets:

```
Byte 0: ESC (0x1B)
Byte 1: message type
Byte 2: payload length
Byte 3+: payload data
```

Key message types:
- `HW_MSG_INIT` — initialize / identify device
- `HW_MSG_INIT_RESP` — response with model ID + cell count
- `HW_MSG_DISPLAY` — write cells to braille display
- `HW_MSG_KEYS` / `HW_MSG_KEY_DOWN` / `HW_MSG_KEY_UP` — key events
- `HW_MSG_KEEP_AWAKE` — keepalive ping
- `HW_MSG_GET_FIRMWARE_VERSION` — query firmware

### 5.3 Device Identification

On USB enumeration, the HumanWare driver matches vendor/product ID `1C71:C005`,
opens configuration 1 / interface 1, and sends `HW_MSG_INIT`. The device
responds with its model identifier and cell count (40 for the BI 40). The driver
then selects the appropriate key table (`KEY_TABLE_DEFINITION(BI40)`).

### 5.4 Newer Models

The Brailliant BI 40X (`1C71:C131`) uses HID protocol instead of the serial
protocol. It communicates via HID reports (`HW_REP_OUT_WriteCells`,
`HW_REP_IN_PressedKeys`, `HW_REP_FTR_Capabilities`, etc.) over USB bulk
endpoints. The same `usb_grub.c` implementation would support both protocols
since BRLTTY handles the protocol difference at the driver level.


## 6. Event Loop and Timing

### 6.1 GRUB's Execution Model

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

### 6.2 BRLTTY's Async Framework

BRLTTY's main wait loop (`Programs/async_wait.c:asyncAwaitCondition`) does:

1. **Check alarms** — fire any expired timer callbacks
2. **Check tasks** — run pending task callbacks
3. **Check I/O** — poll for input with remaining timeout
4. If nothing happened → `approximateDelay(timeout)` (calls `grub_millisleep()`
   on GRUB)

All timing uses `grub_get_time_ms()` — already implemented in
`Programs/timing.c` for the GRUB platform.

### 6.3 Integration: BRLTTY as a Terminal Input Driver

Register a `grub_term_input` whose `getkey()` method:

1. Polls the braille device USB endpoint with a short timeout (~10ms)
2. Runs one iteration of BRLTTY's update cycle:
   - Process any braille key events
   - Fire expired timer callbacks (keepalive, etc.)
   - Update the braille display if screen content changed
3. If a braille key maps to a GRUB action, return the GRUB keycode
4. Otherwise return `GRUB_TERM_NO_KEY`

```
GRUB menu loop
  → grub_getkey_noblock()
    → keyboard terminal getkey()       → poll keyboard
    → serial terminal getkey()         → poll serial
    → brltty terminal getkey()         → NEW
        → poll braille USB input (short timeout)
        → fire expired BRLTTY alarms (keepalive, etc.)
        → update braille display with shadow buffer content
        → translate braille keys → GRUB keycodes
        → return key or GRUB_TERM_NO_KEY
```

This requires no threads, no interrupts, no signal handlers. BRLTTY's async
framework already supports this model — it's the same approach used on DOS.


## 7. Implementation Plan

### Phase 1: GRUB Module Skeleton

Create a minimal `.mod` that loads in GRUB.

- Add `GRUB_MOD_INIT(brltty)` / `GRUB_MOD_FINI(brltty)` entry points
- Register a `grub_register_command("brltty", ...)` that prints a status message
- Set up the build infrastructure (in-tree `Makefile.core.def` entry or
  out-of-tree build recipe)
- Verify: `insmod brltty` works at the GRUB command line

No braille hardware needed. Testable in QEMU.

### Phase 2: Screen Capture (Shadow Terminal)

Implement the terminal output wrapper.

- Intercept `putchar`/`gotoxy`/`cls`/`setcolorstate` on the active terminal
- Maintain a shadow text buffer (columns x rows, character + attribute)
- Register a `brltty_screen` debug command that dumps the buffer contents
- Verify: boot to GRUB menu, `insmod brltty`, confirm shadow buffer matches
  visible screen

No braille hardware needed. Testable in QEMU.

### Phase 3: USB Backend

Implement `usb_grub.c` against GRUB's USB API.

Priority order:
1. `usbFindDevice()` — enumerate via `grub_usb_iterate()`, match vendor/product
2. `usbSetConfiguration()` / `usbClaimInterface()` / `usbReleaseInterface()`
3. `usbReadEndpoint()` / `usbWriteEndpoint()` — bulk transfers
4. `usbControlTransfer()` — device setup
5. `usbClearHalt()` / `usbReadDeviceDescriptor()`
6. Remaining functions as no-ops or trivial implementations

Verify: BRLTTY's USB channel matching finds device `1C71:C005`.

Benefits from QEMU USB passthrough or emulated USB devices.

### Phase 4: Braille Display I/O

Connect BRLTTY's braille driver stack to the USB backend.

- BRLTTY's GIO/USB stack should now work end-to-end
- The HumanWare driver sends `HW_MSG_INIT`, gets cell count, writes cells
- Verify: text appears on the Brailliant BI 40

Requires real hardware (or QEMU USB passthrough to the device).

### Phase 5: Input Integration

Register a `grub_term_input` for braille key input.

- Implement the `getkey()` poll method described in Section 6.3
- Map braille key presses to GRUB keycodes (arrows, enter, escape)
- Fire BRLTTY timer callbacks during each poll cycle
- Verify: navigate the GRUB menu using the braille display

### Phase 6: BRLTTY Screen Driver

Replace the empty `Drivers/Screen/Grub/screen.c`.

- Implement `describe()` — rows, cols, cursor from shadow terminal state
- Implement `readCharacters()` — copy from shadow buffer
- Implement `insertKey()` — inject into GRUB's input queue
- This enables BRLTTY's full navigation, cursor routing, and command processing

### Testing Strategy

- Phases 1-2: QEMU with standard GRUB, no special hardware
- Phase 3: QEMU with USB passthrough (`-device usb-host,vendorid=0x1c71,productid=0xc005`) or emulated USB
- Phases 4-6: Real hardware or QEMU USB passthrough to Brailliant BI 40


## 8. Key Source Files Reference

### BRLTTY (this repository)

| File | Role |
|------|------|
| `cfg-grub` | Build configuration script |
| `configure.ac` (lines 283-295) | GRUB platform detection, compiler flags |
| `Headers/prologue.h` (lines 509-521) | `GRUB_RUNTIME` conditional fixes |
| `Programs/serial_grub.c` | Serial I/O via GRUB serial API |
| `Programs/serial_grub.h` | GRUB serial type mappings |
| `Programs/usb_grub.c` | USB stub (to be implemented) |
| `Programs/dynld_grub.c` | Dynamic loading |
| `Programs/ports_grub.c` | I/O port access |
| `Programs/charset_grub.c` | Character set handling |
| `Programs/timing.c` (lines 60-70, 297-300, 374-375) | GRUB time functions |
| `Programs/usb.c` | Platform-independent USB layer |
| `Programs/usb_devices.c` | USB vendor/product ID database (134 devices) |
| `Programs/gio.c` / `Programs/gio_usb.c` | Generic I/O abstraction |
| `Programs/brl_base.c` | Braille packet read/write |
| `Programs/async_wait.c` | Async event loop |
| `Programs/core.c` | Main update cycle |
| `Drivers/Braille/HumanWare/braille.c` | HumanWare driver (BI 40) |
| `Drivers/Screen/Grub/screen.c` | Screen driver (empty skeleton) |

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
| `grub-core/term/gfxterm.c` | Graphics terminal (has private `virtual_screen`) |
| `grub-core/term/i386/pc/vga_text.c` | VGA text terminal (reads/writes `0xB8000`) |
| `grub-core/term/i386/pc/console.c` | BIOS console terminal |
| `grub-core/kern/term.c` | `grub_getkey`, terminal iteration |
| `grub-core/bus/usb/usb.c` | USB core framework |
| `grub-core/bus/usb/serial/ftdi.c` | FTDI USB-serial driver (example module) |
| `grub-core/bus/usb/serial/common.c` | USB-serial common code |
| `grub-core/normal/menu.c` | Menu display and input loop |
