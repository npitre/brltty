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
 * USB platform implementation for the GRUB bootloader.
 *
 * Maps BRLTTY's platform USB functions to GRUB's USB subsystem API.
 * GRUB provides USB host controller drivers (UHCI, OHCI, EHCI) and
 * a device-level API for enumeration, control transfers, and bulk I/O.
 *
 * The key GRUB functions used are:
 *   grub_usb_iterate()            - enumerate connected USB devices
 *   grub_usb_set_configuration()  - select a device configuration
 *   grub_usb_control_msg()        - send/receive control transfers
 *   grub_usb_bulk_read_extended() - read from a bulk endpoint with timeout
 *   grub_usb_bulk_write()         - write to a bulk endpoint
 *   grub_usb_clear_halt()         - clear endpoint halt/stall condition
 */

#include "prologue.h"

#include <stdio.h>
#include <errno.h>

#include <grub/usb.h>
#include <grub/usbtrans.h>
#include <grub/mm.h>
#include <grub/time.h>

#include "log.h"
#include "io_usb.h"
#include "usb_internal.h"

/* Format up to 16 bytes as hex into a caller-supplied buffer.
 * Returns the buffer pointer for convenience in logMessage() calls. */
static char *
formatHex (char *out, size_t outSize, const void *data, size_t dataLen) {
  static const char digits[] = "0123456789ABCDEF";
  const unsigned char *p = data;
  size_t n = dataLen < 16 ? dataLen : 16;
  size_t pos = 0;

  for (size_t i = 0; i < n && pos + 3 < outSize; i++) {
    out[pos++] = digits[p[i] >> 4];
    out[pos++] = digits[p[i] & 0x0F];
    if (i + 1 < n) out[pos++] = ' ';
  }
  out[pos] = '\0';
  return out;
}

struct UsbDeviceExtensionStruct {
  grub_usb_device_t grubDevice;
};

struct UsbEndpointExtensionStruct {
  /* GRUB endpoints are identified by their descriptor within the device,
   * found via the interface's endpoint array. We cache the GRUB endpoint
   * descriptor pointer for bulk transfer calls. */
  struct grub_usb_desc_endp *grubEndpoint;
};

int
usbDisableAutosuspend (UsbDevice *device) {
  /* No power management in a bootloader. */
  return 1;
}

int
usbSetConfiguration (UsbDevice *device, unsigned char configuration) {
  grub_usb_device_t dev = device->extension->grubDevice;
  grub_usb_err_t err;

  logMessage(LOG_DEBUG, "USB: set configuration %u", configuration);
  grub_errno = GRUB_ERR_NONE;
  err = grub_usb_set_configuration(dev, configuration);
  if (err != GRUB_USB_ERR_NONE) {
    logMessage(LOG_ERR, "GRUB USB set configuration %u failed: %d",
               configuration, err);
    errno = EIO;
    return 0;
  }
  logMessage(LOG_DEBUG, "USB: set configuration %u OK", configuration);

  return 1;
}

int
usbClaimInterface (UsbDevice *device, unsigned char interface) {
  /* No competing drivers in GRUB — claim always succeeds. */
  return 1;
}

int
usbReleaseInterface (UsbDevice *device, unsigned char interface) {
  /* Nothing to release. */
  return 1;
}

int
usbSetAlternative (
  UsbDevice *device,
  unsigned char interface,
  unsigned char alternative
) {
  grub_usb_device_t dev = device->extension->grubDevice;
  grub_usb_err_t err;

  err = grub_usb_control_msg(dev,
    GRUB_USB_REQTYPE_STANDARD | GRUB_USB_REQTYPE_TARGET_INTERF | GRUB_USB_REQTYPE_OUT,
    GRUB_USB_REQ_SET_INTERFACE, alternative, interface, 0, NULL);

  if (err != GRUB_USB_ERR_NONE) {
    logMessage(LOG_ERR, "GRUB USB set alternative %u on interface %u failed: %d",
               alternative, interface, err);
    errno = EIO;
    return 0;
  }

  return 1;
}

int
usbResetDevice (UsbDevice *device) {
  /* GRUB does not provide a device reset API. */
  errno = ENOSYS;
  return 0;
}

