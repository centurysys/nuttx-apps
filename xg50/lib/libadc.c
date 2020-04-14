/****************************************************************************
 * apps/xg50/lib/libadc.c
 *
 * Originally by:
 *
 *   Copyright (C) 2018 Century Systems. All rights reserved.
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
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>

#include <nuttx/analog/adc.h>
#include <nuttx/analog/ioctl.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define ADC_PATH "/dev/adc0"

#define ADDR_TS_CAL1		0x1fff75a8
#define ADDR_TS_CAL2		0x1fff75ca
#define ADDR_VREFINT_CAL	0x1fff75aa

#define TS_CAL1		(*(uint16_t *) ADDR_TS_CAL1)
#define TS_CAL2		(*(uint16_t *) ADDR_TS_CAL2)
#define VREFINT_CAL	(*(uint16_t *) ADDR_VREFINT_CAL)

#define FULL_SCALE ((1 << 12) - 1)

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

/****************************************************************************
 * Private Data
 ****************************************************************************/

/****************************************************************************
 * External Functions
 ****************************************************************************/

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name:
 ****************************************************************************/

static inline uint32_t actual_value(uint16_t vref_data, uint16_t val)
{
  uint64_t denominator, numerator;
  uint32_t actual;

  numerator = (uint64_t) (3 * 1000 * 1000) * VREFINT_CAL * val;
  denominator = (uint64_t) vref_data * FULL_SCALE;

  actual = (uint32_t) (numerator / denominator);

  return actual;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name:
 ****************************************************************************/

int adc_get(uint32_t *values, int count)
{
  int fd, res, readsize, readlen;
  struct adc_msg_s sample[count + 1];

  if (count < 1 || count > 2)
    {
      syslog(LOG_ERR, "! %s: count must be between 1 and 2.\n",
             __FUNCTION__);
      return -1;
    }

  if ((fd = open(ADC_PATH, O_RDONLY)) < 0)
    {
      syslog(LOG_ERR, "! %s: open(%s) failed, %d.\n",
             __FUNCTION__, ADC_PATH, errno);
      return -1;
    }

  res = ioctl(fd, ANIOC_TRIGGER, 0);

  if (res < 0)
    {
      syslog(LOG_ERR, "! %s: ADC Trigger failed, %d.\n",
             __FUNCTION__, res);
      goto ret;
    }

  readsize = sizeof(struct adc_msg_s) * (count + 1);
  readlen = read(fd, sample, readsize);

  if (readlen < 0)
    {
      syslog(LOG_ERR, "! %s: ADC failed, %d.\n",
             __FUNCTION__, errno);
      res = -1;
      goto ret;
    }
  else if (readlen == 0)
    {
      syslog(LOG_WARNING, "%s: no data converted.\n",
             __FUNCTION__);
      res = 0;
    }
  else
    {
      int nsamples;

      nsamples = readlen / sizeof(struct adc_msg_s);

      if ((nsamples * sizeof(struct adc_msg_s)) != readlen)
        {
          syslog(LOG_ERR, "! %s: read size is not a multiple of sample size.\n",
                 __FUNCTION__);
          res = 0;
        }
      else
        {
          /* ADC OK. */
          int i;

          for (i = 1; i < count + 1; i++)
            {
              values[i - 1] = actual_value(sample[0].am_data,
                                           sample[i].am_data);
            }

          res = count;
        }
    }

ret:
  close(fd);
  return res;
}
