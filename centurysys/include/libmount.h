/****************************************************************************
 * apps/centurysys/include/libmount.h
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

#ifndef __APPS_CENTURYSYS_LIB_MOUNT_H
#define __APPS_CENTURYSYS_LIB_MOUNT_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <sys/mount.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define MOUNT_RDONLY MS_RDONLY

/****************************************************************************
 * Public Types
 ****************************************************************************/

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

int mount_fs(const char *source, const char *target, const char *fstype,
             unsigned long flags);
int umount_fs(const char *target);

#endif /* __APPS_CENTURYSYS_LIB_MOUNT_H */