int
usbClearHalt (UsbDevice *device, unsigned char endpointAddress) {
  grub_usb_device_t dev = device->extension->grubDevice;
  grub_usb_err_t err;

  err = grub_usb_clear_halt(dev, endpointAddress);
  if (err != GRUB_USB_ERR_NONE) {
    logMessage(LOG_ERR, "GRUB USB clear halt on endpoint 0x%02X failed: %d",
               endpointAddress, err);
    errno = EIO;
    return 0;
  }

  return 1;
}

ssize_t
usbControlTransfer (
  UsbDevice *device,
  uint8_t direction,
  uint8_t recipient,
  uint8_t type,
  uint8_t request,
  uint16_t value,
  uint16_t index,
  void *buffer,
  uint16_t length,
  int timeout
) {
  grub_usb_device_t dev = device->extension->grubDevice;
  grub_usb_err_t err;

  /* Build the GRUB reqtype byte from BRLTTY's decomposed fields. */
  grub_uint8_t reqtype = 0;

  switch (recipient) {
    case UsbControlRecipient_Device:    reqtype |= GRUB_USB_REQTYPE_TARGET_DEV;    break;
    case UsbControlRecipient_Interface: reqtype |= GRUB_USB_REQTYPE_TARGET_INTERF; break;
    case UsbControlRecipient_Endpoint:  reqtype |= GRUB_USB_REQTYPE_TARGET_ENDP;   break;
    case UsbControlRecipient_Other:     reqtype |= GRUB_USB_REQTYPE_TARGET_OTHER;  break;
  }

  switch (type) {
    case UsbControlType_Standard: reqtype |= GRUB_USB_REQTYPE_STANDARD; break;
    case UsbControlType_Class:    reqtype |= GRUB_USB_REQTYPE_CLASS;    break;
    case UsbControlType_Vendor:   reqtype |= GRUB_USB_REQTYPE_VENDOR;   break;
  }

  if (direction == UsbEndpointDirection_Input) {
    reqtype |= GRUB_USB_REQTYPE_IN;
  } else {
    reqtype |= GRUB_USB_REQTYPE_OUT;
  }

  logMessage(LOG_DEBUG, "USB: control transfer reqtype=0x%02X request=0x%02X value=0x%04X index=0x%04X len=%u",
             reqtype, request, value, index, length);
  if (length > 0 && buffer && direction == UsbEndpointDirection_Output) {
    char hex[3 * 16 + 1];
    logMessage(LOG_DEBUG, "USB: control OUT data=[%s]",
               formatHex(hex, sizeof(hex), buffer, length));
  }
  grub_errno = GRUB_ERR_NONE;
  err = grub_usb_control_msg(dev, reqtype, request, value, index,
                             length, (char *)buffer);
  if (err != GRUB_USB_ERR_NONE) {
    logMessage(LOG_ERR, "GRUB USB control transfer failed: "
               "reqtype=0x%02X request=0x%02X err=%d",
               reqtype, request, err);
    errno = EIO;
    return -1;
  }
  if (length > 0 && buffer && direction == UsbEndpointDirection_Input) {
    char hex[3 * 16 + 1];
    logMessage(LOG_DEBUG, "USB: control IN data=[%s]",
               formatHex(hex, sizeof(hex), buffer, length));
  }
  logMessage(LOG_DEBUG, "USB: control transfer OK");

  return length;
}

void *
usbSubmitRequest (
  UsbDevice *device,
  unsigned char endpointAddress,
  void *buffer,
  size_t length,
  void *context
) {
  /* Async I/O is not used in the GRUB polling model.
   * Return NULL to signal that async submission is not available;
   * callers fall back to synchronous reads via usbReadEndpoint. */
  logUnsupportedFunction();
  return NULL;
}

int
usbCancelRequest (UsbDevice *device, void *request) {
  logUnsupportedFunction();
  return 0;
}

void *
usbReapResponse (
  UsbDevice *device,
  unsigned char endpointAddress,
  UsbResponse *response,
  int wait
) {
  logUnsupportedFunction();
  return NULL;
}

int
usbMonitorInputEndpoint (
  UsbDevice *device, unsigned char endpointNumber,
  AsyncMonitorCallback *callback, void *data
) {
  /* No async monitoring — BRLTTY uses synchronous polling in GRUB. */
  return 0;
}

