/****************************************************************************
 * apps/centurysys/mobile_watch_nx/mobile_watch_nx.c
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
#include <string.h>
#include <fcntl.h>
#include <getopt.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sys/time.h>
#include <net/if.h>
#include <netpacket/netlink.h>

#include <debug.h>

#include <nuttx/board.h>
#include <arch/board/board.h>
#include <nuttx/leds/userled.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

struct task_data
{
  int led_fd;
  int sockfd;
  bool ppp_stat;
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 *
 ****************************************************************************/

static void handle_event_link(struct task_data *self, char *ifname,
                              bool new_del)
{
  if (strncmp(ifname, "ppp0", 4) != 0)
    {
      return;
    }

  if ((self->ppp_stat == true) && (new_del == false))
    {
      userled_set_t set = 0;
      _info("PPP down detected.\n");

      ioctl(self->led_fd, ULEDIOC_SETALL, set);
      self->ppp_stat = false;
    }
}

/****************************************************************************
 *
 ****************************************************************************/

static void handle_event_addr(struct task_data *self, char *ifname,
                              bool new_del)
{
  if (strncmp(ifname, "ppp0", 4) != 0)
    {
      return;
    }

  if ((self->ppp_stat == false) && (new_del == true))
    {
      userled_set_t set = BOARD_MOBILE0_G_BIT;
      _info("PPP up detected.\n");

      ioctl(self->led_fd, ULEDIOC_SETALL, set);
      self->ppp_stat = true;
    }
}

/****************************************************************************
 *
 ****************************************************************************/

static void handle_event(struct task_data *self, struct nlmsghdr *hdr)
{
  char ifname[IFNAMSIZ];
  struct ifinfomsg *ifi = NULL;
  struct ifaddrmsg *ifa = NULL;
  bool new_del;

  switch (hdr->nlmsg_type)
    {
      case RTM_NEWLINK:
      case RTM_DELLINK:
        /* New/Delete LINK Status */

        ifi = NLMSG_DATA(hdr);
        if_indextoname(ifi->ifi_index, ifname);
        new_del = hdr->nlmsg_type == RTM_NEWLINK ? true : false;

        handle_event_link(self, ifname, new_del);
        break;

      case RTM_NEWADDR:
      case RTM_DELADDR:
        /* New/Delete IP Address */

        ifa = NLMSG_DATA(hdr);
        if_indextoname(ifa->ifa_index, ifname);
        new_del = hdr->nlmsg_type == RTM_NEWADDR ? true : false;

        handle_event_addr(self, ifname, new_del);
        break;

      default:
        break;
    }
}

/****************************************************************************
 *
 ****************************************************************************/

static void watch_event(struct task_data *self)
{
  struct pollfd pfd;
  char buf[1024];
  struct nlmsghdr *hdr;
  int len;
  int n;

  _info("Monitoring started.\n");

  memset(buf, 0, sizeof(buf));

  pfd.fd = self->sockfd;
  pfd.events = POLLIN;
  pfd.revents = 0;

  while (1) {
    n = poll(&pfd, 1, -1);
    len = recv(self->sockfd, buf, sizeof(buf), 0);

    if (len < 0)
      {
        break;
      }

    for (hdr = (struct nlmsghdr *)buf; NLMSG_OK(hdr, len);
         hdr = NLMSG_NEXT(hdr, len))
      {
        handle_event(self, hdr);
      }
  }

  return;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 *
 ****************************************************************************/

int main(int argc, char *argv[])
{
  struct task_data self;
  int ret = ERROR;
  socklen_t alen;
  struct sockaddr_nl local;

  self.led_fd = open("/dev/userleds", O_RDWR);
  if (self.led_fd < 0)
    {
      _warn("open userleds failed.\n");
    }

  self.sockfd = socket(AF_NETLINK, SOCK_DGRAM, NETLINK_ROUTE);

  if (self.sockfd < 0)
    {
      _err("?? NETLINK socket not supported.\n");
      goto exit;
    }

  memset(&local, 0, sizeof(struct sockaddr_nl));
  local.nl_family = AF_NETLINK;
  local.nl_groups = RTMGRP_LINK | RTMGRP_NOTIFY | RTMGRP_IPV4_IFADDR;

  alen = sizeof(struct sockaddr_nl);
  ret = bind(self.sockfd, (struct sockaddr *)&local, alen);

  if (ret < 0)
    {
      goto exit;
    }

  if (getsockname(self.sockfd, (struct sockaddr *)&local, &alen) < 0 ||
      alen != sizeof(local) || local.nl_family != AF_NETLINK)
    {
      goto exit;
    }

  watch_event(&self);
  ret = OK;

exit:
  close(self.sockfd);
  return ret;
}
