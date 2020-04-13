/****************************************************************************
 * apps/xg50/lib/libserial.c
 *
 * Originally by:
 *
 *   Copyright (C) 2018 Century Systems. All rights reserved.
 *   Author: Takeyoshi Kikuchi <kikuchi@centurysys.co.jp>
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <errno.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include "libserial.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

/****************************************************************************
 * Private Data
 ****************************************************************************/

/****************************************************************************
 * External Functions
 ****************************************************************************/

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: _wait_recv
 ****************************************************************************/

static int _wait_recv(int fd, int timeout_ms)
{
  fd_set rfds;
  struct timeval tv;
  int ret;

  FD_ZERO(&rfds);
  FD_SET(fd, &rfds);

  if (timeout_ms >= 0)
    {
      tv.tv_sec = timeout_ms / 1000;
      tv.tv_usec = (timeout_ms % 1000) * 1000;

      ret = select(fd + 1, &rfds, NULL, NULL, &tv);
    }
  else
    {
      ret = select(fd + 1, &rfds, NULL, NULL, NULL);
    }

  return ret;
}

/****************************************************************************
 * Name: _read
 ****************************************************************************/

static int _read(int fd, char *buf, int len, int timeout_ms)
{
  int ret;

  ret = _wait_recv(fd, timeout_ms);

  if (ret == 1)
    {
      ret = read(fd, buf, len);
    }
  else if (ret < 0)
    {
      syslog(LOG_ERR, "! %s: select() failed(%d) with %d, '%s'\n",
             __FUNCTION__, ret, errno, strerror(errno));
    }

  return ret;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: serial_open
 ****************************************************************************/

serial_t *serial_open(char *port, int baudrate, int rtscts)
{
  int errno_backup;
  serial_t *ser;
  struct termios termio;

  if (!(ser = zalloc(sizeof(serial_t))))
    {
      printf("! %s: zalloc() failed\n", __FUNCTION__);
      return NULL;
    }

  if ((ser->fd = open(port, O_RDWR)) < 0)
    {
      errno_backup = errno;
      syslog(LOG_ERR, "! %s: open(%s) failed with %d.\n",
             __FUNCTION__, port, errno_backup);
      printf("! %s: open(%s) failed with %d\n", __FUNCTION__, port, errno_backup);
      goto errret1;
    }

  tcgetattr(ser->fd, &ser->oldtermio);
  memcpy(&termio, &ser->oldtermio, sizeof(struct termios));

  termio.c_iflag |= IGNBRK;
  termio.c_oflag &= ~OPOST;
  termio.c_cflag |= CS8 | CREAD | CLOCAL;

  if (rtscts)
    {
      termio.c_cflag |= CRTSCTS;
    }
  else
    {
      termio.c_cflag &= ~CRTSCTS;
    }

  cfsetispeed(&termio, baudrate);

  if (tcsetattr(ser->fd, TCSANOW, &termio) != 0)
    {
      errno_backup = errno;
      syslog(LOG_ERR, "! %s: tcsetattr(%s) failed with %d.\n",
             __FUNCTION__, port, errno);
      printf("! %s: tcsetattr(%s) failed with %d.\n",
             __FUNCTION__, port, errno_backup);
      goto errret2;
    }

  //tcflush(ser->fd, TCIOFLUSH);

  return ser;

errret2:
  tcsetattr(ser->fd, TCSANOW, &ser->oldtermio);
  close(ser->fd);
errret1:
  free(ser);

  return NULL;
}

/****************************************************************************
 * Name: serial_close
 ****************************************************************************/

int serial_close(serial_t *ser)
{
  if (!ser)
    {
      return -1;
    }

  /* restore termios */
  tcsetattr(ser->fd, TCSANOW, &ser->oldtermio);
  close(ser->fd);

  return 0;
}

/****************************************************************************
 * Name: ser_fileno
 ****************************************************************************/

int serial_fileno(serial_t *ser)
{
  if (!ser)
    {
      return -1;
    }

  return ser->fd;
}

/****************************************************************************
 * Name: ser_wait_recv
 ****************************************************************************/

int ser_wait_recv(serial_t *ser, int timeout_ms)
{
  if (!ser)
    {
      return -1;
    }

  return _wait_recv(ser->fd, timeout_ms);
}

/****************************************************************************
 * Name: ser_read
 ****************************************************************************/

int ser_read(serial_t *ser, char *buf, int buflen, int timeout_ms)
{
  char *ptr;
  int res, rest;

  if (!ser)
    {
      printf("! %s: serial_t is invalid.\n", __FUNCTION__);
      return -1;
    }

  ptr = buf;
  *ptr = '\0';

  while (1)
    {
      rest = buflen - ((int) (ptr - buf));

      if (rest < 1)
        {
          break;
        }

      res = _read(ser->fd, ptr, rest, timeout_ms);

      if (res <= 0)
        {
          printf("! %s: timeouted (%d)\n", __FUNCTION__, res);
          break;
        }

      ptr += res;
    }

  res = (int) (ptr - buf);

  return res;
}

/****************************************************************************
 * Name: ser_readline
 ****************************************************************************/

int ser_readline(serial_t *ser, char *buf, int buflen, char delim, int timeout_ms)
{
  char *ptr;
  int res, rest;

  if (!ser)
    {
      printf("! %s: serial_t is invalid.\n", __FUNCTION__);
      return -1;
    }

  ptr = buf;
  *ptr = '\0';

  while (1)
    {
      rest = buflen - ((int) (ptr - buf));

      if (rest <= 1)
        {
          break;
        }

      res = _read(ser->fd, ptr, 1, timeout_ms);

      if (res <= 0)
        {
          printf("! %s: timeouted (%d)\n", __FUNCTION__, res);
          break;
        }

      ptr += res;
      *ptr = '\0';

      if (*(ptr - 1) == delim)
        {
          break;
        }
    }

  res = (int) (ptr - buf);

  return res;
}

/****************************************************************************
 * Name: ser_wait_recv
 ****************************************************************************/

int ser_write(serial_t *ser, const char *buf, int buflen)
{
  int res;

  if (!ser)
    {
      printf("! %s: serial_t is invalid.\n", __FUNCTION__);
      return -1;
    }

  res = write(ser->fd, buf, buflen);

  return res;
}

/****************************************************************************
 * Name: ser_set_dtr
 ****************************************************************************/

int ser_set_dtr(serial_t *ser, bool dtr)
{
  int res, modem;

  modem = dtr ? TIOCM_DTR : 0;
  printf("%s: modem = 0x%08x\n", __FUNCTION__, modem);
  res = ioctl(ser->fd, TIOCMSET, (unsigned long) &modem);

  return res;
}
