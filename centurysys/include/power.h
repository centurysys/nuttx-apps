/****************************************************************************
 * apps/centurysys/include/power.h
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

#ifndef __APPS_CENTURYSYS_LIB_POWER_H
#define __APPS_CENTURYSYS_LIB_POWER_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#ifdef BIT
#undef BIT
#endif
#define BIT(n) (1 << n)

/****************************************************************************
 * Public Types
 ****************************************************************************/

typedef enum
{
  WKUP_RTC    = BIT(1),
  WKUP_OPTSW  = BIT(2),
  WKUP_MSP430 = BIT(3),
} wakeup_source;

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

int enable_wakeup(uint32_t sources);
int disable_wakeup(uint32_t sources);
int get_wakeup(uint32_t *sources);
void board_powerdown(void);

#endif /* __APPS_CENTURYSYS_LIB_POWER_H */
