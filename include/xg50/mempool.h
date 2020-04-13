#ifndef _MEMPOOL_H
#define _MEMPOOL_H

#include "list.h"

#define NAME_LEN_MAX 32

struct mempool {
  void *mem;
  unsigned int size;
  unsigned int n;

  struct list_head free;
  struct list_head used;

  struct mempool_info *info;
};

struct mempool *mempool_create(const char *name, unsigned int size, unsigned int count);
struct mempool *get_mempool_by_name(const char *name);
int mempool_destroy(struct mempool *pool);
void mempool_clean(struct mempool *pool);
void mempool_clean_all(void);

void mempool_info(struct mempool *pool);
void show_mempool_stat(void);

void *mempool_alloc(struct mempool *pool);
void *mempool_zalloc(struct mempool *pool);
void mempool_free(void *ptr);


#endif /* _MEMPOOL_H */
