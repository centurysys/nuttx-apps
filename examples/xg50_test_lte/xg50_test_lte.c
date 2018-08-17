/****************************************************************************
 * examples/xg50_test_lte/xg50_test_lte.c
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

/****************************************************************************
 * Name:
 ****************************************************************************/

static const char *const cmd_CSQ = "AT+CSQ\r\n";

static int lte_test(int fd)
{
  char wbuf[64], rbuf[64 + 1];
  int len, wlen, i, res;
  fd_set rset;
  struct timeval timeout;

  printf("* Write AT command to LTE module... ");

  wlen = strlen(cmd_CSQ);
  len = write(fd, cmd_CSQ, wlen);

  if (len < wlen)
    {
      printf("write failed.\n");
      return -1;
    }
  else
    {
      printf("OK\n");
    }

  while (1)
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

      len = read(fd, rbuf, 64);

      if (len <= 0)
        {
          break;
        }

      for (i = 0; i < len; i++)
        {
          if (rbuf[i] == 0x0a || rbuf[i] == 0x0d ||
              rbuf[i] >= 0x20 || rbuf[i] < 0x7f)
            {
              printf("%c", rbuf[i]);
            }
          else
            {
              printf("[%02x]", rbuf[i]);
            }
        }
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
int test_lte_main(int argc, char *argv[])
#endif
{
  int fd, res = -1;
  struct termios termio[2];

  fd = open("/dev/ttyS3", O_RDWR);

  tcgetattr(fd, &termio[0]);

  memcpy(&termio[1], &termio[0], sizeof(struct termios));

  termio[1].c_iflag |= IGNBRK;
  termio[1].c_oflag &= ~OPOST;
  termio[1].c_cflag |= CS8 | CREAD | CLOCAL;

  cfsetispeed(&termio[1], 19200);

  if (tcsetattr(fd, TCSANOW, &termio[1]) != 0)
    {
      syslog(LOG_ERR, "! %s: tcsetattr() failed with %d.\n",
             __FUNCTION__, errno);
      goto out;
    }

  tcflush(fd, TCIOFLUSH);

  res = lte_test(fd);

out:
  tcsetattr(fd, TCSANOW, &termio[0]);
  return res;
}
