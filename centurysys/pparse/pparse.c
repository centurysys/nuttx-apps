/****************************************************************************
 * apps/centurysys/pparse/pparse.c
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

#include <nuttx/fs/fs.h>
#include <nuttx/fs/partition.h>

#if defined(CONFIG_MBR_PARTITION) || defined(CONFIG_GPT_PARTITION)
static void partition_handler(struct partition_s *part, void *arg)
{
  const char *parent = (const char *) arg;
  char path[NAME_MAX + 1];

  sprintf(path, "%sp%d", parent, part->index + 1);
  register_blockpartition(path, 0666, parent, part->firstblock, part->nblocks);
}
#endif

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * hello_main
 ****************************************************************************/

int main(int argc, FAR char *argv[])
{
  const char path[] = "/dev/mmcblk0";
  int ret;

  ret = parse_block_partition(&path, partition_handler, (void *) &path);
  printf("pparse -> ret: %d\n", ret);

  return ret;
}
