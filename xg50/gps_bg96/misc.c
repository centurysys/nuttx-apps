/****************************************************************************
 * apps/xg50/lib/misc.c
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
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <syslog.h>
#include <errno.h>
#include <nuttx/i2c/i2c_master.h>
#include <nuttx/timers/rtc.h>

#include "misc.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#ifndef CONFIG_I2CTOOL_DEFFREQ
#define CONFIG_I2CTOOL_DEFFREQ 400000
#endif

#ifndef ARRAY_SIZE
#  define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#endif

struct reg_value {
  uint8_t reg;
  uint8_t value;
};

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

/****************************************************************************
 * Private Data
 ****************************************************************************/

static int leds_initialized;

static struct reg_value init_TCA6507[] = {
  { 0x00, 0x00 },
  { 0x01, 0x00 },
  { 0x02, 0x00 },
};

static struct reg_value init_PCA9534[] = {
  { 0x01, 0x0f },
  { 0x03, 0xf0 },
};

/****************************************************************************
 * External Functions
 ****************************************************************************/

extern int stm32l4_pmstop(bool lpds);
extern void stm32l4_clockenable(void);
extern int up_rtc_getdatetime_with_subseconds(FAR struct tm *tp, FAR long *nsec);

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name:
 ****************************************************************************/

static int _find_i2c_device(int bus, uint8_t addr)
{
  int fd, res;
  struct i2c_transfer_s xfer;
  struct i2c_msg_s msg[2];
  char devname[11];
  uint8_t regaddr, buf;

  if (bus < 0 || bus > 2)
    {
      return -1;
    }

  sprintf(devname, "/dev/i2c%d", bus);

  if ((fd = open(devname, O_RDWR)) < 0)
    {
      syslog(LOG_ERR, "! %s: open(%s) failed with %d.\n",
             __FUNCTION__, devname, errno);
      return -1;
    }

  /* setup i2c msg */

  regaddr = 0;

  msg[0].frequency = CONFIG_I2CTOOL_DEFFREQ;
  msg[0].addr      = addr;
  msg[0].flags     = 0;
  msg[0].buffer    = &regaddr;
  msg[0].length    = 1;

  msg[1].frequency = CONFIG_I2CTOOL_DEFFREQ;
  msg[1].addr      = addr;
  msg[1].flags     = I2C_M_READ;
  msg[1].buffer    = &buf;
  msg[1].length    = 1;

  /* Set up the IOCTL argument */

  xfer.msgv = msg;
  xfer.msgc = 2;

  /* Perform the IOCTL */

  res = ioctl(fd, I2CIOC_TRANSFER, (unsigned long) ((uintptr_t) &xfer));

  close(fd);

  return res;
}

/****************************************************************************
 * Name:
 ****************************************************************************/

static int i2c_reg_write(int bus, uint8_t dev_addr, uint8_t reg_addr,
                         uint8_t value)
{
  int fd, res;
  struct i2c_transfer_s xfer;
  struct i2c_msg_s msg[2];
  char devname[11];

  sprintf(devname, "/dev/i2c%d", bus);

  if ((fd = open(devname, O_RDWR)) < 0)
    {
      syslog(LOG_ERR, "! %s: open(%s) failed with %d.\n",
             __FUNCTION__, devname, errno);
      return -1;
    }

  /* setup i2c msg */

  msg[0].frequency = CONFIG_I2CTOOL_DEFFREQ;
  msg[0].addr      = dev_addr;
  msg[0].flags     = 0;
  msg[0].buffer    = &reg_addr;
  msg[0].length    = 1;

  msg[1].frequency = CONFIG_I2CTOOL_DEFFREQ;
  msg[1].addr      = dev_addr;
  msg[1].flags     = I2C_M_NOSTART;
  msg[1].buffer    = &value;
  msg[1].length    = 1;

  /* Set up the IOCTL argument */

  xfer.msgv = msg;
  xfer.msgc = 2;

  /* Perform the IOCTL */

  res = ioctl(fd, I2CIOC_TRANSFER, (unsigned long) ((uintptr_t) &xfer));

  close(fd);

  return res;
}

/****************************************************************************
 * Name:
 ****************************************************************************/

static int set_led_tca6507(uint8_t value)
{
  int res;

  res = i2c_reg_write(1, I2C_ADDR_TCA6507, 2, value);

  return res;
}

/****************************************************************************
 * Name:
 ****************************************************************************/

