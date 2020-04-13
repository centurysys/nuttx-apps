/****************************************************************************
 * examples/bg96_task/bg96_task.c
 *
 *   Copyright (C) 2008, 2011-2012 Gregory Nutt. All rights reserved.
 *   Author: Gregory Nutt <gnutt@nuttx.org>
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
#include <termios.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <syslog.h>
#include <errno.h>
#include <sys/time.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <arch/board/boardctl.h>
#include <nuttx/crypto/blake2s.h>
#include <nuttx/ioexpander/gpio.h>
#include <nuttx/leds/tca6507.h>

#include "gpsutils/minmea.h"
#include "libserial.h"
#include "configuration.h"
#include "misc.h"

#define BUFSIZE 1024
static char buffer[BUFSIZE];

#ifndef max
# define max(a,b) (((a) > (b)) ? (a) : (b))
#endif

typedef enum urc {
  URC_CLOSED = 1,
  URC_RECEIVED,
  URC_INCOING_FULL,
  URC_CONNECTED,
  URC_DEACTIVATED,
} urc_t;

struct urc_closed {
  int connect_id;
};

struct urc_received {
  int connect_id;
};

struct urc_deactivated {
  int context_id;
};

struct urc_info {
  urc_t code;
  union {
    struct urc_closed closed;
    struct urc_received received;
    struct urc_deactivated deactivated;
  } info;
};

typedef enum task_state {
  STAT_SETUP = 1,
  STAT_IDLE,
  STAT_MEASURE,
  STAT_CONNECT,
  STAT_UPLOAD,
  STAT_WAIT_RESPONSE,
  STAT_RECEIVE_RESPONSE,
  STAT_DISCONNECT,
} task_state_t;

typedef struct xg50_stat {
  char iccid[20];
  uint16_t id;
  uint16_t valid;

  uint16_t battery;
  uint32_t sensor[3]; /* 0: Pressure, 1: Temperatur, 2: Humidity */

  struct {
    uint16_t lac;
    uint32_t ci;
  } cell_info;

  struct tm tm;

  int lat;
  int lon;
} xg50_stat_t;

#define NSOCKS 12

typedef struct bg96_state_t {
  int sockets[NSOCKS];
} bg96_state_t;

typedef struct {
  serial_t *ser;
  serial_t *ser_gps;

  config_t config;

  struct xg50_stat info;

  int led_found;
  int sock_idx;
  uint16_t interval;

  task_state_t state;
} task_t;

struct payload_data {
  uint16_t id;
  uint16_t valid;
  uint32_t temperature;
  uint32_t humidity;
  uint16_t voltage;
  uint16_t lac;
  uint32_t ci;
} __attribute__((packed));

struct calc_hash {
  struct payload_data data;
  char iccid[19];
} __attribute__((packed));

struct payload {
  struct payload_data data;
  char hash[8];
} __attribute__((packed));


/****************************************************************************
 * Private Data
 ****************************************************************************/

static bg96_state_t _bg96_state;
static int fd_led;

/****************************************************************************
 * External Functions
 ****************************************************************************/

extern int up_rtc_getdatetime_with_subseconds(FAR struct tm *tp, FAR long *nsec);
extern unsigned char *base64_encode(const unsigned char *src, size_t len,
                                    unsigned char *dst, size_t *out_len);

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static int chop(char *buf)
{
  char *ptr;
  int count = 0;

  if (strlen(buf) <= 1)
    return 0;

  for (ptr = buf + strlen(buf) - 1; *ptr == '\r' || *ptr == '\n'; ptr--)
    {
      *ptr = '\0';
      count++;
    }

  return count;
}

/****************************************************************************
 * LED ON/OFF
 ****************************************************************************/

static void led_onoff(int idx, int onoff)
{
  struct tca6507_onoff_s set;

  switch (idx)
    {
    case 0:
      set.led = LED_STATUS1_RED;
      break;

    case 1:
      set.led = LED_STATUS1_GREEN;
      break;

    case 2:
      set.led = LED_STATUS2_RED;
      break;

    case 3:
      set.led = LED_STATUS2_GREEN;
      break;

    case 4:
      set.led = LED_STATUS3_RED;
      break;

    case 5:
      set.led = LED_STATUS3_GREEN;
      break;

    default:
      return;
    }

  set.on = (onoff == 0) ? 0 : 1;

  ioctl(fd_led, LEDIOC_ONOFF, (intptr_t) &set);
}

/****************************************************************************
 * Name: [BG96] Reset BG96 module
 ****************************************************************************/

static void bg96_reset(void)
{
  boardctl(BIOC_RESET_B2B, 500);
}

/****************************************************************************
 * Name: [BG96] Send AT command
 ****************************************************************************/