/* Find the GRUB endpoint descriptor for a given BRLTTY endpoint number
 * and direction within the currently claimed interface. */
static struct grub_usb_desc_endp *
grubFindEndpoint (UsbDevice *device, unsigned char endpointNumber, unsigned char direction) {
  grub_usb_device_t dev = device->extension->grubDevice;
  const UsbInterfaceDescriptor *brlttyInterface = device->interface;

  if (!brlttyInterface) {
    logMessage(LOG_ERR, "GRUB USB: no interface claimed");
    return NULL;
  }

  unsigned char interfaceNumber = brlttyInterface->bInterfaceNumber;
  unsigned char targetAddress = endpointNumber | direction;

  logMessage(LOG_DEBUG, "USB: findEndpoint: interface=%u target=0x%02X",
             interfaceNumber, targetAddress);

  /* Search through all configurations and interfaces for the matching endpoint. */
  for (int config = 0; config < dev->descdev.configcnt; config++) {
    struct grub_usb_desc_if *interf =
      dev->config[config].interf[interfaceNumber].descif;

    if (!interf) continue;

    for (int ep = 0; ep < interf->endpointcnt; ep++) {
      struct grub_usb_desc_endp *endp =
        &dev->config[config].interf[interfaceNumber].descendp[ep];

      if (endp->endp_addr == targetAddress) {
        return endp;
      }
    }
  }

  logMessage(LOG_ERR, "GRUB USB: endpoint 0x%02X not found", targetAddress);
  return NULL;
}

ssize_t
usbReadEndpoint (
  UsbDevice *device,
  unsigned char endpointNumber,
  void *buffer,
  size_t length,
  int timeout
) {
  grub_usb_device_t dev = device->extension->grubDevice;
  grub_usb_err_t err;
  grub_size_t actual = 0;

  struct grub_usb_desc_endp *endp =
    grubFindEndpoint(device, endpointNumber, 0x80);
  if (!endp) {
    errno = ENOENT;
    return -1;
  }

  logMessage(LOG_DEBUG, "USB: bulk read ep=%u len=%zu timeout=%d addr=0x%02X toggle=%d maxpkt=%u",
             endpointNumber, length, timeout, endp->endp_addr,
             dev->toggle[endp->endp_addr], endp->maxpacket);
  grub_errno = GRUB_ERR_NONE;
  err = grub_usb_bulk_read_extended(dev, endp, length, (char *)buffer,
                                    timeout, &actual);
  if (err == GRUB_USB_ERR_TIMEOUT) {
    logMessage(LOG_DEBUG, "USB: bulk read ep=%u timeout (actual=%zu) toggle=%d",
               endpointNumber, actual, dev->toggle[endp->endp_addr]);
    errno = EAGAIN;
    return -1;
  }

  if (err != GRUB_USB_ERR_NONE) {
    logMessage(LOG_ERR, "USB: bulk read ep=%u failed: err=%d actual=%zu",
               endpointNumber, err, actual);
    errno = EIO;
    return -1;
  }

  {
    char hex[3 * 16 + 1];
    logMessage(LOG_DEBUG, "USB: bulk read ep=%u OK actual=%zu toggle=%d data=[%s]",
               endpointNumber, actual, dev->toggle[endp->endp_addr],
               formatHex(hex, sizeof(hex), buffer, actual));
  }

  return actual;
}

ssize_t
usbWriteEndpoint (
  UsbDevice *device,
  unsigned char endpointNumber,
  const void *buffer,
  size_t length,
  int timeout
) {
  grub_usb_device_t dev = device->extension->grubDevice;
  grub_usb_err_t err;

  struct grub_usb_desc_endp *endp =
    grubFindEndpoint(device, endpointNumber, 0x00);
  if (!endp) {
    errno = ENOENT;
    return -1;
  }

  {
    char hex[3 * 16 + 1];
    logMessage(LOG_DEBUG, "USB: bulk write ep=%u len=%zu addr=0x%02X data=[%s]",
               endpointNumber, length, endp->endp_addr,
               formatHex(hex, sizeof(hex), buffer, length));
  }
  grub_errno = GRUB_ERR_NONE;
  err = grub_usb_bulk_write(dev, endp, length, (char *)buffer);
  if (err != GRUB_USB_ERR_NONE) {
    logMessage(LOG_ERR, "GRUB USB bulk write on endpoint %u failed: %d",
               endpointNumber, err);
    errno = EIO;
    return -1;
  }
  logMessage(LOG_DEBUG, "USB: bulk write ep=%u OK", endpointNumber);

  /* Let the device process the command before we attempt to read
   * a response.  GRUB's USB stack performs synchronous transfers
   * with no kernel-level queuing, so without this gap the next
   * bulk read can arrive before the device has prepared its reply. */
  grub_millisleep(50);

  return length;
}

