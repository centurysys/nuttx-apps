/****************************************************************************
 * apps/centurysys/lte_ctrl/lte_ctrl.c
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
#include <string.h>
#include <fcntl.h>
#include <getopt.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include <nuttx/board.h>

#include "mas1xx_lte.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static void usage(char *name)
{
  fprintf(stderr, "Usage: %s [COMMAND]\n", name);
  fprintf(stderr, "  COMMAND: on / off / reset / status\n");

  exit(EXIT_FAILURE);
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * lte_ctrl main
 ****************************************************************************/

int main(int argc, char *argv[])
{
  bool res;
  char *cmd;

  if (argc != 2)
    {
      usage(argv[0]);
      return ERROR;
    }

  cmd = argv[1];

  if (strncmp(cmd, "on", 2) == 0)
    {
      res = lte_power_ctrl(true);
    }
  else if (strncmp(cmd, "off", 3) == 0)
    {
      res = lte_power_ctrl(false);
    }
  else if (strncmp(cmd, "reset", 5) == 0)
    {
      res = lte_reset();
    }
  else if (strncmp(cmd, "status", 6) == 0)
    {
      char *stat;

      stat = get_lte_status() ? "ON" : "OFF";
      printf("LTE module Power: %s\n", stat);
      res = true;
    }
  else
    {
      usage(argv[0]);
    }

  return res ? OK : ERROR;
}