int bg96_send_AT_command(serial_t *ser, const char *cmd)
{
  int res;

  //printf("** %s: write %s", __FUNCTION__, cmd);

  if (cmd) {
    if (ser_write(ser, cmd, strlen(cmd)) < 0)
      {
        printf("! %s: write failed, %s", __FUNCTION__, cmd);
        return -1;
      }
  }

  /* wait for 'OK' or 'ERROR' */
  while (1)
    {
      memset(buffer, 0, 128);
      res = ser_readline(ser, buffer, 128, '\n', 3000);

      if (res <= 0)
        {
          printf("! %s: read timeouted.\n", __FUNCTION__);
          return -1;
        }

      chop(buffer);
      //printf("* %s: received: \"%s\"\n", __FUNCTION__, buffer);

      if (strlen(buffer) <= 0)
        continue;

      if (strncmp(buffer, "OK", 2) == 0)
        {
          res = 0;
          break;
        }
      else if (strncmp(buffer, "ERROR", 5) == 0)
        {
          res = -1;
          break;
        }
    }

  return res;
}

/****************************************************************************
 * Name: [BG96] TCP/IP接続パラメータ設定
 ****************************************************************************/

static int bg96_echo_off(serial_t *ser)
{
  int res, retry = 3;

  while (retry--) {
    res = bg96_send_AT_command(ser, "ATE0\r\n");

    if (res != 0)
      {
        printf("! %s: ATE0 failed (retry left: %d).\n", __FUNCTION__, retry);
        sleep(1);
      }
    else
      {
        break;
      }
  }

  res = bg96_send_AT_command(ser, "AT+QISDE=0\r\n");

  return res;
}

/****************************************************************************
 * Name: [BG96] TCP/IP接続パラメータ設定
 ****************************************************************************/

static int bg96_setup_apn(serial_t *ser, int cid, char *apn, char *user,
                          char *password)
{
  int res, need_setup = 1;
  char buf2[64];

  memset(buffer, 0, BUFSIZE);

  sprintf(buffer, "AT+QICSGP=1\r\n");
  sprintf(buf2, "+QICSGP: %d,\"%s\",\"%s\",\"%s\",3", cid, apn, user, password);

  res = ser_write(ser, buffer, strlen(buffer));
  chop(buffer);
  //printf("* %s: write '%s' -> %d\n", __FUNCTION__, buffer, res);

  if (res < 0)
    {
      syslog(LOG_ERR, "!%s: ser_write(\"%s\") failed -> RESET\n",
             __FUNCTION__, buffer);
      sleep(2);
      bg96_reset();
      return -1;
    }

  /* wait for 'OK' */
  while (1)
    {
      res = ser_readline(ser, buffer, 128, '\n', 5000);

      if (res <= 0)
        {
          syslog(LOG_ERR, "! %s: receive timeouted.\n", __FUNCTION__);
          res = -1;
          break;
        }

      chop(buffer);

      if (strlen(buffer) <= 0)
        continue;

      //printf("* %s: received: %s\n", __FUNCTION__, buffer);

      if (strncmp(buffer, buf2, strlen(buf2)) == 0)
        {
          //printf("* %s: received: \"%s\" (%d)\n", __FUNCTION__,
          //       buffer, res);
          printf("* %s: do not need to setup APN\n", __FUNCTION__);
          need_setup = 0;
        }
      else if (strstr(buffer, "OK"))
        {
          //printf("* %s: 'OK' received.\n", __FUNCTION__);
          res = 0;
          break;
        }
    }

  if (res < 0 || need_setup == 0)
    {
      return res;
    }

  sprintf(buffer, "AT+QICSGP=%d,1,\"%s\",\"%s\",\"%s\",3\r\n", cid, apn, user, password);
  //printf("* write: '%s'\n", buffer);

  if (ser_write(ser, buffer, strlen(buffer)) < 0)
    {
      syslog(LOG_ERR, "!%s: ser_write(\"%s\") failed -> RESET\n",
             __FUNCTION__, buffer);
      sleep(2);
      bg96_reset();
      return -1;
    }

  /* wait for 'OK' */
  while (1)
    {
      res = ser_readline(ser, buffer, 128, '\n', 5000);

      if (res <= 0)
        {
          res = -1;
          break;
        }

      else if (strncmp(buffer, "OK", 2) == 0)
        {
          //printf("* %s: 'OK' received.\n", __FUNCTION__);
          res = 0;
          break;
        }
    }

  return res;
}

/****************************************************************************
 * Name: [BG96] Check Connection Status
 ****************************************************************************/

static int bg96_get_connection_status(serial_t *ser)
{
  int res;
  int connected = -1;
  const char * const res_connected = "+QIACT:";

  sprintf(buffer, "AT+QIACT?\r\n");
  //printf(" cmd: %s", buffer);

  if (ser_write(ser, buffer, strlen(buffer)) < 0)
    {
      printf("! %s: write failed.\n", __FUNCTION__);
      return -1;
    }

  /* wait for '+QIACT' */
  while (1)
    {
      res = ser_readline(ser, buffer, 128, '\n', 30000);

      if (res <= 0)
        {
          printf("! read timeouted.\n");
          break;
        }

      chop(buffer);
      //printf("* %s: received: \"%s\"\n", __FUNCTION__, buffer);

      if (strstr(buffer, res_connected))
        {
          printf("* %s: stat: connected (\"%s\")\n", __FUNCTION__, buffer);
          connected = 0;
        }
      else if (strstr(buffer, "OK"))
        {
          //printf("* %s: 'OK' received.\n", __FUNCTION__);
          break;
        }
      else if (strncmp(buffer, "ERROR", 5) == 0)
        {
          //printf("! %s: 'ERROR' received.\n", __FUNCTION__);
          break;
        }
    }

  return connected;
}

