/****************************************************************************
 * Memory pool library
 *
 *  Copyright (C) 2012,2018-2019 Century Systems
 *  Author: Takeyoshi Kikuchi <kikuchi@centurysys.co.jp>
 *
 *  Last Modified: 2020/04/12 17:00:47 kikuchi
 ****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "xg50/list.h"
#include "xg50/mempool.h"

static LIST_HEAD(mempool_list);

struct mempool_info {
  struct mempool *pool;
  struct list_head list;
  char name[NAME_LEN_MAX];
};

struct mem_chunk {
  struct list_head list;
  struct mempool *pool;
  char mem[0];
};

static inline unsigned long *deadbeef_ptr(struct mem_chunk *chunk)
{
  struct mempool *pool = chunk->pool;

  return ((unsigned long *) ((char *) chunk->mem + pool->size));
}

static int check_ptr(struct mem_chunk *chunk)
{
  struct mempool_info *info;
  struct list_head *list, *list_tmp;
  struct mempool *pool = chunk->pool;
  int found = 0;

  list_for_each_safe(list, list_tmp, &mempool_list)
    {
      info = list_entry(list, struct mempool_info, list);

      if (info->pool == pool)
        {
          found = 1;
          break;
        }
    }

  if (found == 0)
    {
      syslog(LOG_ERR, "!!! %s: chunk (0x%p) is broken!\n",
             __FUNCTION__, chunk);
      return -1;
    }

  if (*deadbeef_ptr(chunk) != 0xdeadbeef)
    {
      syslog(LOG_ERR, "!!! %s: chunk (0x%p) is overwritten!\n",
             __FUNCTION__, chunk);
      return -1;
    }

  return 0;
}

struct mempool *mempool_create(const char *name, unsigned int size, unsigned int count)
{
  int i;
  struct mempool *pool;
  struct mempool_info *info;
  struct mem_chunk *chunk;
  void *mem;
  unsigned int aligned_size;

  aligned_size = ALIGN(sizeof(struct mem_chunk) + size + 4, 4);

  if (!(pool = zalloc(sizeof(struct mempool))))
    {
      goto err1;
    }

  pool->size = size;
  pool->n = count;
  INIT_LIST_HEAD(&pool->free);
  INIT_LIST_HEAD(&pool->used);

  if (!(mem = zalloc(aligned_size * count)))
    {
      goto err2;
    }

  pool->mem = mem;
  pool->size = size;

  for (i = 0; i < count; i++)
    {
      chunk = (struct mem_chunk *) ((char *) mem + aligned_size * i);
      chunk->pool = pool;
      INIT_LIST_HEAD(&chunk->list);
      list_add_tail(&chunk->list, &pool->free);

      *((unsigned long *) (chunk->mem + size)) = 0xdeadbeef;
    }

  if (!(info = malloc(sizeof(struct mempool_info))))
    {
      goto err3;
    }

  memset(info, 0, sizeof(struct mempool_info));
  info->pool = pool;
  pool->info = info;

  INIT_LIST_HEAD(&info->list);
  strncpy(info->name, name, strlen(name));

  list_add_tail(&info->list, &mempool_list);

  return pool;

err3:
  free(mem);
err2:
  free(pool);
err1:
  return NULL;
}

int __mempool_destroy(struct mempool *pool, int force)
{
  struct mempool_info *info;

  sched_lock();

  if (!force && !list_empty(&pool->used))
    {
      sched_unlock();
      errno = EBUSY;
      return -1;
    }

  info = pool->info;
  list_del(&info->list);

  free(pool->mem);
  free(pool);
  free(info);

  sched_unlock();

  return 0;
}

int mempool_destroy(struct mempool *pool)
{
  return __mempool_destroy(pool, 0);
}

void mempool_clean(struct mempool *pool)
{
  __mempool_destroy(pool, 1);
}

void mempool_clean_all(void)
{
  struct mempool_info *info;
  struct list_head *list, *list_tmp;

  sched_lock();

  list_for_each_safe(list, list_tmp, &mempool_list)
    {
      info = list_entry(list, struct mempool_info, list);
      __mempool_destroy(info->pool, 1);
    }

  sched_unlock();
}

struct mempool *get_mempool_by_name(const char *name)
{
  struct mempool_info *info;
  struct mempool *pool = NULL;
  struct list_head *list, *list_tmp;
  int len;

  len = strlen(name);

  list_for_each_safe(list, list_tmp, &mempool_list)
    {
      info = list_entry(list, struct mempool_info, list);

      if (strncmp(info->name, name, len) == 0) {
        pool = info->pool;
        break;
      }
    }

  return pool;
}

void mempool_info(struct mempool *pool)
{
  struct list_head *list, *list_tmp;
  void *ptr;
  unsigned long *ptr_deadbeef;
  int n_free, n_used;

  n_free = n_used = 0;

  sched_lock();

  printf("=== mempool info ===\n");
  printf(" name: '%s'\n", pool->info->name);
  printf(" chunk size:  %u [bytes]\n", pool->size);
  printf(" chunk count: %u\n", pool->n);

  printf("  --- free ---\n");

  list_for_each_safe(list, list_tmp, &pool->free)
    {
      ptr = (void *) list_entry(list, struct mem_chunk, list);
      ptr_deadbeef = deadbeef_ptr(ptr);

      printf(" %d: %p (0x%p -> 0x%08x)\n", n_free++, ptr,
           ptr_deadbeef, *ptr_deadbeef);
    }

  printf("  --- used ---\n");

  list_for_each_safe(list, list_tmp, &pool->used)
    {
      ptr = (void *) list_entry(list, struct mem_chunk, list);
      ptr_deadbeef = deadbeef_ptr(ptr);

      printf(" %d: %p (0x%p -> 0x%08x)\n", n_used++, ptr,
           ptr_deadbeef, *ptr_deadbeef);
    }

  sched_unlock();
}

void show_mempool_stat(void)
{
  struct mempool_info *info;
  struct mempool *pool;
  struct list_head *list, *list_tmp;

  sched_lock();

  list_for_each_safe(list, list_tmp, &mempool_list)
    {
      info = list_entry(list, struct mempool_info, list);
      pool = info->pool;

      mempool_info(pool);
    }

  sched_unlock();
}

static struct mem_chunk *__mempool_alloc(struct mempool *pool)
{
  struct mem_chunk *chunk;
  struct list_head *list;

  if (list_empty(&pool->free))
    {
      return NULL;
    }

  sched_lock();

  chunk = list_first_entry(&pool->free, struct mem_chunk, list);
  list = &chunk->list;
  list_del(list);
  list_add_tail(list, &pool->used);

  sched_unlock();

  return chunk;
}

static struct mem_chunk *__mempool_zalloc(struct mempool *pool)
{
  struct mem_chunk *chunk;

  if (!(chunk = __mempool_alloc(pool)))
    {
      return NULL;
    }

  memset(chunk->mem, 0, pool->size);

  return chunk;
}

void *mempool_alloc(struct mempool *pool)
{
  struct mem_chunk *chunk;

  if (!(chunk = __mempool_alloc(pool)))
    {
      return NULL;
    }

  return chunk->mem;
}

void *mempool_zalloc(struct mempool *pool)
{
  struct mem_chunk *chunk;

  if (!(chunk = __mempool_zalloc(pool)))
    {
      return NULL;
    }

  return chunk->mem;
}

static void __mempool_free(struct mem_chunk *chunk)
{
  struct mempool *pool;
  struct list_head *list;

  sched_lock();

  if (check_ptr(chunk) == 0)
    {
      pool = chunk->pool;

      list = &chunk->list;
      list_del(list);
      list_add_tail(list, &pool->free);
    }

  sched_unlock();
}

void mempool_free(void *ptr)
{
  struct mem_chunk *chunk;

  chunk = container_of(ptr, struct mem_chunk, mem);
  __mempool_free(chunk);
}
