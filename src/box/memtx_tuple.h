#ifndef INCLUDES_TARANTOOL_BOX_MEMTX_TUPLE_H
#define INCLUDES_TARANTOOL_BOX_MEMTX_TUPLE_H
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
 * THIS SOFTWARE IS PROVIDED BY AUTHORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * AUTHORS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "diag.h"
#include "tuple_format.h"
#include "tuple.h"

#if defined(__cplusplus)
extern "C" {
#endif /* defined(__cplusplus) */

/**
 * Initialize memtx_tuple library
 */
void
memtx_tuple_init(uint64_t tuple_arena_max_size, uint32_t objsize_min,
		 uint32_t objsize_max, float alloc_factor);

/**
 * Cleanup memtx_tuple library
 */
void
memtx_tuple_free(void);

/** Create a tuple in the memtx engine format. @sa tuple_new(). */
struct tuple *
memtx_tuple_new(struct tuple_format *format, const char *data, const char *end);

/**
 * Free the tuple of a memtx space.
 * @pre tuple->refs  == 0
 */
void
memtx_tuple_delete(struct tuple_format *format, struct tuple *tuple);

/** tuple format vtab for memtx engine. */
extern struct tuple_format_vtab memtx_tuple_format_vtab;

void
memtx_tuple_begin_snapshot();

void
memtx_tuple_end_snapshot();

/** \cond public */

/**
 * Allocate and initialize a new tuple from a raw MsgPack Array data.
 *
 * \param format tuple format.
 * Use box_tuple_format_default() to create space-independent tuple.
 * \param data tuple data in MsgPack Array format ([field1, field2, ...]).
 * \param end the end of \a data
 * \retval NULL on out of memory
 * \retval tuple otherwise
 * \pre data, end is valid MsgPack Array
 * \sa \code box.tuple.new(data) \endcode
 */
box_tuple_t *
box_tuple_new(box_tuple_format_t *format, const char *data, const char *end);

box_tuple_t *
box_tuple_update(const box_tuple_t *tuple, const char *expr, const
		 char *expr_end);

box_tuple_t *
box_tuple_upsert(const box_tuple_t *tuple, const char *expr, const
		 char *expr_end);

/** \endcond public */

#if defined(__cplusplus)
}

/**
 * Create a tuple in the memtx engine format. Throw an exception
 * if an error occured. @sa memtx_tuple_new().
 */
static inline struct tuple *
memtx_tuple_new_xc(struct tuple_format *format, const char *data,
		   const char *end)
{
	struct tuple *res = memtx_tuple_new(format, data, end);
	if (res == NULL)
		diag_raise();
	return res;
}

#endif /* defined(__cplusplus) */

#endif
