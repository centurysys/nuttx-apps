#ifndef _MAILBOX_H
#define _MAILBOX_H

#include <mqueue.h>
#include <sys/types.h>
#include "list.h"

#define NAME_LEN_MAX 32

struct mailbox {
  struct list_head list;
  mqd_t mq;
  int pipe_rd;
  char name[NAME_LEN_MAX - 10];
  char fifoname[NAME_LEN_MAX];  /* "/tmp/FIFO_..." */

  size_t size;
  size_t depth;
  int use_poll;

  /* statistics */
  int sendcount;
  int recvcount;
  int errcount;
};

struct mailbox *mailbox_create(const char *name, size_t size, size_t depth, int use_poll);
struct mailbox *mailbox_open(const char *name);
int mailbox_destroy(struct mailbox *mbox);

int mailbox_send(struct mailbox *mbox, void *data, unsigned long timeout);
int mailbox_recv(struct mailbox *mbox, void *data, unsigned long timeout);
int mailbox_fd(struct mailbox *mbox);

void mailbox_info(struct mailbox *mbox);
void show_mailbox_stat(void);

#endif /* _MAILBOX_H */