static int set_led_pca9534(uint8_t value)
{
  int res;

  res = i2c_reg_write(1, I2C_ADDR_PCA9534, 1, value);

  return res;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name:
 ****************************************************************************/

int find_i2c_device(uint8_t addr)
{
  return _find_i2c_device(1, addr);
}

/****************************************************************************
 * Name:
 ****************************************************************************/

void init_leds(void)
{
  int i;

  if (leds_initialized != 0)
    {
      return;
    }

  /* initialize TCA6507 */

  for (i = 0; i < ARRAY_SIZE(init_TCA6507); i++)
    {
      i2c_reg_write(1, I2C_ADDR_TCA6507, init_TCA6507[i].reg, init_TCA6507[i].value);
    }

  /* initialize PCA9534 */

  for (i = 0; i < ARRAY_SIZE(init_PCA9534); i++)
    {
      i2c_reg_write(1, I2C_ADDR_PCA9534, init_PCA9534[i].reg, init_PCA9534[i].value);
    }

  leds_initialized = 1;
}

/****************************************************************************
 * Name:
 ****************************************************************************/

int set_signal_level(int level)
{
  uint8_t val_tca6507, val_pca9534;

  if (level > 5)
    {
      return -1;
    }

  init_leds();

  switch (level)
    {
    case 5:
      val_pca9534 = 0x03;
      val_tca6507 = 0x38;
      break;

    case 4:
      val_pca9534 = 0x0b;
      val_tca6507 = 0x38;
      break;

    case 3:
      val_pca9534 = 0x0f;
      val_tca6507 = 0x38;
      break;

    case 2:
      val_pca9534 = 0x0f;
      val_tca6507 = 0x18;
      break;

    case 1:
      val_pca9534 = 0x0f;
      val_tca6507 = 0x08;
      break;

    default:
      val_pca9534 = 0x0f;
      val_tca6507 = 0x01;
      break;
    }

  set_led_pca9534(val_pca9534);
  set_led_tca6507(val_tca6507);

  return -1;
}

/****************************************************************************
 * Name:
 ****************************************************************************/

int set_signal_level_by_LQI(uint8_t LQI)
{
  int level;

  if (LQI >= 125)
    {
      level = 5;
    }
  else if (LQI >= 88)
    {
      level = 4;
    }
  else if (LQI >= 52)
    {
      level = 3;
    }
  else if (LQI >= 34)
    {
      level = 2;
    }
  else if (LQI >= 8)
    {
      level = 1;
    }
  else
    {
      level = -1;
    }

  return set_signal_level(level);
}

/****************************************************************************
 * Name:
 ****************************************************************************/

#if 0
uint16_t rtc_sleep(char *task_name, uint16_t seconds)
{
  int fd;
  struct rtc_setrelative_s setrel;
  struct tm tm;
  struct timeval tv;
  time_t now;
  long nsec;

  if ((fd = open("/dev/rtc0", O_RDWR)) < 0)
    {
      syslog(LOG_ERR, "! %s::%s: unable to open RTC driver.\n",
             task_name ? task_name : "UNKNOWN", __FUNCTION__);
      return 0;
    }

  setrel.id      = 0;
  setrel.signo   = 1;
  setrel.pid     = 0;
  setrel.reltime = (time_t) seconds;

  if (ioctl(fd, RTC_SET_RELATIVE, (unsigned long) ((uintptr_t) &setrel)) == 0)
    {
      irqstate_t flags;

      flags = enter_critical_section();

      stm32l4_pmstop(true);

      leave_critical_section(flags);
      stm32l4_clockenable();
      clock_synchronize();
    }

  close(fd);

  up_rtc_getdatetime_with_subseconds(&tm, &nsec);
  now = mktime(&tm);

  tv.tv_sec = now;
  tv.tv_usec = nsec / 1000;

  settimeofday(&tv, NULL);

  syslog(LOG_INFO, "** [[%s]] exit deepsleep (%d sec) (nsec: %ld)...\n",
         task_name ? task_name : "UNKNOWN", seconds, nsec);

  return seconds;
}
#endif

/****************************************************************************
 * Name:
 ****************************************************************************/

char *isoformat(const time_t *timer, char *buf)
{
  struct tm tm;

  localtime_r(timer, &tm);

  sprintf(buf, "%04d/%02d/%02d %02d:%02d:%02d",
          tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
          tm.tm_hour, tm.tm_min, tm.tm_sec);

  return buf;
}
