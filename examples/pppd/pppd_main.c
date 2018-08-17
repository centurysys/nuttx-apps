/****************************************************************************
 * examples/pppd/pppd_main.c
 *
 *   Copyright (C) 2015 Brennan Ashton. All rights reserved.
 *   Author: Brennan Ashton <brennan@ombitron.com>
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

#include <nuttx/config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "netutils/pppd.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/****************************************************************************
 * Private Data
 ****************************************************************************/

static FAR char connect_script_template[] =
  "ABORT BUSY "
  "ABORT \"NO CARRIER\" "
  "ABORT ERROR "
  "TIMEOUT 10 "
  "\"\" ATE0 "
  "OK AT+COPS=2 "
  "OK AT+CGDCONT=1,\\\"IP\\\",\\\"%s\\\" "
  "OK AT+UAUTHREQ=1,1,\\\"%s\\\",\\\"%s\\\" "
  "OK AT+COPS? "
  "OK ATD*99***1# "
  "CONNECT \\c";

static FAR char disconnect_script[] =
  "\"\" ATE0 "
  "OK AT+COPS? "
  "OK AT+CGACT? "
  "OK \\c";

/****************************************************************************
 * Static Functions
 ****************************************************************************/

/****************************************************************************
 * Name: call_pppd
 ****************************************************************************/

static int call_pppd(char *apn, char *user, char *passwd)
{
  char *connect_script;
  int len, res;
  struct pppd_settings_s pppd_settings =
  {
    .disconnect_script = disconnect_script,
    .ttyname = "/dev/ttyS3",
    .persist = 0,
  };


  len = strlen(connect_script_template) - 3; /* '%' * 3 */
  len += strlen(apn) + strlen(user) + strlen(passwd) + 1;

  if (!(connect_script = malloc(len)))
    {
      printf("Out of memory.\n");
      return -ENOMEM;
    }

  sprintf(connect_script, connect_script_template,
          apn, user, passwd);

  pppd_settings.connect_script = connect_script;
  strncpy(pppd_settings.pap_username, user, strlen(user));
  strncpy(pppd_settings.pap_password, passwd, strlen(passwd));

  res = pppd(&pppd_settings);

  free(connect_script);

  return res;
}

/****************************************************************************
 * Name: usage
 ****************************************************************************/
static void usage(void)
{
  printf("Usage: pppd -a APN -u UserName -p Password\n");
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: pppd_main
 ****************************************************************************/

int pppd_main(int argc, char **argv)
{
  int option, opts = 0, len;
  char *apn = NULL, *user = NULL, *passwd = NULL;

  while ((option = getopt(argc, argv, "a:u:p:")) != ERROR)
    {
      switch (option)
        {
        case 'a':
          /* APN */
          apn = optarg;
          opts++;
          break;

        case 'u':
          /* user */
          user = optarg;
          opts++;
          break;

        case 'p':
          /* password */
          passwd = optarg;
          opts++;
          break;

        default:
          usage();
          return -1;
        }
    }

  if (opts != 3)
    {
      usage();
      return -1;
    }

  return call_pppd(apn, user, passwd);
}
