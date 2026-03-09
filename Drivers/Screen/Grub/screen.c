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

/*
 * Screen driver for GRUB.
 *
 * GRUB has no API to read back screen contents. This driver works by
 * installing a "shadow" terminal output that wraps the real GRUB terminal.
 * Every putchar/gotoxy/cls/setcolorstate call is intercepted to maintain
 * an in-memory text buffer, then forwarded to the original terminal so
 * the display is unaffected.
 *
 * BRLTTY's screen reading functions (describe, readCharacters) then
 * read directly from this shadow buffer.
 *
 * The shadow terminal is registered via grub_term_register_output_active()
 * and replaces the original terminal in the active output list. The
 * original terminal's function pointers are saved and called through
 * for every operation.
 *
 * Key injection uses GRUB's input terminal: we push keys into a small
 * ring buffer that our grub_term_input getkey() handler returns.
 */

#include "prologue.h"

#include <stdio.h>
#include <string.h>

#include <grub/term.h>
#include <grub/mm.h>
#include <grub/misc.h>

#include "log.h"
#include "embed.h"
#include "scr_driver.h"

/* Shadow screen dimensions. GRUB's default text mode is 80x25.
 * We query the real terminal at init time and resize if needed. */
#define GRUB_SCREEN_ROWS_MAX 60
#define GRUB_SCREEN_COLS_MAX 256

/* Each cell in the shadow buffer holds a Unicode codepoint and a
 * VGA-style color attribute byte. */
typedef struct {
  wchar_t text;
  unsigned char attributes;
} ShadowCell;

static ShadowCell shadowBuffer[GRUB_SCREEN_ROWS_MAX][GRUB_SCREEN_COLS_MAX];
static unsigned short screenRows;
static unsigned short screenCols;
static unsigned short cursorRow;
static unsigned short cursorCol;
static unsigned char currentAttributes;

/* The real terminal we are wrapping. */
static struct grub_term_output *realTerminal;

/* Our shadow terminal output structure. */
static struct grub_term_output shadowTerminal;

/* Map GRUB color state to VGA attribute byte. GRUB stores normal and
 * highlight colors as VGA attribute bytes in global variables. */
static unsigned char
grubColorToVga (grub_term_color_state state) {
  extern grub_uint8_t grub_term_normal_color;
  extern grub_uint8_t grub_term_highlight_color;

  switch (state) {
    case GRUB_TERM_COLOR_HIGHLIGHT:
      return grub_term_highlight_color;
    case GRUB_TERM_COLOR_NORMAL:
      return grub_term_normal_color;
    default:
      return GRUB_TERM_DEFAULT_STANDARD_COLOR;
  }
}

static void
clearShadowBuffer (void) {
  for (int row = 0; row < screenRows; row++) {
    for (int col = 0; col < screenCols; col++) {
      shadowBuffer[row][col].text = L' ';
      shadowBuffer[row][col].attributes = currentAttributes;
    }
  }
}

/* Shadow terminal callbacks — intercept, record, and forward. */

static void
shadow_putchar (struct grub_term_output *term,
                const struct grub_unicode_glyph *c) {
  grub_uint32_t code = c->base;

  if (code == '\n') {
    cursorCol = 0;
    cursorRow++;
    if (cursorRow >= screenRows) {
      /* Scroll up by one line. */
      memmove(&shadowBuffer[0], &shadowBuffer[1],
              (screenRows - 1) * sizeof(shadowBuffer[0]));
      for (int col = 0; col < screenCols; col++) {
        shadowBuffer[screenRows - 1][col].text = L' ';
        shadowBuffer[screenRows - 1][col].attributes = currentAttributes;
      }
      cursorRow = screenRows - 1;
    }
  } else if (code == '\r') {
    cursorCol = 0;
  } else if (code == '\b') {
    if (cursorCol > 0) cursorCol--;
  } else if (code == '\t') {
    /* Advance to next tab stop (every 8 columns). */
    unsigned short nextTab = (cursorCol + 8) & ~7;
    if (nextTab > screenCols) nextTab = screenCols;
    while (cursorCol < nextTab) {
      if (cursorCol < screenCols) {
        shadowBuffer[cursorRow][cursorCol].text = L' ';
        shadowBuffer[cursorRow][cursorCol].attributes = currentAttributes;
      }
      cursorCol++;
    }
  } else {
    if (cursorRow < screenRows && cursorCol < screenCols) {
      shadowBuffer[cursorRow][cursorCol].text = (wchar_t)code;
      shadowBuffer[cursorRow][cursorCol].attributes = currentAttributes;
    }
    cursorCol++;
    if (cursorCol >= screenCols) {
      cursorCol = 0;
      cursorRow++;
      if (cursorRow >= screenRows) {
        memmove(&shadowBuffer[0], &shadowBuffer[1],
                (screenRows - 1) * sizeof(shadowBuffer[0]));
        for (int col = 0; col < screenCols; col++) {
          shadowBuffer[screenRows - 1][col].text = L' ';
          shadowBuffer[screenRows - 1][col].attributes = currentAttributes;
        }
        cursorRow = screenRows - 1;
      }
    }
  }

  /* Forward to the real terminal. */
  if (realTerminal && realTerminal->putchar) {
    realTerminal->putchar(realTerminal, c);
  }
}

