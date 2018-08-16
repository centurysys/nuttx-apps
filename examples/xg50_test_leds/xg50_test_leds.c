/****************************************************************************
 * examples/xg50_test_leds/xg50_test_leds.c
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
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/boardctl.h>
#include <fcntl.h>
#include <syslog.h>
#include <errno.h>
#include <unistd.h>

#include <nuttx/leds/tca6507.h>
#include <arch/board/boardctl.h>

#ifndef ARRAY_SIZE
#  define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#endif

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name:
 ****************************************************************************/

static int test_led_switch(void)
{
  int state;

  boardctl(BIOC_GET_INITSW, (uintptr_t) &state);
  printf(" - INIT Switch is %s.\n", (state == 0 ? "ON" : "OFF"));

  boardctl(BIOC_GET_LEDSW, (uintptr_t) &state);
  printf(" - LED OFF Switch is %s.\n", (state == 0 ? "ON" : "OFF"));

  return state;
}

/****************************************************************************
 * Name:
 ****************************************************************************/

static inline void powerled_onoff(bool onoff)
{
  printf(" - Power LED <- %s\n", (onoff == true ? "ON" : "OFF"));

  boardctl(BIOC_SET_LED, (uintptr_t) onoff);
}

/****************************************************************************
 * Name:
 ****************************************************************************/

static void test_power_leds(void)
{
  powerled_onoff(true);
  sleep(1);

  powerled_onoff(false);
  sleep(1);
}

/****************************************************************************
 * Name:
 ****************************************************************************/

static inline int status_led_onoff(uint8_t led, int wait)
{
  int fd, res = -1;
  struct tca6507_onoff_s set;

  fd = open("/dev/leddrv0", O_RDONLY);

  if (fd > 0)
    {
      set.led = led;
      set.on = 1;

      printf(" - Status LED <- ON\n");
      ioctl(fd, LEDIOC_ONOFF, (intptr_t) &set);

      if (wait > 0)
        {
          sleep(wait);
        }

      set.on = 0;
      printf(" - Status LED <- OFF\n");
      ioctl(fd, LEDIOC_ONOFF, (intptr_t) &set);

      close(fd);

      res = 0;
    }
  else
    {
      printf(" ! Failed to open LED driver.\n");
    }

  return res;
}

/****************************************************************************
 * Name:
 ****************************************************************************/

static void test_status_leds(void)
{
  int i;

  uint8_t leds[] = {
    LED_STATUS1_RED,
    LED_STATUS2_RED,
    LED_STATUS3_RED,
    LED_STATUS1_GREEN,
    LED_STATUS2_GREEN,
    LED_STATUS3_GREEN
  };

  for (i = 0; i < ARRAY_SIZE(leds); i++)
    {
      if (status_led_onoff(leds[i], 1) < 0)
        {
          break;
        }
    }
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
int test_leds_main(int argc, char *argv[])
#endif
{
  int state;

  printf("* XG-50 LED test\n");

  state = test_led_switch();

  if (state != 0)
    {
      test_power_leds();
      test_status_leds();
    }

  return 0;
}
