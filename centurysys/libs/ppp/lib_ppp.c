/****************************************************************************
 * apps/centurysys/libs/ppp/lib_ppp.c
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
#include <sched.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <debug.h>

#include <nuttx/board.h>

#include "pppd.h"

/****************************************************************************
 * Private Data
 ****************************************************************************/

static const char connect_script[] =
  "TIMEOUT 2 "
  "\"\" ATE0 "
  "OK ATD*99***1# "
  "CONNECT \\c";

static const char disconnect_script[] =
  "TIMEOUT 2 "
  "\"\" ATH "
  "\"\" ATH "
  "OK ATE1 "
  "OK \\c";

static bool running = false;
static struct pppd_settings_s settings;
static int pppd_pid = ERROR;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static int spawn_pppd(int argc, char **argv)
{
  int ret;

  if (running)
    {
      return ERROR;
    }

  running = true;

  ret = pppd(&settings);

  pppd_pid = ERROR;
  running = false;

  return ret;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int launch_pppd(char *tty, char *account, char *password, bool use_pap,
                bool persist)
{
  int pid;

  if (running)
    {
      return ERROR;
    }

  memset(&settings, 0, sizeof(struct pppd_settings_s));

  settings.disconnect_script = disconnect_script;
  settings.connect_script = connect_script;
  settings.holdoff = 5;

  strncpy(settings.ttyname, tty, strlen(tty));
#ifdef CONFIG_NETUTILS_PPPD_PAP
  if (use_pap)
    {
      strncpy(settings.pap_username, account, strlen(account));
      strncpy(settings.pap_password, password, strlen(password));
    }
#endif
  settings.persist = persist;

  pid = task_create("pppd", 100, 4096, (main_t)spawn_pppd, NULL);

  usleep(USEC_PER_TICK);

  if (kill(pid, 0) != OK)
    {
      return ERROR;
    }
  else
    {
      _info("pppd started, pid = %d\n", pid);
      pppd_pid = pid;
      return pid;
    }
}

int get_pppd_pid(void)
{
  return pppd_pid;
}

int terminate_pppd(void)
{
  int ret = ERROR;

  if (pppd_pid > 0)
    {
      ret = kill(pppd_pid, SIGTERM);
    }

  return ret;
}