static struct grub_term_coordinate
shadow_getxy (struct grub_term_output *term) {
  if (realTerminal && realTerminal->getxy) {
    return realTerminal->getxy(realTerminal);
  }
  return (struct grub_term_coordinate){ .x = cursorCol, .y = cursorRow };
}

static struct grub_term_coordinate
shadow_getwh (struct grub_term_output *term) {
  if (realTerminal && realTerminal->getwh) {
    return realTerminal->getwh(realTerminal);
  }
  return (struct grub_term_coordinate){ .x = screenCols, .y = screenRows };
}

static void
shadow_gotoxy (struct grub_term_output *term,
               struct grub_term_coordinate pos) {
  cursorCol = pos.x;
  cursorRow = pos.y;

  if (realTerminal && realTerminal->gotoxy) {
    realTerminal->gotoxy(realTerminal, pos);
  }
}

static void
shadow_cls (struct grub_term_output *term) {
  clearShadowBuffer();
  cursorRow = 0;
  cursorCol = 0;

  if (realTerminal && realTerminal->cls) {
    realTerminal->cls(realTerminal);
  }
}

static void
shadow_setcolorstate (struct grub_term_output *term,
                      grub_term_color_state state) {
  currentAttributes = grubColorToVga(state);

  if (realTerminal && realTerminal->setcolorstate) {
    realTerminal->setcolorstate(realTerminal, state);
  }
}

static void
shadow_setcursor (struct grub_term_output *term, int on) {
  if (realTerminal && realTerminal->setcursor) {
    realTerminal->setcursor(realTerminal, on);
  }
}

static void
shadow_refresh (struct grub_term_output *term) {
  if (realTerminal && realTerminal->refresh) {
    realTerminal->refresh(realTerminal);
  }
}

static grub_size_t
shadow_getcharwidth (struct grub_term_output *term,
                     const struct grub_unicode_glyph *c) {
  if (realTerminal && realTerminal->getcharwidth) {
    return realTerminal->getcharwidth(realTerminal, c);
  }
  return 1;
}

static grub_err_t
shadow_init (struct grub_term_output *term) {
  return GRUB_ERR_NONE;
}

static grub_err_t
shadow_fini (struct grub_term_output *term) {
  return GRUB_ERR_NONE;
}

/* Key injection ring buffer for insertKey support.
 * GRUB's input model is polling-based: grub_getkey() iterates all
 * registered input terminals calling getkey() until one returns a key.
 * We provide a grub_term_input whose getkey() drains this buffer. */

#define KEY_BUFFER_SIZE 16
static int keyBuffer[KEY_BUFFER_SIZE];
static unsigned int keyBufferHead;
static unsigned int keyBufferTail;
static unsigned int keyBufferCount;

static void
pushKey (int grubKey) {
  if (keyBufferCount >= KEY_BUFFER_SIZE) {
    logMessage(LOG_WARNING, "GRUB screen driver: key buffer full");
    return;
  }
  keyBuffer[keyBufferHead] = grubKey;
  keyBufferHead = (keyBufferHead + 1) % KEY_BUFFER_SIZE;
  keyBufferCount++;
}

static int
popKey (void) {
  if (keyBufferCount == 0) return GRUB_TERM_NO_KEY;
  int key = keyBuffer[keyBufferTail];
  keyBufferTail = (keyBufferTail + 1) % KEY_BUFFER_SIZE;
  keyBufferCount--;
  return key;
}

