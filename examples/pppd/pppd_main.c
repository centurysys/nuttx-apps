/****************************************************************************
 * apps/examples/pppd/pppd_main.c
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

#include "netutils/pppd.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/****************************************************************************
 * Private Data
 ****************************************************************************/

static FAR const char connect_script[] =
  "TIMEOUT 30 "
  "\"\" ATE0 "
  "OK AT+CGDCONT=1,\\\"IP\\\",\\\"soracom.io\\\" "
  "OK AT$QCPDPP=1,1,\\\"sora\\\",\\\"sora\\\" "
  "OK ATD*99***1# "
  "CONNECT \\c";

static FAR const char disconnect_script[] =
  "TIMEOUT 5 "
  "\"\" ATH "
  "OK ATE1 "
  "OK \\c";

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: pppd_main
 ****************************************************************************/

int main(int argc, char *argv[])
{
  const struct pppd_settings_s pppd_settings =
  {
    .disconnect_script = disconnect_script,
    .connect_script = connect_script,
    .ttyname = "/dev/ttyACM2",
#ifdef CONFIG_NETUTILS_PPPD_PAP
    .pap_username = "sora",
    .pap_password = "sora",
#endif
    .holdoff = 5,
    .persist = false,
  };

  return pppd(&pppd_settings);
}
