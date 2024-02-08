/****************************************************************************
 * apps/centurysys/rtcwake/rtcwake.c
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

static const char *bool2str(int val)
{
  return (val > 0 ? "Yes" : "No");
}

static int test_rtc_alarm(int fd)
{
  int ret, i;
  struct rtc_time rtctime;
  struct tm tm_now;
  time_t now, next, interval;
  const time_t intervals[] = {10, 60, 720, 1440};
  char timbuf[MAX_TIME_STRING];

  ret = ioctl(fd, RTC_RD_TIME, &rtctime);

  if (ret < 0)
    {
      printf("failed to get RTC time.\n");
      return ret;
    }

  now = timegm((struct tm *)&rtctime);

  printf("timegm(now) -> tv_sec: %lld\n", now);
  localtime_r(&now, &tm_now);
  ret = strftime(timbuf, MAX_TIME_STRING, "%a, %b %d %H:%M:%S %Y",
                 &tm_now);
  if (ret >= 0)
    {
      printf("  %s\n", timbuf);
    }

  for (i = 0; i < ARRAY_SIZE(intervals); i++)
    {
      struct schedule_s schedule;
      struct tm *alarm;

      interval = intervals[i] * 60;
      get_next_schedule(interval, 60, &schedule);

      next = timegm(&schedule.sched_time);
      alarm = &schedule.sched_time;

      ret = strftime(timbuf, MAX_TIME_STRING, "%a, %b %d %H:%M:%S %Y",
                     &schedule.tm_local);
      if (ret >= 0)
        {
          printf("interval: %lld, next time_t: %lld ->  %s\n",
                 interval, next, timbuf);
          printf(" Alarm: Day: %02d, Time: %02d:%02d\n",
                 alarm->tm_mday, alarm->tm_hour, alarm->tm_min);
          if (schedule.skipped)
            {
              printf(" Alarm Skipped, sleeptime: %d [sec]\n",
                     schedule.wait_sec);
            }
        }
    }

  return OK;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * hello_main
 ****************************************************************************/

int main(int argc, FAR char *argv[])
{
  int fd;
  int ret = -ENOTTY;
  struct rtc_wkalrm alarm;
  char timbuf[MAX_TIME_STRING];

  fd = open("/dev/rtc0", O_RDONLY);
  if (fd < 0)
    {
      printf("open RTC failed!, %s\n", strerror(errno));
      return ret;
    }

  ret = ioctl(fd, RTC_ALM_READ, &alarm);

  if (ret == 0)
    {
      printf(" enabled: %s\n", bool2str(alarm.enabled));
      printf(" pending: %s\n", bool2str(alarm.pending));
      ret = strftime(timbuf, MAX_TIME_STRING, "%a, %b %d %H:%M:%S %Y",
                     (struct tm *)&alarm.time);
      if (ret >= 0)
        {
          printf(" %s\n", timbuf);
        }
    }

  ret = test_rtc_alarm(fd);

  close(fd);

  return ret;
}