/****************************************************************************
 * Name: [BG96] Connect
 ****************************************************************************/

static int bg96_connect(serial_t *ser)
{
  int res;

  res = bg96_get_connection_status(ser);

  if (res == 0)
    {
      /* already connected. */
      return 0;
    }

  sprintf(buffer, "AT+QIACT=1\r\n");
  printf(" cmd: %s", buffer);

  if (bg96_send_AT_command(ser, buffer) < 0)
    {
      printf("! %s: write failed.\n", __FUNCTION__);
      return -1;
    }

  return bg96_get_connection_status(ser);
}

/****************************************************************************
 * Name: [BG96] Disconnect
 ****************************************************************************/

static int bg96_disconnect(serial_t *ser)
{
  int res;

  res = bg96_get_connection_status(ser);

  if (res == -1)
    {
      /* already disconnected. */
      return 0;
    }

  sprintf(buffer, "AT+QIDEACT=1\r\n");
  printf(" cmd: %s", buffer);

  if (ser_write(ser, buffer, strlen(buffer)) < 0)
    {
      printf("! %s: write failed.\n", __FUNCTION__);
      return -1;
    }

  res = bg96_get_connection_status(ser);

  return (res == -1) ? 0 : -1;
}

/****************************************************************************
 * Name: [BG96] Open TCP socket
 ****************************************************************************/

static int bg96_connect_tcp(serial_t *ser, const char *host, uint16_t dest_port)
{
  int i, res, stat, err = -1, idx = -1;
  const char *res_connected = "+QIOPEN:";

  /* search for free socket index. */
  for (i = 0; i < NSOCKS; i++)
    {
      if (_bg96_state.sockets[i] == 0)
        {
          idx = i;
          break;
        }
    }

  if (idx == -1)
    {
      printf("! %s: no free sockets.\n", __FUNCTION__);
      return -1;
    }

  sprintf(buffer, "AT+QIOPEN=1,%d,\"TCP\",\"%s\",%d,0,0\r\n", idx, host, dest_port);
  //printf(" cmd: %s", buffer);

  if (ser_write(ser, buffer, strlen(buffer)) < 0)
    {
      printf("! %s: write failed.\n", __FUNCTION__);
      return -1;
    }

  /* wait for '+QIOPEN:' */
  while (1)
    {
      res = ser_readline(ser, buffer, 128, '\n', 30000);

      if (res <= 0)
        {
          printf("! %s: read timeouted.\n", __FUNCTION__);
          break;
        }

      chop(buffer);
      //printf("* %s: received: \"%s\"\n", __FUNCTION__, buffer);

      if (strstr(buffer, res_connected))
        {
          res = sscanf(buffer, "+QIOPEN: %d,%d", &stat, &err);

          if (res != 2)
            {
              printf("! %s: parse +QIOPEN result failed (%d).\n",
                     __FUNCTION__, res);
            }
          else if (err == 0)
            {
              printf("* %s: connected.\n", __FUNCTION__);
              _bg96_state.sockets[i] = 1;
              err = idx;
            }
          else
            {
              printf("! %s: connect failed (%d).\n", __FUNCTION__, err);
              _bg96_state.sockets[i] = 0;
              err = -err;
            }

          break;
        }
    }

  return err;
}

/****************************************************************************
 * Name: [BG96] Close TCP socket
 ****************************************************************************/

static int bg96_disconnect_tcp(serial_t *ser, int idx)
{
  int res;

  if (_bg96_state.sockets[idx] == 0)
    {
      /* not opened. */
      return 0;
    }

  sprintf(buffer, "AT+QICLOSE=%d\r\n", idx);

  res = bg96_send_AT_command(ser, buffer);

  if (res < 0)
    {
      printf("! %s: close socket failed.\n", __FUNCTION__);
    }

  _bg96_state.sockets[idx] = 0;

  return 0;
}

/****************************************************************************
 * Name: [BG96] TCP send
 ****************************************************************************/

