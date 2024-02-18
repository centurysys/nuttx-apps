/****************************************************************************
 * apps/netutils/pppd/pppd.c
 *
 *   Copyright (C) 2015 Max Nekludov. All rights reserved.
 *   Author: Max Nekludov <macscomp@gmail.com>
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

#include <sys/socket.h>
#include <sys/ioctl.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sched.h>
#include <fcntl.h>
#include <errno.h>
#include <poll.h>
#include <time.h>
#include <signal.h>
#include <debug.h>
#include <unistd.h>

#include <netinet/in.h>
#include <net/if.h>
#include <nuttx/net/tun.h>

#include "ppp.h"

#include "netutils/chat.h"
#include "netutils/pppd.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#ifndef SIGTERM
#define SIGTERM 15
#endif

/****************************************************************************
 * Private Data
 ****************************************************************************/

volatile static int sig_received;

/****************************************************************************
 * External Functions
 ****************************************************************************/

#ifdef PPP_ARCH_HAVE_MODEM_RESET
void ppp_arch_modem_reset(const char *tty);
#endif

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: sig_handler
 ****************************************************************************/

static void sig_handler(int signo, siginfo_t *info, void *ucontext)
{
  sig_received = signo;
}

/****************************************************************************
 * Name: setup_signal
 ****************************************************************************/

static void setup_signal(void)
{
  struct sigaction sa;

  sig_received = 0;

  memset(&sa, 0, sizeof(struct sigaction));

  sigemptyset(&sa.sa_mask);
  sa.sa_sigaction = sig_handler;

  if (sigaction(SIGTERM, &sa, NULL) < 0)
    {
      _err("pppd: sigaction() failed.\n");
    }
}

/****************************************************************************
 * Name: make_nonblock
 ****************************************************************************/

static int make_nonblock(int fd)
{
  int flags;

  if ((flags = fcntl(fd, F_GETFL, 0)) < 0)
    {
      return flags;
    }

  if ((flags = fcntl(fd, F_SETFL, flags | O_NONBLOCK)) < 0)
    {
      return flags;
    }

  return 0;
}

/****************************************************************************
 * Name: tun_alloc
 ****************************************************************************/

static int tun_alloc(char *dev)
{
  struct ifreq ifr;
  int fd;
  int errcode;

  if ((fd = open("/dev/tun", O_RDWR)) < 0)
    {
      return fd;
    }

  debug_printf("tun fd:%i\n", fd);

  if ((errcode = make_nonblock(fd)) < 0)
    {
      close(fd);
      return errcode;
    }

  memset(&ifr, 0, sizeof(ifr));
  ifr.ifr_flags = IFF_TUN;
  if (*dev)
    {
      strlcpy(ifr.ifr_name, dev, IFNAMSIZ);
    }

  if ((errcode = ioctl(fd, TUNSETIFF, (unsigned long)&ifr)) < 0)
    {
      close(fd);
      return errcode;
    }

  strcpy(dev, ifr.ifr_name);

  return fd;
}

/****************************************************************************
 * Name: open_tty
 ****************************************************************************/

static int open_tty(const char *dev)
{
  int fd;
  int errcode;

  if ((fd = open(dev, O_RDWR)) < 0)
    {
      return fd;
    }

  if ((errcode = make_nonblock(fd)) < 0)
    {
      close(fd);
      return errcode;
    }

  debug_printf("tty fd:%i\n", fd);

  return fd;
}

/****************************************************************************
 * Name: ppp_check_errors
 ****************************************************************************/

