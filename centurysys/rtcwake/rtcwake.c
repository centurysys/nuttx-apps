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
#include <getopt.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include <nuttx/timers/rtc.h>
#include <nuttx/timers/dsk324sr.h>
#include <nuttx/board.h>

#include "power.h"
#include "schedule.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define MAX_TIME_STRING 80

#ifndef ARRAY_SIZE
#  define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#endif

struct parameter
{
  time_t interval;
  time_t minimum;
  bool verbose;
  bool fake;
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static const char *bool2str(int val)
{
  return (val > 0 ? "Yes" : "No");
}

static int rtcwake(struct parameter *param)
{
  int ret;
  struct schedule_s schedule;
  const char *time_fmt = "%a, %b %d %H:%M:%S %Y";

  ret = get_next_schedule(param->interval, param->minimum, &schedule);

  if (ret < 0)
    {
      return ret;
    }

  if (param->verbose || param->fake)
    {
      char timbuf[MAX_TIME_STRING];

      printf("--- Schedule ---\n");
      printf("  Interval: %lld [sec], Minimum: %lld [sec]\n",
             param->interval, param->minimum);
      strftime(timbuf, MAX_TIME_STRING, time_fmt,
               &schedule.tm_local);
      printf("  Next Schedule: %s\n", timbuf);
      strftime(timbuf, MAX_TIME_STRING, time_fmt,
               &schedule.sched_time);
      printf("  RTC Alarm:     %s\n", timbuf);
      printf("  Skip Schedule: %s\n", bool2str(schedule.skipped));
      printf("  Wait:          %d [sec]\n", schedule.wait_sec);
      fflush(stdout);

      sleep(1);

      if (param->fake)
        {
          return OK;
        }
    }

  if (schedule.skipped)
    {
      fprintf(stderr, "Wait %d [sec] -> Schedule Skipped.\n", schedule.wait_sec);
      return OK;
    }

  ret = enable_wakeup(WKUP_RTC | WKUP_OPTSW);

  if (ret < 0)
    {
      fprintf(stderr, "enable_wakeup(RTC|OPTSW) failed.\n");
      return ret;
    }

  ret = set_rtc_alarm((struct rtc_time *)&schedule.sched_time);

  if (ret < 0)
    {
      fprintf(stderr, "set_rtc_alarm failed.\n");
    }
  else
    {
      board_powerdown();
      /* not reached here */
    }

  return ret;
}

static void usage(char *name)
{
  fprintf(stderr, "Usage: %s [OPTIONS]\n", name);
  fprintf(stderr, "\t-i|--interval <seconds>: schedule interval\n");
  fprintf(stderr, "\t-m|--minimum  <seconds>: minimum wait\n");
  fprintf(stderr, "\t-v|--verbose: verbose\n");
  fprintf(stderr, "\t-f|--fake: do not shutdown (fake)\n");
  fprintf(stderr, "\t-h|--help: show this message\n");

  exit(EXIT_FAILURE);
}

static int parse_args(int argc, char **argv, struct parameter *param)
{
  int ret;
  struct option options[] =
    {
      {"interval", 1, NULL, 'i'},
      {"minmum", 1, NULL, 'm' },
      {"verbose", 0, NULL, 'v' },
      {"fake", 0, NULL, 'f' },
      {"help", 0, NULL, 'h' },
    };

  memset(param, 0, sizeof(struct parameter));

  while ((ret = getopt_long(argc, argv, "i:m:fvh", options, NULL))
         != ERROR)
    {
      switch (ret)
        {
          case 'i':
            param->interval = atoi(optarg);
            break;

          case 'm':
            param->minimum = atoi(optarg);
            break;

          case 'v':
            param->verbose = true;
            break;

          case 'f':
            param->fake = true;
            break;

          case 'h':
          case '?':
          default:
            usage(argv[0]);
            break;
        }
    }

  return 0;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * rtcwake main
 ****************************************************************************/

int main(int argc, FAR char *argv[])
{
  int ret;
  struct parameter param;

  ret = parse_args(argc, argv, &param);
  if (ret < 0)
    {
      return ret;
    }

  if (param.interval <= 0)
    {
      usage(argv[0]);
    }

  printf(" interval: %lld, minimum: %lld\n", param.interval, param.minimum);

  return rtcwake(&param);
}