static int bg96_send_tcp(serial_t *ser, int sock, const char *wbuf, int buflen)
{
  int res, writelen = 0;

  sprintf(buffer, "AT+QISEND=%d,%d\r\n", sock, buflen);
  //printf(" cmd: %s", buffer);

  if (ser_write(ser, buffer, strlen(buffer)) < 0)
    {
      printf("! %s: write failed.\n", __FUNCTION__);
      return -1;
    }

  /* wait for '>' */
  while (1)
    {
      res = ser_read(ser, buffer, 1, 1000);

      if (res <= 0)
        {
          printf("! %s: read timeouted.\n", __FUNCTION__);
          return -1;
        }

      buffer[1] = '\0';
      //printf("* %s: received: 0x%02x\n", __FUNCTION__, buffer[0]);

      if (buffer[0] == '>')
        {
          while (writelen < buflen)
            {
              res = ser_write(ser, wbuf + writelen, buflen - writelen);

              if (res <= 0)
                {
                  printf("! %s: ser_write() failed.\n", __FUNCTION__);
                  return -1;
                }

              writelen += res;
            }

          break;
        }
    }

  /* wait for result */
  /* wait for '+QIOPEN:' */
  while (1)
    {
      res = ser_readline(ser, buffer, 128, '\n', 30000);

      if (res <= 0)
        {
          printf("! %s: read timeouted.\n", __FUNCTION__);
          return -1;
        }

      chop(buffer);
      //printf("* %s: received: \"%s\"\n", __FUNCTION__, buffer);

      if (strstr(buffer, "SEND OK"))
        {
          printf("  -> send succeeded.\n");
          break;
        }
      else if (strstr(buffer, "SEND FAIL"))
        {
          printf("  -> send failed.\n");
          return -1;
        }
      else if (strstr(buffer, "ERROR"))
        {
          printf("  -> send error.\n");
          return -1;
        }
    }

  printf("---> writelen: %d\n", writelen);
  return writelen;
}

/****************************************************************************
 * Name: [BG96] polling URC
 ****************************************************************************/

static int bg96_handle_URC(serial_t *ser, struct urc_info *urc)
{
  int res, code;
  char report[16];

  /* wait for '+QIURC:' */
  while (1)
    {
      res = ser_readline(ser, buffer, 256, '\n', 1000);

      if (res <= 0)
        {
          printf("! %s: read timeouted.\n", __FUNCTION__);
          return -1;
        }

      chop(buffer);
      printf("* %s: received: \"%s\"\n", __FUNCTION__, buffer);

      if (strstr(buffer, "+QIURC:"))
        {
          res = sscanf(buffer, "+QIURC: \"%[a-z]\",%d", &report, &code);

          if (res != 2)
            {
              printf("! %s: parse +QIURC result failed (%d), '%s'.\n",
                     __FUNCTION__, res, report);
              return -1;
            }

          break;
        }
    }

  /* classify URC */
  res = 0;

  if (strstr(report, "closed"))
    {
      urc->code = URC_CLOSED;
      urc->info.closed.connect_id = code;
    }
  else if (strstr(report, "recv"))
    {
      urc->code = URC_RECEIVED;
      urc->info.received.connect_id = code;
    }
  else if (strstr(report, "pdpdeact"))
    {
      urc->code = URC_DEACTIVATED;
      urc->info.deactivated.context_id = code;
    }
  else
    {
      /* ??? */
      printf("! %s: unhandled URC received, \"%s\"\n", __FUNCTION__, report);
      res = -1;
    }

  return res;
}

/****************************************************************************
 * Name: [BG96] TCP recv
 ****************************************************************************/

static int bg96_recv_tcp(serial_t *ser, int sock, char *rbuf, int buflen)
{
  int res, restlen, readlen;

  sprintf(buffer, "AT+QIRD=0\r\n", sock, buflen);
  printf(" cmd: %s", buffer);

  if (ser_write(ser, buffer, strlen(buffer)) < 0)
    {
      printf("! %s: write failed.\n", __FUNCTION__);
      return -1;
    }

  /* wait for '+QIRD:' */
  while (1)
    {
      res = ser_readline(ser, buffer, 128, '\n', 5000);

      if (res <= 0)
        {
          printf("! read timeouted.\n");
          return -1;
        }

      chop(buffer);
      printf("* %s: received: \"%s\"\n", __FUNCTION__, buffer);

      if (strstr(buffer, "+QIRD:"))
        {
          res = sscanf(buffer, "+QIRD: %d", &restlen);

          if (res != 1)
            {
              printf("! %s: parse +QIRD result failed (%d).\n",
                     __FUNCTION__, res);
              return -1;
            }

          readlen = ser_read(ser, rbuf, restlen, 3000);
          break;
        }
    }

  return readlen;
}

/****************************************************************************
 * Name: [BG96] CCLK (get network clock)
 ****************************************************************************/

