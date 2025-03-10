/****************************************************************************
 * apps/centurysys/libs/schedule/lib_schedule.c
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.  The
 * ASF licenses this file to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <stdio.h>
#include <fcntl.h>
#include <debug.h>
#include <sys/ioctl.h>

#include <nuttx/timers/rtc.h>
#include <nuttx/timers/dsk324sr.h>
#include <nuttx/board.h>

#include "schedule.h"

#define MAX_TIME_STRING 80

#ifndef ARRAY_SIZE
#  define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#endif

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static time_t calc_next_schedule(struct tm *current, time_t interval)
{
  time_t offset, time_utc, time_local, time_sched;

  time_utc = timegm(current);
  offset = time_utc - mktime(current);

  time_local = time_utc + offset;
  time_sched = ((time_local + interval) / interval) * interval - offset;

  return time_sched;
}

static int i2c_bus_reset(int bus)
{
  char path[16];
  char *result_txt;
  char msg[128];
  int fd, ret;

  sprintf(path, "/dev/i2c%d", bus);
  fd = open(path, O_RDONLY);

  if (fd < 0)
    {
      _err("could not open i2c device.");
      return ERROR;
    }

  ret = ioctl(fd, I2CIOC_RESET, 0);

  result_txt = ret >= 0 ? "succeed" : "fail";
  sprintf(msg, "reset I2C-bus %sed\n", result_txt);
  printf(msg);
  _info("%s", msg);

  close(fd);

  return ret;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int get_next_schedule(time_t interval, time_t minimum_diff,
                      struct schedule_s *sched)
{
  int fd, ret, retry = 3;;
  struct rtc_time rtctime;
  time_t now, next, diff;
  bool skipped = false;

  if (!sched)
    {
      return -EFAULT;
    }

retry:
  fd = open("/dev/rtc0", O_RDONLY);
  if (fd < 0)
    {
      printf("open RTC failed!, %s\n", strerror(errno));
      return -ENODEV;
    }

  ret = ioctl(fd, RTC_RD_TIME, &rtctime);

  if (ret < 0)
    {
      printf("failed to get RTC time (retry: %d).\n", retry);
      close(fd);

      usleep(10 * 1000);
      i2c_bus_reset(1);

      if (--retry > 0)
        {
          usleep(10 * 1000);
          goto retry;
        }

      return ret;
    }

  now = timegm((struct tm *)&rtctime);
  next = calc_next_schedule((struct tm *)&rtctime, interval);
  diff = next - now;

  if (diff < minimum_diff)
    {
      next += interval;
      skipped = true;
    }

  gmtime_r(&next, &sched->sched_time);
  localtime_r(&next, &sched->tm_local);
  sched->skipped = skipped;
  sched->wait_sec = diff;

  close(fd);

  return ret;
}