/* GRUB input terminal for braille key input.
 *
 * This is the critical integration point between GRUB and BRLTTY.
 * GRUB's menu loop calls grub_getkey_noblock(), which iterates all
 * registered input terminals. Our getkey() is called on each iteration,
 * giving us the opportunity to:
 *
 * 1. Run one non-blocking BRLTTY poll cycle (brlttyWait with 0 timeout).
 *    This fires expired alarms (display refresh, keepalive), processes
 *    pending USB I/O (reading braille key events), and updates the
 *    braille display with current shadow buffer contents.
 *
 * 2. Return any key that BRLTTY translated from braille input and
 *    injected via insertKey() → pushKey(). If no key is pending,
 *    return GRUB_TERM_NO_KEY.
 *
 * This cooperative polling model requires no threads or interrupts.
 * It's the same approach BRLTTY uses on DOS. */
static struct grub_term_input brlttyInputTerminal;

static int
brlttyInput_getkey (struct grub_term_input *term) {
  brlttyWait(0);
  return popKey();
}

/* Convert a BRLTTY ScreenKey to a GRUB key code.
 * BRLTTY uses wchar_t codepoints for printable characters and
 * SCR_KEY_* constants (in the 0xF800 range) for special keys. */
static int
screenKeyToGrub (ScreenKey key) {
  int modifiers = 0;

  if (key & SCR_KEY_CONTROL) modifiers |= GRUB_TERM_CTRL;
  if (key & SCR_KEY_ALT_LEFT) modifiers |= GRUB_TERM_ALT;
  if (key & SCR_KEY_SHIFT) modifiers |= GRUB_TERM_SHIFT;

  int ch = key & SCR_KEY_CHAR_MASK;

  if (isSpecialKey(key)) {
    int grubKey;
    switch (ch) {
      case SCR_KEY_ENTER:        grubKey = '\r'; break;
      case SCR_KEY_TAB:          grubKey = GRUB_TERM_TAB; break;
      case SCR_KEY_BACKSPACE:    grubKey = GRUB_TERM_BACKSPACE; break;
      case SCR_KEY_ESCAPE:       grubKey = GRUB_TERM_ESC; break;
      case SCR_KEY_CURSOR_LEFT:  grubKey = GRUB_TERM_KEY_LEFT; break;
      case SCR_KEY_CURSOR_RIGHT: grubKey = GRUB_TERM_KEY_RIGHT; break;
      case SCR_KEY_CURSOR_UP:    grubKey = GRUB_TERM_KEY_UP; break;
      case SCR_KEY_CURSOR_DOWN:  grubKey = GRUB_TERM_KEY_DOWN; break;
      case SCR_KEY_PAGE_UP:      grubKey = GRUB_TERM_KEY_PPAGE; break;
      case SCR_KEY_PAGE_DOWN:    grubKey = GRUB_TERM_KEY_NPAGE; break;
      case SCR_KEY_HOME:         grubKey = GRUB_TERM_KEY_HOME; break;
      case SCR_KEY_END:          grubKey = GRUB_TERM_KEY_END; break;
      case SCR_KEY_INSERT:       grubKey = GRUB_TERM_KEY_INSERT; break;
      case SCR_KEY_DELETE:       grubKey = GRUB_TERM_KEY_DC; break;
      default:
        /* Function keys F1-F12. */
        if (ch >= SCR_KEY_F1 && ch <= SCR_KEY_F12) {
          /* GRUB_TERM_KEY_F1 through F12 are sequential. */
          grubKey = GRUB_TERM_KEY_F1 + (ch - SCR_KEY_F1);
        } else {
          logMessage(LOG_WARNING, "GRUB screen: unsupported special key 0x%04X", ch);
          return -1;
        }
        break;
    }
    return grubKey | modifiers;
  }

  /* Printable character. Apply control modifier. */
  if (modifiers & GRUB_TERM_CTRL) {
    if (ch >= 'a' && ch <= 'z') ch -= 'a' - 1;
    else if (ch >= 'A' && ch <= 'Z') ch -= 'A' - 1;
  }

  if (key & SCR_KEY_UPPER) {
    if (ch >= 'a' && ch <= 'z') ch -= 'a' - 'A';
  }

  return ch | modifiers;
}

/* BRLTTY screen driver interface implementation. */

static int
processParameters_GrubScreen (char **parameters) {
  return 1;
}