static int bg96_sync_clock(serial_t *ser)
{
  int res, tz_offset, result = -1;
  struct tm now;
  time_t time_now;
  struct timeval tv;
  const char * const res_cclk = "+CCLK:";

  sprintf(buffer, "AT+CCLK?\r\n");

  if (ser_write(ser, buffer, strlen(buffer)) < 0)
    {
      printf("! %s: write failed.\n", __FUNCTION__);
      return -1;
    }

  /* wait for '+CCLK' response */
  while (1)
    {
      res = ser_readline(ser, buffer, 128, '\n', 3000);

      if (res <= 0)
        {
          printf("! %s: read timeouted.\n", __FUNCTION__);
          return -1;
        }

      chop(buffer);
      //printf("* %s: received: '%s'\n", __FUNCTION__, buffer);

      if (strncmp(buffer, res_cclk, strlen(res_cclk)) == 0)
        {
          memset(&now, 0, sizeof(struct tm));

          res = sscanf(buffer, "+CCLK: \"%02d/%02d/%02d,%02d:%02d:%02d+%02d\"",
                       &now.tm_year, &now.tm_mon, &now.tm_mday,
                       &now.tm_hour, &now.tm_min, &now.tm_sec,
                       &tz_offset);
          if (res != 7)
            {
              printf("! %s: parse +CCLK result failed (%s -> %d).\n",
                     __FUNCTION__, buffer, res);
              return -1;
            }

          printf("* %s: response: '%s'\n", __FUNCTION__, buffer);
          tz_offset /= 4;

          now.tm_year += 100;
          now.tm_mon -= 1;
          now.tm_hour += tz_offset;

          time_now = mktime(&now);
          printf("* %s: now: %d\n", __FUNCTION__, time_now);

          if (now.tm_year < 140)
            {
              tv.tv_sec = time_now;
              tv.tv_usec = 0;

              settimeofday(&tv, NULL);
              result = 0;
            }
          else
            {
              syslog(LOG_ERR, "! %s: invalid +CCLK response (%s)\n",
                     __FUNCTION__, buffer);
            }
        }
      else if (strstr(buffer, "OK"))
        {
          //printf("* %s: 'OK' received.\n", __FUNCTION__);
          break;
        }
    }

  return result;
}

/****************************************************************************
 * Name: [BG96] get ICCID
 ****************************************************************************/

static int bg96_get_iccid(serial_t *ser, char *iccid)
{
  int res, len, result = -1;
  const char *cmd = "AT+CCID\r\n";
  const char *fmt = "+CCID: %s";
  char _iccid[20];

  res = ser_write(ser, cmd, strlen(cmd));

  if (res < 0)
    {
      return -1;
    }

  /* wait for 'OK' */
  while (1)
    {
      memset(buffer, 0, 128);
      res = ser_readline(ser, buffer, 128, '\n', 2000);

      if (res <= 0)
        {
          break;
        }
      else
        {
          if (strstr(buffer, "+CCID:"))
            {
              res = sscanf(buffer, fmt, &_iccid);

              if (res == 1)
                {
                  len = strlen(_iccid);

                  if (_iccid[len - 1] == 'F')
                    {
                      _iccid[len - 1] = '\0';
                      len--;
                    }

                  printf("* %s: ICCID: %s\n", __FUNCTION__, _iccid);

                  if (len > 0)
                    {
                      strncpy(iccid, _iccid, len);
                      result = 0;
                    }
                }
            }
          else if (strstr(buffer, "OK"))
            {
              break;
            }
        }
    }

  return result;
}

/****************************************************************************
 * Name: gen_payload
 ****************************************************************************/

static int gen_payload(struct xg50_stat *stat, char *buf)
{
  int len;
  int coeff = 10000000;
  int lat_min, lon_min, lat_sec, lon_sec;

  lat_min = stat->lat / coeff;
  lat_sec = stat->lat % coeff;
  lat_sec /= 6;

  lon_min = stat->lon / coeff;
  lon_sec = stat->lon % coeff;
  lon_sec /= 6;

  len = sprintf(buf, "{\"lat\" : %d.%06d, \"lon\" : %d.%06d}",
                lat_min, lat_sec, lon_min, lon_sec);
  return len;
}

/****************************************************************************
 * Name: initialize swtich gpio
 ****************************************************************************/

static int init_switch(task_t *task)
{
  int fd, res;

  fd = open("/dev/gpout0", O_RDWR);
  if (fd < 0)
    {
      printf("! open GPOUT failed.\n");
      return -1;
    }

  res = ioctl(fd, GPIOC_WRITE, (unsigned long) 0);
  close(fd);

  if (res < 0)
    {
      printf("! write GPIO failed.\n");
      return -1;
    }

  return 0;
}

/****************************************************************************
 * Name: wait for switch
 ****************************************************************************/

static int wait_switch(struct tm *tm_deadline)
{
  int res, fd, value;
  struct tm tm_now;
  long nsec;
  time_t now, deadline;

  fd = open("/dev/gpin0", O_RDONLY);

  if (fd < 0)
    {
      printf("! open GPIN failed.\n");
      return -1;
    }

  deadline = mktime(tm_deadline);

  while (1)
    {
      up_rtc_getdatetime_with_subseconds(&tm_now, &nsec);
      now = mktime(&tm_now);

      if (now >= deadline)
        {
          res = 0;
          break;
        }

      res = ioctl(fd, GPIOC_READ, (unsigned long)((uintptr_t) &value));

      if (value == 0)
        {
          printf("### Switch ON detected!\n");
          res = 1;
          break;
        }
      else
        {
          usleep(50 * 1000);
        }
    }

  close(fd);
  return res;
}