static uint8_t ppp_check_errors(FAR struct ppp_context_s *ctx)
{
  uint8_t ret = 0;

  /* Check Errors */

  if (ctx->lcp_state & (LCP_TX_TIMEOUT | LCP_RX_TIMEOUT | LCP_TERM_PEER))
    {
      ret = 1;
    }

#ifdef CONFIG_NETUTILS_PPPD_PAP
  if (ctx->pap_state & (PAP_TX_AUTH_FAIL | PAP_RX_AUTH_FAIL |
                        PAP_TX_TIMEOUT | PAP_RX_TIMEOUT))
    {
      ret = 2;
    }
#endif

  if (ctx->ipcp_state & (IPCP_TX_TIMEOUT))
    {
      ret = 3;
    }

  return ret;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: ppp_disconnect
 ****************************************************************************/

void ppp_disconnect(struct ppp_context_s *ctx)
{
  int ret, modem;
  const struct pppd_settings_s *pppd_settings = ctx->settings;

  netlib_ifdown((char*)ctx->ifname);

  lcp_disconnect(ctx, ++ctx->ppp_id);
  usleep(100);
  lcp_disconnect(ctx, ++ctx->ppp_id);
  usleep(100);
  write(ctx->ctl.fd, "+++\r\n", 5);
  usleep(100);

  if (pppd_settings->disconnect_script)
    {
      ret = chat(&ctx->ctl, pppd_settings->disconnect_script);
      if (ret < 0)
        {
          _info("disconnect script failed\n");
        }

      modem = TIOCM_DTR;
      ret = ioctl(ctx->ctl.fd, TIOCMBIC, (unsigned long) &modem);
    }

  ctx->ip_len = 0;
}

/****************************************************************************
 * Name: ppp_reconnect
 ****************************************************************************/

void ppp_reconnect(FAR struct ppp_context_s *ctx, bool first)
{
  int ret;
  int retry = PPP_MAX_CONNECT;
  const struct pppd_settings_s *pppd_settings = ctx->settings;
  netlib_ifdown((char *)ctx->ifname);

  _info("pppd started.\n");

  lcp_disconnect(ctx, ++ctx->ppp_id);
  usleep(100);
  lcp_disconnect(ctx, ++ctx->ppp_id);
  usleep(100);

  if (!first)
    {
      //write(ctx->ctl.fd, "+++\r\n", 5);
      //usleep(100);

      if (pppd_settings->disconnect_script)
        {
          ret = chat(&ctx->ctl, pppd_settings->disconnect_script);
          if (ret < 0)
            {
              debug_printf("ppp: disconnect script failed\n");
            }
        }
    }

  if (pppd_settings->connect_script)
    {
      do
        {
          ret = chat(&ctx->ctl, pppd_settings->connect_script);
          if (ret < 0)
            {
              debug_printf("ppp: connect script failed\n");
              --retry;
              if (retry == 0)
                {
                  retry = PPP_MAX_CONNECT;
#ifdef PPP_ARCH_HAVE_MODEM_RESET
                  ppp_arch_modem_reset(pppd_settings->ttyname);
#endif
                  sleep(30);
                }
              else
                {
                  sleep(pppd_settings->holdoff);
                }
            }
        }
      while (ret != 0);
    }

  ppp_init(ctx);
  ppp_connect(ctx);

  ctx->ip_len = 0;
}

/****************************************************************************
 * Name: ppp_arch_clock_seconds
 ****************************************************************************/

time_t ppp_arch_clock_seconds(void)
{
  struct timespec ts;

  if (clock_gettime(CLOCK_MONOTONIC, &ts) < 0)
    {
      return 0;
    }

  return ts.tv_sec;
}

/****************************************************************************
 * Name: ppp_arch_getchar
 ****************************************************************************/

int ppp_arch_getchar(FAR struct ppp_context_s *ctx, FAR uint8_t * c)
{
  int ret;

  ret = read(ctx->ctl.fd, c, 1);
  return ret == 1 ? ret : 0;
}

/****************************************************************************
 * Name: ppp_arch_putchar
 ****************************************************************************/

int ppp_arch_putchar(FAR struct ppp_context_s *ctx, uint8_t c)
{
  int ret;
  struct pollfd fds;

  ret = write(ctx->ctl.fd, &c, 1);
  if (ret < 0 && errno == EAGAIN)
    {
      fds.fd = ctx->ctl.fd;
      fds.events = POLLOUT;
      fds.revents = 0;

      ret = poll(&fds, 1, 1000);
      if (ret > 0)
        {
          ret = write(ctx->ctl.fd, &c, 1);
        }
    }

  return ret == 1 ? ret : 0;
}

/****************************************************************************
 * Name: pppd
 ****************************************************************************/

int pppd(const struct pppd_settings_s *pppd_settings)
{
  struct pollfd fds[2];
  int ret;
  FAR struct ppp_context_s *ctx;

  setup_signal();

  ctx = (struct ppp_context_s *)malloc(sizeof(struct ppp_context_s));
  memset(ctx, 0, sizeof(struct ppp_context_s));
  strlcpy((char *)ctx->ifname, "ppp%d", sizeof(ctx->ifname));

  ctx->settings = pppd_settings;
  ctx->if_fd = tun_alloc((char *)ctx->ifname);
  if (ctx->if_fd < 0)
    {
      free(ctx);
      return 2;
    }

  ctx->ctl.fd = open_tty(pppd_settings->ttyname);
  if (ctx->ctl.fd < 0)
    {
      close(ctx->if_fd);
      free(ctx);
      return 2;
    }

  ctx->ctl.echo = false;
  ctx->ctl.verbose = false;
  ctx->ctl.timeout = 30;

  fds[0].fd = ctx->if_fd;
  fds[0].events = POLLIN;

  fds[1].fd = ctx->ctl.fd;
  fds[1].events = POLLIN;

  ppp_init(ctx);
  ppp_reconnect(ctx, true);

  while (1)
    {
      fds[0].revents = fds[1].revents = 0;

      ret = poll(fds, 2, 1000);

      if (ret > 0 && fds[0].revents & POLLIN)
        {
          ret = read(ctx->if_fd, ctx->ip_buf, PPP_RX_BUFFER_SIZE);
          debug_printf("read from tun :%i\n", ret);
          if (ret > 0)
            {
              ctx->ip_len = ret;
              ppp_send(ctx);
              ctx->ip_len = 0;
            }
        }

      ppp_poll(ctx);

      if (sig_received != 0)
        {
          _info("Signal Received: %d\n", sig_received);
        }

      if (ppp_check_errors(ctx) || sig_received != 0)
        {
          _info("Connection Terminated.\n");

          if (sig_received != 0 || !ctx->settings->persist)
            {
              ppp_disconnect(ctx);
              break;
            }
          else
            {
              if (ctx->settings->holdoff > 0)
                {
                  _info("Wait %d seconds.\n", ctx->settings->holdoff);
                  sleep(ctx->settings->holdoff);
                }

              ppp_reconnect(ctx, false);
            }
        }
      else
        {
          if (ctx->ip_len > 0)
            {
              ret = write(ctx->if_fd, ctx->ip_buf, ctx->ip_len);
              ctx->ip_len = 0;

              ret = read(ctx->if_fd, ctx->ip_buf, PPP_RX_BUFFER_SIZE);
              if (ret > 0)
                {
                  ctx->ip_len = ret;
                  ppp_send(ctx);
                  ctx->ip_len = 0;
                }
            }
        }
    }

  close(ctx->ctl.fd);
  close(ctx->if_fd);

  return 1;
}
