/****************************************************************************
 * Mailbox library
 *
 *  Copyright (C) 2012,2018-2019 Century Systems
 *  Author: Takeyoshi Kikuchi <kikuchi@centurysys.co.jp>
 *
 *  Last Modified: 2020/04/12 17:01:01 kikuchi
 ****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <mqueue.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/time.h>

#include "xg50/mailbox.h"

static LIST_HEAD(mailbox_list);

struct mailbox *mailbox_create(const char *name, size_t size, size_t depth,
                               int use_poll)
{
  struct mailbox *mbox;
  mqd_t mq;
  struct mq_attr attr;
  int __err = 0;

  if (strlen(name) > NAME_LEN_MAX - 10)
    {
      __err = ENAMETOOLONG;
      goto err1;
    }

  if (!(mbox = malloc(sizeof(struct mailbox))))
    {
      __err = errno;
      goto err1;
    }

  memset(mbox, 0, sizeof(struct mailbox));
  strncpy(mbox->name, name, strlen(name));
  INIT_LIST_HEAD(&mbox->list);

  memset(&attr, 0, sizeof(struct mq_attr));
  attr.mq_maxmsg = depth;
  attr.mq_msgsize = size;
  attr.mq_flags = 0;

  if (!(mq = mq_open(name, O_CREAT | O_RDWR | O_EXCL, 0666, &attr)))
    {
      __err = errno;
      goto err2;
    }

  mbox->mq = mq;
  mbox->size = size;
  mbox->depth = depth;
  mbox->use_poll = (use_poll != 0 ? 1 : 0);

  if (mbox->use_poll == 1)
    {
      sprintf(mbox->fifoname, "/tmp/fifo_%s", name);

      if (mkfifo(mbox->fifoname, 0666) != 0)
        {
          syslog(LOG_ERR, "! %s: mkfifo() failed, '%s'\n",
                 __FUNCTION__, strerror(errno));
          goto err3;
        }
    }
  else
    {
      mbox->pipe_rd = -1;
    }

  list_add_tail(&mbox->list, &mailbox_list);

  return mbox;

err3:
  mq_close(mq);
err2:
  free(mbox);
err1:
  errno = __err;
  return NULL;
}

struct mailbox *mailbox_open(const char *name)
{
  struct mailbox *mbox = NULL, *tmp;
  struct list_head *list, *list_tmp;
  int len;

  len = strlen(name);

  sched_lock();

  list_for_each_safe(list, list_tmp, &mailbox_list)
    {
      tmp = list_entry(list, struct mailbox, list);

      if (strncmp(tmp->name, name, len) == 0)
        {
          mbox = tmp;
          break;
        }
    }

  sched_unlock();

  return mbox;
}

int mailbox_destroy(struct mailbox *mbox)
{
  if (!mbox)
    {
      errno = EFAULT;
      return -1;
    }

  sched_lock();

  list_del(&mbox->list);

  if (mbox->use_poll == 1 && mbox->pipe_rd > 0)
    {
      close(mbox->pipe_rd);
    }

  mq_close(mbox->mq);
  mq_unlink(mbox->name);
  free(mbox);

  sched_unlock();

  return 0;
}

int mailbox_send(struct mailbox *mbox, void *data, unsigned long timeout)
{
  size_t msglen;
  struct timespec abstime;
  struct timeval now;
  int res, tmp, fd;

  msglen = mbox->size;

  if (timeout == 0)
    {
      /* blocking */
      res = mq_send(mbox->mq, (const char *) data, msglen, 10);
    }
  else
    {
      /* with timeout */
      gettimeofday(&now, NULL);
      abstime.tv_sec = now.tv_sec + timeout / 1000;
      abstime.tv_nsec = now.tv_usec * 1000 + (timeout % 1000) * 1000000;

      res = mq_timedsend(mbox->mq, (const char *) data, msglen, 10,
                         &abstime);
    }

  if (res == 0)
    {
      mbox->sendcount++;

      if (mbox->use_poll == 1)
        {
          /* for poll() */
          if ((fd = open(mbox->fifoname, O_WRONLY)) > 0)
            {
              tmp = write(fd, "W", 1);

              if (tmp < 0)
                {
                  syslog(LOG_ERR, "! %s: [%s] write(%d) failed, '%s'\n",
                         __FUNCTION__, mbox->name, fd, strerror(errno));
                }

              close(fd);
            }
        }
    }
  else
    {
      mbox->errcount++;
    }

  return res;
}

int mailbox_recv(struct mailbox *mbox, void *data, unsigned long timeout)
{
  size_t msglen;
  struct timespec abstime;
  struct timeval now;
  int res, tmp;

  msglen = mbox->size;

  if (timeout == 0)
    {
      /* blocking */
      res = mq_receive(mbox->mq, data, msglen, NULL);
    }
  else
    {
      /* with timeout */
      gettimeofday(&now, NULL);
      abstime.tv_sec = now.tv_sec + timeout / 1000;
      abstime.tv_nsec = now.tv_usec * 1000 + (timeout % 1000) * 1000000;

      if (abstime.tv_nsec >= 1000000000)
        {
          abstime.tv_sec += 1;
          abstime.tv_nsec -= 1000000000;
        }

      res = mq_timedreceive(mbox->mq, data, msglen, NULL, &abstime);
    }

  if (res > 0)
    {
      if (mbox->use_poll == 1)
        {
          char buf[1];

          if (mbox->pipe_rd == 0)
            {
              mbox->pipe_rd = open(mbox->fifoname, O_RDWR);
            }

          tmp = read(mbox->pipe_rd, buf, 1);

          if (tmp < 0)
            {
              syslog(LOG_ERR, "! %s: [%s] read(%d) failed, '%s'\n",
                     __FUNCTION__, mbox->name, mbox->pipe_rd, strerror(errno));
            }
        }

      mbox->recvcount++;
      res = 0;
    }
  else
    {
      if (errno != ETIMEDOUT)
        {
          syslog(LOG_ERR, "! %s: mq_receive() failed, '%s'\n",
                 __FUNCTION__, strerror(errno));
        }
    }

  return res;
}

int mailbox_fd(struct mailbox *mbox)
{
  if (mbox->use_poll == 1)
    {
      if (mbox->pipe_rd == 0)
        {
          mbox->pipe_rd = open(mbox->fifoname, O_RDWR);
        }
    }

  syslog(LOG_INFO, "%s: mbox(%s) fd = %d\n",
         __FUNCTION__, mbox->name, mbox->pipe_rd);

  return mbox->pipe_rd;
}

void mailbox_info(struct mailbox *mbox)
{
  sched_lock();

  printf("=== mailbox info ===\n");
  printf(" name: '%s'\n", mbox->name);
  printf(" size: %d [bytes]\n", mbox->size);
  printf(" depth: %d\n", mbox->depth);
  printf(" use_poll: %d (fd: %d)\n", mbox->use_poll, mbox->pipe_rd);
  printf(" --- statistics ---\n");
  printf("  send count:  %d\n", mbox->sendcount);
  printf("  recv count:  %d\n", mbox->recvcount);
  printf("  error count: %d\n", mbox->errcount);

  sched_unlock();
}

void show_mailbox_stat(void)
{
  struct mailbox *mbox;
  struct list_head *list, *list_tmp;

  list_for_each_safe(list, list_tmp, &mailbox_list)
    {
      mbox = list_entry(list, struct mailbox, list);
      mailbox_info(mbox);
    }
}