/****************************************************************************
 * Name: Enable GPS
 ****************************************************************************/

static int bg96_enable_gps(serial_t *ser)
{
  int res;

  if ((res = bg96_send_AT_command(ser, "AT+QGPS=1\r\n")) != 0)
    {
      printf("! %s: AT+QGPS=1 failed.\n", __FUNCTION__);
      return res;
    }

  if ((res = bg96_send_AT_command(ser, "AT+QGPSCFG=\"nmeasrc\",1\r\n")) != 0)
    {
      printf("! %s: AT+QGPSCFG=\"nmeasrc\",1 failed.\n", __FUNCTION__);
    }
  else
    {
      printf("* %s: Acquisition of NMEA via command succeeded.\n",
             __FUNCTION__);
    }

  return res;
}

static int bg96_get_gps_location(serial_t *ser, int *lat, int *lon)
{
  int res;
  bool result;
  const char *nmearmc = "AT+QGPSGNMEA=\"RMC\"\r\n";
  char rmc_buf[128];
  struct minmea_sentence_rmc frame;

  res = ser_write(ser, nmearmc, strlen(nmearmc));
  if (res < 0)
    {
      return -1;
    }

  while (1)
    {
      res = ser_readline(ser, buffer, 256, '\n', 1000);

      if (res > 0)
        {
          buffer[res] = '\0';

          if (strncmp(buffer, "+QGPS", 5) == 0)
            {
              chop(buffer);
              strncpy(rmc_buf, &buffer[12], strlen(buffer) - 12);
            }
        }
      else
        {
          break;
        }

      if (strncmp(buffer, "OK", 2) == 0)
        {
          break;
        }
    }

  printf("RMC: \"%s\"\n", rmc_buf);
  result = minmea_parse_rmc(&frame, (const char *) &rmc_buf);

  if (result == true && frame.valid)
    {
      struct timespec ts;
      struct timeval tv;

      *lat = minmea_rescale(&frame.latitude, 100000);
      *lon = minmea_rescale(&frame.longitude, 100000);

      printf("Fixed-point Latitude...........: %d\n", *lat);
      printf("Fixed-point Longitude..........: %d\n", *lon);
      minmea_gettime(&ts, &frame.date, &frame.time);
      printf("timestamp......................: %d\n", ts.tv_sec);

      tv.tv_sec = ts.tv_sec + 9 * 60 * 60;
      tv.tv_usec = ts.tv_nsec / NSEC_PER_USEC;
      settimeofday(&tv, NULL);

      led_onoff(5, 1);
      led_onoff(4, 0);

      res = 0;
    }
  else
    {
      led_onoff(5, 0);
      led_onoff(4, 1);

      res = -1;
    }

  return res;
}

/****************************************************************************
 * Name: Handler (SETUP)
 ****************************************************************************/

static task_state_t setup_bg96(task_t *task)
{
  int res;
  config_t *config;
  struct xg50_stat *bg96;
  struct tca6507_onoff_s set;

  config = &task->config;
  bg96 = &task->info;

  bg96->id = config->id;
  bg96->valid = 0;

  res = bg96_echo_off(task->ser);

  if (res < 0)
    {
      printf("! %s: echo off failed.\n", __FUNCTION__);
      sleep(5);

      return STAT_SETUP;
    }

  res = bg96_setup_apn(task->ser, 1, config->apn, config->user,
                       config->password);

  if (res < 0)
    {
      printf("! %s: setup_apn failed.\n", __FUNCTION__);
      sleep(5);

      return STAT_SETUP;
    }

  bg96_enable_gps(task->ser);

  bg96_send_AT_command(task->ser, "AT+CFUN=4\r\n");
  sleep(2);
  bg96_send_AT_command(task->ser, "AT+CFUN=1\r\n");
  sleep(1);
  bg96_send_AT_command(task->ser, "AT+CGATT?\r\n");
  sleep(1);

  res = bg96_get_iccid(task->ser, bg96->iccid);

  if (res < 0)
    {
      printf("! %s: get_iccid failed.\n", __FUNCTION__);
      sleep(5);

      return STAT_SETUP;
    }

  printf("* %s: ICCID: %s\n", __FUNCTION__, bg96->iccid);
  while (1)
    {
      res = bg96_sync_clock(task->ser);

      if (res == 0)
        {
          led_onoff(0, 0);
          led_onoff(1, 1);
          return STAT_IDLE;
        }

      led_onoff(0, 1);
      led_onoff(1, 0);
      sleep(5);
    }
}

/****************************************************************************
 * Name: Handler (IDLE)
 ****************************************************************************/

