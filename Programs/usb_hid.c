/*
 * BRLTTY - A background process providing access to the console screen (when in
 *          text mode) for a blind person using a refreshable braille display.
 *
 * Copyright (C) 1995-2021 by The BRLTTY Developers.
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

#include <errno.h>

#include "log.h"
#include "io_usb.h"
#include "hid.h"
#include "bitfield.h"

const UsbHidDescriptor *
usbHidDescriptor (UsbDevice *device) {
  const UsbDescriptor *descriptor = NULL;

  while (usbNextDescriptor(device, &descriptor)) {
    if (descriptor->header.bDescriptorType == UsbDescriptorType_HID) {
      return &descriptor->hid;
    }
  }

  logMessage(LOG_WARNING, "USB: HID descriptor not found");
  errno = ENOENT;
  return NULL;
}

ssize_t
usbHidGetItems (
  UsbDevice *device,
  unsigned char interface,
  unsigned char number,
  unsigned char **items,
  int timeout
) {
  const UsbHidDescriptor *hid = usbHidDescriptor(device);

  if (hid) {
    if (number < hid->bNumDescriptors) {
      const UsbClassDescriptor *descriptor = &hid->descriptors[number];
      uint16_t length = getLittleEndian16(descriptor->wDescriptorLength);
      void *buffer = malloc(length);

      if (buffer) {
        ssize_t result = usbControlRead(
          device, UsbControlRecipient_Interface, UsbControlType_Standard,
          UsbStandardRequest_GetDescriptor,
          (descriptor->bDescriptorType << 8) | interface,
          number, buffer, length, timeout
        );

        if (result != -1) {
          *items = buffer;
          return result;
        }

        free(buffer);
      } else {
        logMallocError();
      }
    } else {
      logMessage(LOG_WARNING, "USB report descriptor not found: %u[%u]",
                 interface, number);
    }
  }

  return -1;
}

ssize_t
usbHidGetReport (
  UsbDevice *device,
  unsigned char interface,
  unsigned char report,
  void *buffer,
  uint16_t length,
  int timeout
) {
  return usbControlRead(device,
    UsbControlRecipient_Interface, UsbControlType_Class,
    UsbHidRequest_GetReport,
    (UsbHidReportType_Input << 8) | report, interface,
    buffer, length, timeout
  );
}

ssize_t
usbHidSetReport (
  UsbDevice *device,
  unsigned char interface,
  unsigned char report,
  const void *buffer,
  uint16_t length,
  int timeout
) {
  return usbControlWrite(device,
    UsbControlRecipient_Interface, UsbControlType_Class,
    UsbHidRequest_SetReport,
    (UsbHidReportType_Output << 8) | report, interface,
    buffer, length, timeout
  );
}

ssize_t
usbHidGetFeature (
  UsbDevice *device,
  unsigned char interface,
  unsigned char report,
  void *buffer,
  uint16_t length,
  int timeout
) {
  return usbControlRead(device,
    UsbControlRecipient_Interface, UsbControlType_Class,
    UsbHidRequest_GetReport,
    (UsbHidReportType_Feature << 8) | report, interface,
    buffer, length, timeout
  );
}

ssize_t
usbHidSetFeature (
  UsbDevice *device,
  unsigned char interface,
  unsigned char report,
  const void *buffer,
  uint16_t length,
  int timeout
) {
  return usbControlWrite(device,
    UsbControlRecipient_Interface, UsbControlType_Class,
    UsbHidRequest_SetReport,
    (UsbHidReportType_Feature << 8) | report, interface,
    buffer, length, timeout
  );
}
