/****************************************************************************
 * apps/centurysys/include/libppp.h
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

#ifndef __APPS_CENTURYSYS_LIB_PPP_H
#define __APPS_CENTURYSYS_LIB_PPP_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

/****************************************************************************
 * Public Types
 ****************************************************************************/

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

int launch_pppd(char *tty, char *account, char *password, bool use_pap,
                bool persist);
int get_pppd_pid(void);
int terminate_pppd(void);

#endif /* __APPS_CENTURYSYS_LIB_PPP_H */