static task_state_t wait_next_measurement_time(task_t *task)
{
  struct tm tm, next_tm;
  time_t now, next;
  long nsec, sleeptime;
  useconds_t usec;
  uint16_t interval;

  up_rtc_getdatetime_with_subseconds(&tm, &nsec);
  now = mktime(&tm);

  interval = task->interval;
  if (interval < 10)
    {
      interval = 10;
    }

  next = ((now + 1 + interval) / interval) * interval;
  localtime_r(&next, &next_tm);

  sleeptime = (next - now) * 1000 - (nsec / 1000000);

  if (sleeptime < 0)
    {
      sleeptime = 0;
    }

  if (sleeptime > 0)
    {
      usec = sleeptime * 1000;

      printf("--- %s: next: %04d/%02d/%02d %02d:%02d:%02d -> "
             "sleep %d [msec] (%lu [usec])\n",
             __FUNCTION__,
             next_tm.tm_year + 1900, next_tm.tm_mon + 1, next_tm.tm_mday,
             next_tm.tm_hour, next_tm.tm_min, next_tm.tm_sec,
             sleeptime, usec);

      wait_switch(&next_tm);

      /* wakeup! */
      up_rtc_getdatetime_with_subseconds(&tm, &nsec);
      now = mktime(&tm);
      localtime_r(&now, &next_tm);

      printf("--- %s: wakeup: %04d/%02d/%02d %02d:%02d:%02d\n",
             __FUNCTION__,
             next_tm.tm_year + 1900, next_tm.tm_mon + 1, next_tm.tm_mday,
             next_tm.tm_hour, next_tm.tm_min, next_tm.tm_sec);
    }

  return STAT_MEASURE;
}

/****************************************************************************
 * Name: Handler (MEASURE)
 ****************************************************************************/

static task_state_t measurement(task_t *task)
{
  int res, lat, lon;

  res = bg96_get_gps_location(task->ser, &lat, &lon);

  if (res != 0)
    {
      task->interval = 10;
      return STAT_IDLE;
    }
  else
    {
      task->interval = 60;
      task->info.lat = lat;
      task->info.lon = lon;
      return STAT_CONNECT;
    }
}

/****************************************************************************
 * Name: Handler (CONNECT)
 ****************************************************************************/

static task_state_t connect_bg96(task_t *task)
{
  int res;
  config_t *config;

  /* activate PDP context */
  res = bg96_connect(task->ser);

  if (res < 0)
    {
      printf("! %s: Activate PDP context failed -> RESET\n", __FUNCTION__);
      sleep(2);
      bg96_reset();
      return STAT_IDLE;
    }

  /* open socket */
  config = &task->config;

  printf("* %s: try to connect to %s:%d ...\n", __FUNCTION__,
         config->host, config->port);
  res = bg96_connect_tcp(task->ser, config->host, config->port);

  if (res < 0)
    {
      printf("! %s: open socket failed -> RESET\n", __FUNCTION__);
      bg96_disconnect(task->ser);
      sleep(2);
      bg96_reset();
      return STAT_IDLE;
    }

  printf("* connected.\n");

  task->sock_idx = res;

  return STAT_UPLOAD;
}

/****************************************************************************
 * Name: Handler (UPLOAD)
 ****************************************************************************/

task_state_t upload_data(task_t *task)
{
  int len, res;
  struct xg50_stat *stat = &task->info;
  char sendbuf[128];

  len = gen_payload(stat, sendbuf);
  printf("* %s: payload '%s' (length: %d)\n", __FUNCTION__, sendbuf, len);

  res = bg96_send_tcp(task->ser, task->sock_idx, (const char *) sendbuf, len);

  if (res > 0)
    {
      printf("  -> send_tcp: %d bytes.\n", res);
    }

  return STAT_WAIT_RESPONSE;
}

/****************************************************************************
 * Name: Handler (WAIT_RESPONSE)
 ****************************************************************************/

task_state_t wait_response(task_t *task)
{
  int res, retry = 1;
  struct urc_info urc;
  task_state_t next = STAT_DISCONNECT;

  while (retry)
    {
      res = bg96_handle_URC(task->ser, &urc);

      if (res < 0)
        {
          /* timeouted ? */
          break;
        }

      switch (urc.code)
        {
        case URC_CLOSED:
          next = STAT_DISCONNECT;
          retry = 0;
          break;

        case URC_DEACTIVATED:
          next = STAT_SETUP;
          retry = 0;
          break;

        case URC_RECEIVED:
          printf("* %s: packet received.\n", __FUNCTION__);
          next = STAT_RECEIVE_RESPONSE;
          retry = 0;
          break;

        default:
          break;
        }
    }

  return next;
}

/****************************************************************************
 * Name: Handler (RECEIVE_RESPONSE)
 ****************************************************************************/

