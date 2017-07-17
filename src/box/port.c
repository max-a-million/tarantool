/*
 * Copyright 2010-2016, Tarantool AUTHORS, please see AUTHORS file.
 *
 * Redistribution and use in source and binary forms, with or
 * without modification, are permitted provided that the following
 * conditions are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the
 *    following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY <COPYRIGHT HOLDER> ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * <COPYRIGHT HOLDER> OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#include "port.h"
#include "tuple.h"
#include <small/obuf.h>
#include <small/slab_cache.h>
#include <small/mempool.h>
#include <fiber.h>
#include "errinj.h"

static struct mempool port_entry_pool;

int
port_add_tuple(struct port *port, struct tuple *tuple)
{
	struct port_entry *e;
	if (port->size == 0) {
		if (tuple_ref(tuple) != 0)
			return -1;
		e = &port->first_entry;
		port->first = port->last = e;
	} else {
		e = (struct port_entry *)
			mempool_alloc(&port_entry_pool);
		if (e == NULL) {
			diag_set(OutOfMemory, sizeof(*e), "mempool_alloc", "e");
			return -1;
		}
		if (tuple_ref(tuple) != 0) {
			mempool_free(&port_entry_pool, e);
			return -1;
		}
		port->last->next = e;
		port->last = e;
	}
	e->tuple = tuple;
	e->next = NULL;
	++port->size;
	return 0;
}

void
port_create(struct port *port)
{
	port->size = 0;
	port->first = NULL;
	port->last = NULL;
}

void
port_destroy(struct port *port)
{
	struct port_entry *e = port->first;
	if (e == NULL)
		return;
	tuple_unref(e->tuple);
	e = e->next;
	while (e != NULL) {
		struct port_entry *cur = e;
		e = e->next;
		tuple_unref(cur->tuple);
		mempool_free(&port_entry_pool, cur);
	}
}

int
port_dump(struct port *port, struct obuf *out)
{
	struct port_entry *pe = port->first;
	struct port_entry *tmp;
	if (pe == NULL)
		return 0;
	if (tuple_to_obuf(pe->tuple, out) != 0) {
		/*
		 * The first port entry is pre-allocated,
		 * no need to destroy it, only unref
		 * the tuple.
		 */
		tuple_unref(pe->tuple);
		/*
		 * In case of error the dump must destroy the
		 * port.
		 */
		pe = pe->next;
		if (pe != NULL)
			goto error;
		return -1;
	}
	tuple_unref(pe->tuple);
	pe = pe->next;

	while (pe != NULL) {
		if (tuple_to_obuf(pe->tuple, out) != 0) {
			/*
			 * After the first error we can only
			 * destroy the port.
			 */
			goto error;
		}

		ERROR_INJECT(ERRINJ_PORT_DUMP, {
			diag_set(OutOfMemory, tuple_size(pe->tuple), "obuf_dup",
				 "data");
			goto error;
		});
		tmp = pe->next;
		tuple_unref(pe->tuple);
		mempool_free(&port_entry_pool, pe);
		pe = tmp;
	}
	return 0;

	/* Free the port */
	while (pe != NULL) {
error:
		tmp = pe->next;
		tuple_unref(pe->tuple);
		mempool_free(&port_entry_pool, pe);
		pe = tmp;
	}
	return -1;
}

void
port_init(void)
{
	mempool_create(&port_entry_pool, &cord()->slabc,
		       sizeof(struct port_entry));
}

void
port_free(void)
{
	mempool_destroy(&port_entry_pool);
}
