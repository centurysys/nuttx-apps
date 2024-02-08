/****************************************************************************
 * apps/centurysys/libs/power/lib_power.c
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

#include <nuttx/board.h>

#include "boardctl.h"
#include "power.h"

#ifndef ARRAY_SIZE
#  define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#endif

/****************************************************************************
 * Private Data
 ****************************************************************************/

struct wkup_sources
{
  uint32_t api;
  uint32_t hw;
};

static struct wkup_sources srcs[] =
{
  { .api = WKUP_RTC, .hw = WKUPEN_ALARM },
  { .api = WKUP_OPTSW, .hw = WKUPEN_OPTSW },
  { .api = WKUP_MSP430, .hw = WKUPEN_MSP430 },
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static uint32_t api2hw(uint32_t sources_api)
{
  uint32_t val = 0;
  int i;

  for (i = 0; i < ARRAY_SIZE(srcs); i++)
    {
      if ((sources_api & srcs[i].api) != 0)
        {
          val |= srcs[i].hw;
        }
    }

  return val;
}

static uint32_t hw2api(uint32_t sources_hw)
{
  uint32_t val = 0;
  int i;

  for (i = 0; i < ARRAY_SIZE(srcs); i++)
    {
      if ((sources_hw & srcs[i].hw) != 0)
        {
          val |= srcs[i].api;
        }
    }

  return val;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int enable_wakeup(uint32_t sources)
{
  uint32_t val = api2hw(sources);

  if (val == 0)
    {
      return -EINVAL;
    }

  return boardctl(BIOC_ENABLE_WAKEUP, (uintptr_t)val);
}

int disable_wakeup(uint32_t sources)
{
  uint32_t val = api2hw(sources);

  if (val == 0)
    {
      return -EINVAL;
    }

  return boardctl(BIOC_DISABLE_WAKEUP, (uintptr_t)val);
}

int get_wakeup(uint32_t *sources)
{
  int ret;
  uint32_t val;

  if (!sources)
    {
      return -EFAULT;
    }

  ret = boardctl(BIOC_GET_WAKEUP, (uintptr_t)&val);
  if (ret < 0)
    {
      return ret;
    }

  *sources = hw2api(val);

  return 0;
}

void board_powerdown(void)
{
  boardctl(BIOC_SHUTDOWN, (uintptr_t)0);
}

