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
 * GRUB loadable module entry point for BRLTTY.
 *
 * This file provides the GRUB_MOD_INIT/GRUB_MOD_FINI hooks that GRUB's
 * dynamic loader calls when the module is loaded via "insmod brltty"
 * and unloaded. These macros expand to grub_mod_init()/grub_mod_fini()
 * functions — the only two symbols genmod.sh preserves after stripping.
 *
 * On init we call brlttyConstruct() with a minimal argv to start the
 * braille subsystem. On fini we call brlttyDestruct() to shut it down.
 *
 * BRLTTY normally runs its own event loop (brlttyWait in a loop), but
 * under GRUB the polling happens through the grub_term_input getkey()
 * callback registered by the screen driver — each time GRUB polls for
 * input, BRLTTY gets a chance to update the braille display.
 */

#include "prologue.h"

#include <grub/dl.h>
#include <grub/misc.h>

#include "embed.h"
#include "log.h"

GRUB_MOD_LICENSE("GPLv3+");

GRUB_MOD_INIT(brltty)
{
  static char arg0[] = "brltty";
  static char argLogLevel[] = "-l";
  static char argLogValue[] = "debug";
  static char argNoDaemon[] = "-n";
  static char *argv[] = { arg0, argLogLevel, argLogValue, argNoDaemon, NULL };
  int argc = 4;

  grub_printf("brltty: calling brlttyConstruct()\n");
  ProgramExitStatus status = brlttyConstruct(argc, argv);
  if (status == PROG_EXIT_SUCCESS) {
    grub_printf("brltty: initialized successfully\n");

    /* brlttyConstruct() schedules driver start activities via async alarms.
     * These alarms only fire when the event loop runs. Pump a few iterations
     * so the screen and braille drivers actually start. Once the screen
     * driver starts, it registers a grub_term_input whose getkey() callback
     * calls brlttyWait(0) on every GRUB input poll — sustaining the loop. */
    for (int i = 0; i < 10; i++) {
      brlttyWait(0);
    }
  } else {
    grub_printf("brltty: initialization failed (status %d)\n", status);
  }
}

GRUB_MOD_FINI(brltty)
{
  brlttyDestruct();
}
