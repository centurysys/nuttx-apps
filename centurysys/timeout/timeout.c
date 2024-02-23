/****************************************************************************
 * apps/centurysys/timeout/timeout.c
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
#include <getopt.h>
#include <unistd.h>
#include <sys/boardctl.h>

#include "libmount.h"
#include "power.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

struct parameter
{
  time_t timeout;
  bool verbose;
  bool fake;
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static void usage(char *name)
{
  fprintf(stderr, "Usage: %s [OPTIONS]\n", name);
  fprintf(stderr, "\t-t|--timeout <seconds>: timeout time\n");
  fprintf(stderr, "\t-v|--verbose: verbose\n");
  fprintf(stderr, "\t-f|--fake: do not reboot (fake)\n");
  fprintf(stderr, "\t-h|--help: show this message\n");

  exit(EXIT_FAILURE);
}

static int parse_args(int argc, char **argv, struct parameter *param)
{
  int ret;
  struct option options[] =
    {
      {"timeout", 1, NULL, 't'},
      {"verbose", 0, NULL, 'v' },
      {"fake", 0, NULL, 'f' },
      {"help", 0, NULL, 'h' },
    };

  memset(param, 0, sizeof(struct parameter));

  while ((ret = getopt_long(argc, argv, "t:fvh", options, NULL))
         != ERROR)
    {
      switch (ret)
        {
          case 't':
            param->timeout = atoi(optarg);
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
 * hello_main
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

  if (param.timeout <= 0)
    {
      usage(argv[0]);
    }

  printf("timeout: %lld\n", param.timeout);

  sleep(param.timeout);

  if (param.verbose)
    {
      fprintf(stderr, "timeouted, reboot!\n");
    }

  if (!param.fake)
    {
      umount_fs("/home");
      boardctl(BOARDIOC_RESET, 0);
    }

  return ret;
}