task_state_t receive_response(task_t *task)
{
  int readlen;
  uint16_t waittime;

  readlen = bg96_recv_tcp(task->ser, task->sock_idx, buffer, BUFSIZE);

  if (readlen < 0)
    {
      printf("! %s: TCP receive failed.\n", __FUNCTION__);
      return STAT_DISCONNECT;
    }

  if (readlen != sizeof(uint16_t))
    {
      printf("! %s: TCP receive lenght missmatch (%d)\n", __FUNCTION__, readlen);
      return STAT_DISCONNECT;
    }

  waittime = ntohs(*(uint16_t *) buffer);
  printf("* %s: next wait time = %u\n", __FUNCTION__, waittime);
  task->interval = waittime;

  return STAT_DISCONNECT;
}

/****************************************************************************
 * Name: Handler (DISCONNECT)
 ****************************************************************************/

static task_state_t disconnect_bg96(task_t *task)
{
  printf("---- %s start...\n", __FUNCTION__);

  bg96_disconnect_tcp(task->ser, task->sock_idx);
  bg96_disconnect(task->ser);

  return STAT_IDLE;
}

/****************************************************************************
 * Name:
 ****************************************************************************/

static int mainloop(task_t *task)
{
  task_state_t next;

  while (1)
    {
      switch (task->state)
        {
        case STAT_SETUP:
          printf("@@@@@ BG96: %s: STAT_SETUP\n", __FUNCTION__);
          next = setup_bg96(task);
          break;

        case STAT_IDLE:
          printf("@@@@@ BG96: %s: STAT_IDLE\n", __FUNCTION__);
          next = wait_next_measurement_time(task);
          break;

        case STAT_MEASURE:
          printf("@@@@@ BG96: %s: STAT_MEASURE\n", __FUNCTION__);
          next = measurement(task);
          break;

        case STAT_CONNECT:
          printf("@@@@@ BG96: %s: STAT_CONNECT\n", __FUNCTION__);
          next = connect_bg96(task);
          break;

        case STAT_UPLOAD:
          printf("@@@@@ BG96: %s: STAT_UPLOAD\n", __FUNCTION__);
          next = upload_data(task);
          break;

        case STAT_WAIT_RESPONSE:
          printf("@@@@@ BG96: %s: STAT_WAIT_RESPONSE\n", __FUNCTION__);
          next = wait_response(task);
          break;

        case STAT_RECEIVE_RESPONSE:
          printf("@@@@@ BG96: %s: STAT_RECEIVE_RESPONSE\n", __FUNCTION__);
          next = receive_response(task);
          break;

        case STAT_DISCONNECT:
          printf("@@@@@ BG96: %s: STAT_DISCONNECT\n", __FUNCTION__);
          next = disconnect_bg96(task);
          break;

        default:
          printf("@@@@@ BG96: %s: DEFAULT -> STAT_IDLE\n", __FUNCTION__);
          next = STAT_IDLE;
          break;
        }

      task->state = next;
    }

  return 0;
}

/****************************************************************************
 * Name: task initialize
 ****************************************************************************/

static task_t *init_task(void)
{
  task_t *task;
  char *bg96_path = "/dev/ttyS3";
  char *gps_path = "/dev/ttyS1";

  if (!(task = zalloc(sizeof(task_t))))
    {
      return NULL;
    }

  load_config(&task->config);

  if (task->config.valid != 1)
    {
      strcpy(task->config.apn, "soracom.io");
      strcpy(task->config.user, "sora");
      strcpy(task->config.password, "sora");
      strcpy(task->config.host, "harvest.soracom.io");
      task->config.port = 8514;
    }

  printf("=== BG96 configurations ===\n");
  printf(" * APN: %s\n", task->config.apn);
  printf(" * user: %s\n", task->config.user);
  printf(" * passowrd: %s\n", task->config.password);
  printf(" * host: %s\n", task->config.host);
  printf(" * port: %d\n", task->config.port);

  task->ser = serial_open(bg96_path, 115200, 0);
  task->ser_gps = serial_open(gps_path, 115200, 0);
  task->state = STAT_SETUP;

  init_switch(task);

  if ((find_i2c_device(I2C_ADDR_TCA6507) == 0) &&
      (find_i2c_device(I2C_ADDR_PCA9534) == 0))
    {
      task->led_found = 1;
      fd_led = open("/dev/leddrv0", O_RDONLY);
    }

  sleep(1);

  return task;
}

/****************************************************************************
 * Name: boot task
 ****************************************************************************/

static int boot_main(int argc, char **argv)
{
  task_t *task = init_task();

  if (!task)
    {
      return -1;
    }

  return mainloop(task);
}

/****************************************************************************
 * Name: setting or boot task
 ****************************************************************************/

static int select_mode(int argc, char **argv)
{
  int sw_state;

  boardctl(BIOC_GET_LEDSW, (uintptr_t) &sw_state);

  if (sw_state == 0)
    {
      /* configuration mode */
      return configuration(argc, argv);
    }

  return boot_main(argc, argv);
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * serial_relay_main
 ****************************************************************************/

#ifdef CONFIG_BUILD_KERNEL
int main(int argc, FAR char *argv[])
#else
int bg96_main(int argc, char *argv[])
#endif
{
  return select_mode(argc, argv);
}
