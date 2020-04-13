/****************************************************************************
 * configuration.c
 *
 * Originally by:
 *
 *   Copyright (C) 2019 Century Systems. All rights reserved.
 *   Author: Takeyoshi Kikuchi <kikuchi@centurysys.co.jp>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name NuttX nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <syslog.h>

#include "configuration.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#ifndef CONFIG_XG50_CONFIG_MTD
#  define CONFIG_XG50_CONFIG_MTD "/dev/mtdblock0"
#endif

#ifndef ARRAY_SIZE
#  define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#endif

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

/****************************************************************************
 * Private Data
 ****************************************************************************/

static config_t _config;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name:
 ****************************************************************************/

static void usage(void)
{
  printf("usage: setup [-i ID] [-a APN] [-u user] [-p password] [-H server_host] [-P server_port]"
         " [-s] [-l] [-S] [-h]\n"
         "   -i ID\n"
         "   -a APN\n"
         "   -u user\n"
         "   -p password\n"
         "   -H server_host : server hostname\n"
         "   -P server_port : server port\n"
         "   -s : save configuration\n"
         "   -l : (re)load configuration from EEPROM\n"
         "   -S : show configuration\n"
         "   -h : show this message\n");
}

/****************************************************************************
 * Name:
 ****************************************************************************/

static void show_config(config_t *config)
{
  if (config->valid)
    {
      sched_lock();

      printf("=== BG96 Configuration ===\n"
             " - APN:      %s\n"
             " - user:     %s\n"
             " - password: %s\n"
             " - Server:   %s\n"
             " - Port:     %d\n"
             " - ID:       %d\n",
             config->apn, config->user, config->password,
             config->host, config->port, config->id);

        sched_unlock();
    }
  else
    {
      printf("!!! configuration not valid!\n");
    }
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name:
 ****************************************************************************/

int save_config(config_t *config)
{
  int mtd, len, res = 0;

  if ((mtd = open(CONFIG_XG50_CONFIG_MTD, O_RDWR)) < 0)
    {
      syslog(LOG_ERR, "! %s: open(MTD) failed.\n", __FUNCTION__);
      res = -1;
      goto ret;
    }

  config->valid = 1;

  len = write(mtd, (char *) config, sizeof(config_t));

  if (len < sizeof(config_t))
    {
      syslog(LOG_ERR, "! %s: write(MTD) failed.\n", __FUNCTION__);
      res = -1;
    }
  else
    {
      syslog(LOG_ERR, "! %s: config saved to mtd.\n", __FUNCTION__);
    }

  close(mtd);

ret:
  return res;
}

/****************************************************************************
 * Name:
 ****************************************************************************/

int load_config(config_t *config)
{
  int mtd, len, res = 0;

  if ((mtd = open(CONFIG_XG50_CONFIG_MTD, O_RDONLY)) < 0)
    {
      syslog(LOG_ERR, "! %s: open(MTD) failed.\n", __FUNCTION__);
      res = -1;
      goto ret1;
    }

  len = read(mtd, (char *) config, sizeof(config_t));

  if (len < sizeof(config_t))
    {
      syslog(LOG_ERR, "! %s: read(MTD) failed.\n", __FUNCTION__);
      res = -1;
      goto ret2;
    }

  if (config->valid != 1)
    {
      syslog(LOG_INFO, "%s: config area is not valid.\n", __FUNCTION__);
      res = -1;
    }

ret2:
  if (res != 0)
    {
      memset(config, 0, sizeof(config_t));
    }

  close(mtd);

ret1:
  return res;
}

/****************************************************************************
 * Name:
 ****************************************************************************/

int configuration(int argc, char **argv)
{
  int opt, len, err = 0;
  uint32_t tmp;

  while ((opt = getopt(argc, argv, "i:a:u:p:H:P:sSlh")) != ERROR)
    {
      if (err == 1)
        {
          continue;
        }

      switch (opt)
        {
        case 'a': /* APN */
          len = strlen(optarg);

          if (len > 0 && len < LEN_APN)
            {
              memset(_config.apn, 0, LEN_APN);
              strncpy(_config.apn, optarg, len);
            }
          else
            {
              usage();
              err = 1;
            }
          break;

        case 'u': /* user */
          len = strlen(optarg);

          if (len > 0 && len < LEN_USER)
            {
              memset(_config.user, 0, LEN_USER);
              strncpy(_config.user, optarg, len);
            }
          else
            {
              usage();
              err = 1;
            }
          break;

        case 'p': /* password */
          len = strlen(optarg);

          if (len > 0 && len < LEN_PASSWORD)
            {
              memset(_config.password, 0, LEN_PASSWORD);
              strncpy(_config.password, optarg, len);
            }
          else
            {
              usage();
              err = 1;
            }
          break;

        case 'H': /* hostname */
          len = strlen(optarg);

          if (len > 0 && len < LEN_HOST)
            {
              memset(_config.host, 0, LEN_HOST);
              strncpy(_config.host, optarg, len);
            }
          else
            {
              usage();
              err = 1;
            }
          break;

        case 'P': /* port */
          tmp = strtoul(optarg, NULL, 0);

          if (tmp < 65536)
            {
              _config.port = tmp;
            }
          else
            {
              usage();
              err = 1;
            }
          break;

        case 'i': /* ID */
          tmp = strtoul(optarg, NULL, 0);

          if (tmp < 0xffff)
            {
              _config.id = (uint16_t) tmp;
            }
          else
            {
              usage();
              err = 1;
            }
          break;

        case 's': /* save configuration */
          save_config(&_config);
          printf(" - configuration saved.\n");
          break;

        case 'l': /* (re)load configruration */
          load_config(&_config);
          printf(" - configuration loaded.\n");
          break;

        case 'S': /* show configuration */
          show_config(&_config);
          break;

        case 'h': /* help */
        default:
          usage();
          err = 1;
          break;

        }
    }

  return 0;
}
