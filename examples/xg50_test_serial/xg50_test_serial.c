/****************************************************************************
 * examples/xg50_test_serial/xg50_test_serial.c
 *
 *   Copyright (C) 2008, 2011-2012 Gregory Nutt. All rights reserved.
 *   Author: Gregory Nutt <gnutt@nuttx.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name NuttX nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <syslog.h>
#include <errno.h>
#include <sys/time.h>
#include <unistd.h>

#define MBIT(modem, bit) ((modem & bit) ? "ON" : "OFF")

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static int set_get_modem(int fd, int set, int clr)
{
  int res, modem;

  res = ioctl(fd, TIOCMBIC, (unsigned long) &clr);
  if (res < 0)
    {
      printf("! TIOCMBIC failed.\n");
      return res;
    }

  res = ioctl(fd, TIOCMBIS, (unsigned long) &set);
  if (res < 0)
    {
      printf("! TIOCMBIS failed.\n");
      return res;
    }

  res = ioctl(fd, TIOCMGET, (unsigned long) &modem);

  if (res == 0)
    {
      printf(" - RTS: %s\n", MBIT(modem, TIOCM_RTS));
      printf(" - CTS: %s\n", MBIT(modem, TIOCM_CTS));
      printf(" - DTR: %s\n", MBIT(modem, TIOCM_DTR));
      printf(" - DSR: %s\n", MBIT(modem, TIOCM_DSR));
      printf(" - DCD: %s\n", MBIT(modem, TIOCM_CD));
      printf(" - RI:  %s\n", MBIT(modem, TIOCM_RI));
    }

  return res;
}

/****************************************************************************
 * Name:
 ****************************************************************************/

static int control_signal_test(int fd)
{
  int res;

  printf("* Clear RTS and DTR...\n");
  res = set_get_modem(fd, 0, TIOCM_RTS | TIOCM_DTR);

  printf("* Set   RTS and DTR...\n");
  res = set_get_modem(fd, TIOCM_RTS | TIOCM_DTR, 0);

  return res;
}

/****************************************************************************
 * Name:
 ****************************************************************************/

static int loopback_test(int fd)
{
  char wbuf[64], rbuf[64 + 1];
  int len, pos = 0, res;
  fd_set rset;
  struct timeval timeout;

  memset(wbuf, 'A', 64);

  printf("* Write to  UART: 64 bytes... ");

  len = write(fd, wbuf, 64);
  if (len < 64)
    {
      printf("write failed.\n");
      return -1;
    }
  else
    {
      printf("OK\n");
    }

  while (pos < 64)
    {
      timeout.tv_sec = 0;
      timeout.tv_usec = 100 * 1000;

      FD_ZERO(&rset);
      FD_SET(fd, &rset);

      res = select(fd + 1, &rset, NULL, NULL, &timeout);

      if (res <= 0)
        {
          break;
        }

      len = read(fd, &rbuf[pos], 64 - pos);

      if (len <= 0)
        {
          break;
        }

      pos += len;
    }

  if (pos < 64)
    {
      printf("! read length mismatch (%d)\n", pos);
      return -1;
    }
  else
    {
      printf("* Read from UART: %d bytes.\n", pos);
    }

  return 0;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * serial_relay_main
 ****************************************************************************/

#ifdef CONFIG_BUILD_KERNEL
int main(int argc, FAR char *argv[])
#else
int test_serial_main(int argc, char *argv[])
#endif
{
  int fd, res = -1;
  struct termios termio[2];

  fd = open("/dev/ttyS2", O_RDWR);

  tcgetattr(fd, &termio[0]);

  memcpy(&termio[1], &termio[0], sizeof(struct termios));

  termio[1].c_iflag |= IGNBRK;
  termio[1].c_oflag &= ~OPOST;
  termio[1].c_cflag |= CS8 | CREAD | CLOCAL;

  termio[1].c_cflag &= ~CRTSCTS;

  cfsetispeed(&termio[1], 9600);

  if (tcsetattr(fd, TCSANOW, &termio[1]) != 0)
    {
      syslog(LOG_ERR, "! %s: tcsetattr() failed with %d.\n",
             __FUNCTION__, errno);
      goto out;
    }

  tcflush(fd, TCIOFLUSH);

  res = loopback_test(fd);
  res = control_signal_test(fd);

out:
  tcsetattr(fd, TCSANOW, &termio[0]);
  return res;
}
