/*
 * Memory management functions.
 *
 * Copyright 2000-2007 Willy Tarreau <w@1wt.eu>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 *
 */

#include <common/config.h>
#include <common/memory.h>
#include <common/mini-clist.h>
#include <common/standard.h>

#include <proto/log.h>

static struct list pools = LIST_HEAD_INIT(pools);

/* Try to find an existing shared pool with the same characteristics and
 * returns it, otherwise creates this one. NULL is returned if no memory
 * is available for a new creation.
 */
struct pool_head *create_pool(char *name, unsigned int size, unsigned int flags)
{
	struct pool_head *pool;
	unsigned int align;

	/* We need to store at least a (void *) in the chunks. Since we know
	 * that the malloc() function will never return such a small size,
	 * let's round the size up to something slightly bigger, in order to
	 * ease merging of entries. Note that the rounding is a power of two.
	 */

	align = 4 * sizeof(void *);
	size  = (size + align - 1) & -align;

	pool = NULL;
	if (flags & MEM_F_SHARED) {
		struct pool_head *entry;
		list_for_each_entry(entry, &pools, list) {
			if (!(entry->flags & MEM_F_SHARED))
				continue;
			if (entry->size == size) {
				pool = entry;
				break;
			}
		}
	}

	if (!pool) {
		pool = CALLOC(1, sizeof(*pool));
		if (!pool)
			return NULL;
		if (name)
			strlcpy2(pool->name, name, sizeof(pool->name));
		pool->size = size;
		pool->flags = flags;
		LIST_ADDQ(&pools, &pool->list);
	}
	return pool;
}

/* Allocate a new entry for pool <pool>, and return it for immediate use.
 * NULL is returned if no memory is available for a new creation.
 */
void *refill_pool_alloc(struct pool_head *pool)
{
	void *ret;

	if (pool->limit && (pool->allocated >= pool->limit))
		return NULL;
	ret = MALLOC(pool->size);
	if (!ret)
		return NULL;
	pool->allocated++;
	pool->used++;
	return ret;
}

/* Dump statistics on pools usage.
 */
void dump_pools(void)
{
	struct pool_head *entry;
	unsigned long allocated, used;
	int nbpools;

	allocated = used = nbpools = 0;
	qfprintf(stderr, "Dumping pools usage.\n");
	list_for_each_entry(entry, &pools, list) {
		qfprintf(stderr, "  - Pool %s (%d bytes) : %d allocated, %d used%s\n",
			 entry->name, entry->size, entry->allocated, entry->used,
			 (entry->flags & MEM_F_SHARED) ? " (SHARED)" : "");

		allocated += entry->allocated * entry->size;
		used += entry->used * entry->size;
		nbpools++;
	}
	qfprintf(stderr, "Total: %d pools, %lu allocated, %lu used.\n",
		 nbpools, allocated, used);
}

/*
 * Local variables:
 *  c-indent-level: 8
 *  c-basic-offset: 8
 * End:
 */
