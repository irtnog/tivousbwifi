/*
  $Id$

  Copyright (c) 2007  Stephen Smith.  All rights reserved.
  stephenws@users.sourceforge.net

  Load firmware to TiVo USB Wireless Adapter.
  Should be able to load firmware to the Broadcom chip
      BCM4320
      AirForce One. Single-Chip 802.11g USB 2.0 Transceiver
  http://www.broadcom.com/products/Wireless-LAN/802.11-Wireless-LAN-Solutions/BCM4320

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.

  $Log$
  Revision 1.1.1.1  2007/09/10 18:32:10  stephenws
  Initial revision.


*/

#include <config.h>
#include <stdint.h>
#include <stdio.h>
#include <getopt.h>
#include <errno.h>

// libusb
#include <usb.h>

/* pre-ipl
T:  Bus=04 Lev=01 Prnt=01 Port=05 Cnt=01 Dev#=  5 Spd=480 MxCh= 0
D:  Ver= 2.00 Cls=ff(vend.) Sub=00 Prot=00 MxPS=64 #Cfgs=  1
P:  Vendor=0a5c ProdID=bd11 Rev= 0.01
S:  Manufacturer=Broadcom
S:  Product=Remote Download Wireless Adapter
S:  SerialNumber=000000000001
C:* #Ifs= 1 Cfg#= 1 Atr=80 MxPwr=200mA
I:  If#= 0 Alt= 0 #EPs= 1 Cls=ff(vend.) Sub=02 Prot=ff Driver=(none)
E:  Ad=01(O) Atr=02(Bulk) MxPS= 512 Ivl=0ms
*/

/* post-ipl
T:  Bus=04 Lev=01 Prnt=01 Port=05 Cnt=01 Dev#= 16 Spd=480 MxCh= 0
D:  Ver= 2.00 Cls=02(comm.) Sub=00 Prot=00 MxPS=64 #Cfgs=  1
P:  Vendor=150a ProdID=1010 Rev= 0.06
S:  Manufacturer=Broadcom
S:  Product=USBCDC 802.11 Wireless Adapter
S:  SerialNumber=############
C:* #Ifs= 2 Cfg#= 1 Atr=80 MxPwr=100mA
I:  If#= 0 Alt= 0 #EPs= 1 Cls=02(comm.) Sub=02 Prot=ff Driver=(none)
E:  Ad=81(I) Atr=03(Int.) MxPS=  16 Ivl=1ms
I:  If#= 1 Alt= 0 #EPs= 2 Cls=0a(data ) Sub=00 Prot=00 Driver=(none)
E:  Ad=82(I) Atr=02(Bulk) MxPS= 512 Ivl=0ms
E:  Ad=03(O) Atr=02(Bulk) MxPS= 512 Ivl=125us
*/

#define USB_REQ_HOST_TO_DEVICE                  0x00
#define USB_REQ_DEVICE_TO_HOST                  0x80

#define TIVO_WNET_USB_PRE_IPL_VID               0x0a5c
#define TIVO_WNET_USB_PRE_IPL_PID               0xbd11

#define TIVO_WNET_USB_IPL_TRANSFER_SIZE         2000
#define TIVO_WNET_USB_IPL_TIMEOUT               1000

#define BULK_OUT_EP                             0x01

#pragma pack(1)
struct TIVO_WNET_USB_ipl_packet_reply
{
  uint32_t unknown;
  uint32_t ttl;
};
#pragma pack(0)

// globals
char *g_firmware = "tivownet_firmware.bin";
int g_packet_len = TIVO_WNET_USB_IPL_TRANSFER_SIZE;

// function prototypes
int TIVO_WNET_usb_ipl(usb_dev_handle *h, char *sfile);
void usage(char *appname);
int cmdline(int argc, char **argv);