int
usbReadDeviceDescriptor (UsbDevice *device) {
  grub_usb_device_t dev = device->extension->grubDevice;
  struct grub_usb_desc_device *grubDesc = &dev->descdev;

  /* Copy fields from GRUB's device descriptor to BRLTTY's format.
   * Both are standard USB device descriptors but with different struct types. */
  UsbDeviceDescriptor *desc = &device->descriptor;

  desc->bLength = grubDesc->length;
  desc->bDescriptorType = grubDesc->type;
  desc->bcdUSB = grubDesc->usbrel;
  desc->bDeviceClass = grubDesc->class;
  desc->bDeviceSubClass = grubDesc->subclass;
  desc->bDeviceProtocol = grubDesc->protocol;
  desc->bMaxPacketSize0 = grubDesc->maxsize0;
  desc->idVendor = grubDesc->vendorid;
  desc->idProduct = grubDesc->prodid;
  desc->bcdDevice = grubDesc->devrel;
  desc->iManufacturer = grubDesc->strvendor;
  desc->iProduct = grubDesc->strprod;
  desc->iSerialNumber = grubDesc->strserial;
  desc->bNumConfigurations = grubDesc->configcnt;

  return 1;
}

int
usbAllocateEndpointExtension (UsbEndpoint *endpoint) {
  UsbEndpointExtension *eptx;

  if (!(eptx = malloc(sizeof(*eptx)))) {
    logMallocError();
    return 0;
  }

  memset(eptx, 0, sizeof(*eptx));
  endpoint->extension = eptx;
  return 1;
}

void
usbDeallocateEndpointExtension (UsbEndpointExtension *eptx) {
  free(eptx);
}

void
usbDeallocateDeviceExtension (UsbDeviceExtension *devx) {
  free(devx);
}

typedef struct {
  UsbDeviceChooser *chooser;
  UsbChooseChannelData *data;
  UsbDevice *device;
} GrubUsbSearchData;

static int
grubUsbDeviceIterator (grub_usb_device_t dev, void *closure) {
  GrubUsbSearchData *search = closure;

  logMessage(LOG_DEBUG, "USB iterate: device %p, initialized=%d",
             dev, dev->initialized);

  if (!dev->initialized) {
    grub_usb_err_t err = grub_usb_device_initialize(dev);
    if (err != GRUB_USB_ERR_NONE) {
      logMessage(LOG_DEBUG, "USB iterate: device_initialize failed: %d", err);
      return 0; /* skip this device, continue iterating */
    }
  }

  logMessage(LOG_DEBUG, "USB iterate: vendor=0x%04x product=0x%04x",
             dev->descdev.vendorid, dev->descdev.prodid);

  UsbDeviceExtension *devx = malloc(sizeof(*devx));
  if (!devx) {
    logMallocError();
    return 0;
  }

  devx->grubDevice = dev;

  UsbDevice *device = usbTestDevice(devx, search->chooser, search->data);
  if (device) {
    search->device = device;
    return 1; /* found — stop iterating */
  }

  free(devx);
  return 0; /* continue iterating */
}

UsbDevice *
usbFindDevice (UsbDeviceChooser *chooser, UsbChooseChannelData *data) {
  GrubUsbSearchData search = {
    .chooser = chooser,
    .data = data,
    .device = NULL
  };

  logMessage(LOG_DEBUG, "USB: scanning for devices...");
  grub_errno = GRUB_ERR_NONE;  /* clear stale errors from file operations */
  grub_usb_iterate(grubUsbDeviceIterator, &search);
  if (!search.device) {
    logMessage(LOG_DEBUG, "USB: no matching device found");
  }
  return search.device;
}

void
usbForgetDevices (void) {
}