static int
construct_GrubScreen (void) {
  /* Find the first active GRUB terminal output to wrap. */
  realTerminal = grub_term_outputs;
  if (!realTerminal) {
    logMessage(LOG_ERR, "GRUB screen: no active terminal output found");
    return 0;
  }

  /* Query dimensions from the real terminal. */
  struct grub_term_coordinate wh = realTerminal->getwh(realTerminal);
  screenCols = wh.x ? wh.x : 80;
  screenRows = wh.y ? wh.y : 25;

  if (screenCols > GRUB_SCREEN_COLS_MAX) screenCols = GRUB_SCREEN_COLS_MAX;
  if (screenRows > GRUB_SCREEN_ROWS_MAX) screenRows = GRUB_SCREEN_ROWS_MAX;

  currentAttributes = GRUB_TERM_DEFAULT_NORMAL_COLOR;
  cursorRow = 0;
  cursorCol = 0;
  clearShadowBuffer();

  /* Read current cursor position from the real terminal. */
  if (realTerminal->getxy) {
    struct grub_term_coordinate pos = realTerminal->getxy(realTerminal);
    cursorCol = pos.x;
    cursorRow = pos.y;
  }

  /* Set up the shadow terminal to intercept all output. */
  memset(&shadowTerminal, 0, sizeof(shadowTerminal));
  shadowTerminal.name = "brltty_shadow";
  shadowTerminal.init = shadow_init;
  shadowTerminal.fini = shadow_fini;
  shadowTerminal.putchar = shadow_putchar;
  shadowTerminal.getcharwidth = shadow_getcharwidth;
  shadowTerminal.getwh = shadow_getwh;
  shadowTerminal.getxy = shadow_getxy;
  shadowTerminal.gotoxy = shadow_gotoxy;
  shadowTerminal.cls = shadow_cls;
  shadowTerminal.setcolorstate = shadow_setcolorstate;
  shadowTerminal.setcursor = shadow_setcursor;
  shadowTerminal.refresh = shadow_refresh;
  shadowTerminal.flags = realTerminal->flags;

  /* Remove the real terminal from the active list and insert our shadow.
   * We keep a pointer to the real terminal for forwarding. */
  grub_term_unregister_output(realTerminal);
  grub_term_register_output_active("brltty_shadow", &shadowTerminal);

  /* Set up the input terminal for key injection. */
  memset(&brlttyInputTerminal, 0, sizeof(brlttyInputTerminal));
  brlttyInputTerminal.name = "brltty_keys";
  brlttyInputTerminal.getkey = brlttyInput_getkey;
  keyBufferHead = 0;
  keyBufferTail = 0;
  keyBufferCount = 0;
  grub_term_register_input_active("brltty_keys", &brlttyInputTerminal);

  logMessage(LOG_INFO, "GRUB screen driver: shadow terminal active (%ux%u)",
             screenCols, screenRows);
  return 1;
}

static void
destruct_GrubScreen (void) {
  /* Restore the original terminal. */
  grub_term_unregister_output(&shadowTerminal);
  if (realTerminal) {
    grub_term_register_output_active(realTerminal->name, realTerminal);
    realTerminal = NULL;
  }

  grub_term_unregister_input(&brlttyInputTerminal);

  logMessage(LOG_INFO, "GRUB screen driver: shadow terminal removed");
}

static void
describe_GrubScreen (ScreenDescription *description) {
  description->cols = screenCols;
  description->rows = screenRows;
  description->posx = cursorCol;
  description->posy = cursorRow;
  description->number = 1;
  description->hasCursor = 1;
}

static int
readCharacters_GrubScreen (const ScreenBox *box, ScreenCharacter *buffer) {
  if (!validateScreenBox(box, screenCols, screenRows)) return 0;

  for (int row = box->top; row < box->top + box->height; row++) {
    for (int col = box->left; col < box->left + box->width; col++) {
      buffer->text = shadowBuffer[row][col].text;
      buffer->color.vgaAttributes = shadowBuffer[row][col].attributes;
      buffer++;
    }
  }

  return 1;
}

static int
insertKey_GrubScreen (ScreenKey key) {
  int grubKey = screenKeyToGrub(key);
  if (grubKey < 0) return 0;

  logMessage(LOG_DEBUG, "GRUB screen: inject key 0x%08X", grubKey);
  pushKey(grubKey);
  return 1;
}

static int
currentVirtualTerminal_GrubScreen (void) {
  return 1;
}

static void
scr_initialize (MainScreen *main) {
  initializeRealScreen(main);
  main->base.describe = describe_GrubScreen;
  main->base.readCharacters = readCharacters_GrubScreen;
  main->base.insertKey = insertKey_GrubScreen;
  main->base.currentVirtualTerminal = currentVirtualTerminal_GrubScreen;
  main->processParameters = processParameters_GrubScreen;
  main->construct = construct_GrubScreen;
  main->destruct = destruct_GrubScreen;
}