int TIVO_WNET_usb_ipl(usb_dev_handle *h, char *sfile)
{
  FILE *fp;
  char *buf = (char *) 0;
  int i;
  int offset = 0;
  int result = 0;
  struct TIVO_WNET_USB_ipl_packet_reply ipl_packet_reply;

  printf("Downloading firmware %s using packet length of %d\n", sfile, g_packet_len);
  fp = fopen(sfile, "rb");
  if (fp)
  {
    // allocate a buffer for the firmware packet
    buf = malloc(g_packet_len);
    if (!buf)
    {
      printf("Out of memory, requested %d bytes!\n", g_packet_len);
      result = -ENOMEM;
    }

    for (offset = 0; result == 0; )
    {
      // read firmwre to fill a single packet
      i = fread(buf, sizeof(uint8_t), g_packet_len, fp);
      if (i <= 0)
      {
        if (i < 0)
        {
          printf("Error reading file %s\n", sfile);
        }
        break;
      }

      // send the packet to the device
      result = usb_bulk_write(h, BULK_OUT_EP, buf, i, TIVO_WNET_USB_IPL_TIMEOUT);
      if (result != i)
      {
        printf("usb_bulk_write returned %d, expected %d\n", result, i);
        break;
      }

      result = 0;
      offset += i;

      // read the status
      i = usb_control_msg(
            h,
            USB_REQ_DEVICE_TO_HOST
              |USB_TYPE_VENDOR
              |USB_RECIP_INTERFACE,
            0x00,                       // request
            0x0100,                     // value
            0x0000,                     // index
            (char *) &ipl_packet_reply, // bytes
            sizeof(ipl_packet_reply),   // size
            TIVO_WNET_USB_IPL_TIMEOUT   // timeout
      );
      if (i != sizeof(ipl_packet_reply))
      {
        printf("usb_control_msg returned %d, expected %d\n", i, sizeof(ipl_packet_reply));
        break;
      }

      if (ipl_packet_reply.ttl != offset)
      {
        printf("ipl_packet_reply.ttl %d != offset %d\n", ipl_packet_reply.ttl, offset);
        break;
      }
    }

    if (feof(fp))
    {
      result = 0;

      // reboot the device with the new firmware
      printf("Firmware loaded, rebooting device.\n");
      i = usb_control_msg(
            h,
            USB_REQ_DEVICE_TO_HOST
              |USB_TYPE_VENDOR
              |USB_RECIP_INTERFACE,
            0x06,                       // request
            0x0100,                     // value
            0x0000,                     // index
            (char *) &ipl_packet_reply, // bytes
            sizeof(ipl_packet_reply),   // size
            TIVO_WNET_USB_IPL_TIMEOUT   // timeout
      );
      if (i != sizeof(ipl_packet_reply))
      {
        printf("usb_control_msg returned %d, expected %d\n", i, sizeof(ipl_packet_reply));
      }
    }
    else if (result == 0)
    {
      result = -EIO;
    }

    if (buf)
    {
      free(buf);
    }
    fclose(fp);
  }
  else
  {
    printf("Unable to open firmware file %s\n", sfile);
    result = -EIO;
  }

  return result;
}

void usage(char *appname)
{
  printf("usage: %s [-f firmware_file] [-l #]\n", appname);
  printf("  options:\n");
  printf("    -f, --firmware [file]     firmware file to load (default: %s)\n", g_firmware);
  printf("    -l, --packet_length #     length of packets, used for testing (default: %d)\n", g_packet_len);
  printf("    -h, --help                help\n");
}

int cmdline(int argc, char **argv)
{
  static struct option long_options[] =
  {
    { "firmware", required_argument, 0, 'f' },
    { "packet_length", required_argument, 0, 'l' },
    { "help", no_argument, 0, 'h' },
    { 0, 0, 0, 0 }
  };
  int option_index = 0;
  int c;

  opterr = 0;
  while ((c = getopt_long(argc, argv, "hf:l:", long_options, &option_index)) != -1)
  {
    switch (c)
    {
    case 'h':
      usage(argv[0]);
      return 1;
    case 'f':
      g_firmware = optarg;
      break;
    case 'l':
      g_packet_len = atoi(optarg);
      break;
    default:
      if (isprint(optopt))
      {
        printf("Unknown option %c, use --help for help.\n", optopt);
      }
      else
      {
        printf("Unknown option %#x (%c).\n", optopt, c);
      }
      return -1;
    }
  }

  return opterr;
}

int main(int argc, char **argv)
{
  struct usb_bus *bus;
  int status;

  printf("Copyright (c) 2007 Stephen Smith (stephen.ws).  All rights reserved.\n\n");

  status = cmdline(argc, argv);
  if (status != 0)
  {
    return 1;
  }

  usb_init();
  usb_find_busses();
  usb_find_devices();

  for (bus = usb_get_busses(), status = 0; bus && status == 0; bus = bus->next)
  {
    struct usb_device *dev;

    for (dev = bus->devices; dev && status == 0; dev = dev->next)
    {
      if ( dev->descriptor.idVendor  == TIVO_WNET_USB_PRE_IPL_VID
        && dev->descriptor.idProduct == TIVO_WNET_USB_PRE_IPL_PID
      )
      {
        usb_dev_handle *h;

        printf("Found adapter, configuring.\n");

        h = usb_open(dev);
        if (h)
        {
          status = usb_set_configuration(h, 1);
          if (status != 0)
          {
            printf("usb_set_configuration returned %d.\n", status);
            break;
          }

          status = usb_claim_interface(h, 0);
          if (status != 0)
          {
            printf("usb_claim_interface returned %d.\n", status);
            break;
          }

          status = TIVO_WNET_usb_ipl(h, g_firmware);

          usb_close(h);
          if (status == 0)
          {
            printf("Finished, status success.\n");
            return 0;
          }
        }
      }
    }
  }

  if (status == 0)
  {
    printf("Unable to find Tivo Wireless adapter\n");
    status = -ENXIO;
  }

  printf("Finished, status failure (%d).\n", status);
  return 1;
}

