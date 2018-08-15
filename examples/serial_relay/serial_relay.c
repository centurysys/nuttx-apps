/****************************************************************************
 * examples/serial_relay/serial_relay_main.c
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

#define BUFSIZE 256
static char buffer[BUFSIZE];

#ifndef max
# define max(a,b) (((a) > (b)) ? (a) : (b))
#endif

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name:
 ****************************************************************************/

static int relay(int *fds)
{
  int res, i, j, cont = 1, num = 0, last_fd = -1, wfd, in_frame = 0;
  fd_set rset;
  char escape = 0x7d;
  struct timeval timeout;

  while (cont == 1)
    {
      timeout.tv_sec = 1;
      timeout.tv_usec = 0;

      FD_ZERO(&rset);
      FD_SET(fds[0], &rset);
      FD_SET(fds[1], &rset);

      res = select(max(fds[0], fds[1]) + 1, &rset, NULL, NULL, &timeout);

      if (res == 0)
        {
          fflush(stdout);
          continue;
        }
      else if (res < 0)
        {
          break;
        }

      for (i = 0; i < 2; i++)
        {
          if (FD_ISSET(fds[i], &rset))
            {
              res = read(fds[i], buffer, 1);

              if (res > 0)
                {
                  if (last_fd == -1 || last_fd != fds[i])
                    {
                      printf("\n");
                      fflush(stdout);
                      num = 0;

                      printf("[%d -> %d]", i, (i + 1) % 2, res);

                      last_fd = fds[i];
                    }

                  for (j = 0; j < res; j++)
                    {
                      if (num > 0 && (num % 16) == 0)
                        {
                          printf("\n        ");
                        }

                      if (i == 0 && in_frame == 0 && buffer[j] == 0x7e)
                        {
                          in_frame = 1;
                        }

                      if (i == 0 && in_frame == 1 && buffer[j] <= 0x1f)
                        {
                          printf(":%02x", buffer[j]);
                        }
                      else
                        {
                          printf(" %02x", buffer[j]);
                        }

                      num++;
                    }

                  wfd = fds[(i + 1) % 2];

                  if (i == 0 && in_frame == 1 && buffer[0] <= 0x1f)
                    {
                      buffer[0] ^= 0x20;
                      write(wfd, &escape, 1);
                    }

                  write(wfd, buffer, res);
                }
              else
                {
                  cont = 0;
                  break;
                }
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
int serial_relay_main(int argc, char *argv[])
#endif
{
  int fds[2], i;
  struct termios termio[2];

  fds[0] = open("/dev/ttyS2", O_RDWR);
  fds[1] = open("/dev/ttyS3", O_RDWR);

  for (i = 0; i < 2; i++)
    {
      tcgetattr(fds[i], &termio[i]);

      termio[i].c_iflag |= IGNBRK;
      termio[i].c_oflag &= ~OPOST;
      termio[i].c_cflag |= CS8 | CREAD | CLOCAL;

      termio[i].c_cflag |= CRTSCTS;

      cfsetispeed(&termio[i], 9600);

      if (tcsetattr(fds[i], TCSANOW, &termio[i]) != 0)
        {
          syslog(LOG_ERR, "! %s: tcsetattr(%s) failed with %d.\n",
                 __FUNCTION__, i, errno);
          goto err;
        }

      tcflush(fds[i], TCIOFLUSH);
    }

  return relay(fds);

err:
  return -1;
}
