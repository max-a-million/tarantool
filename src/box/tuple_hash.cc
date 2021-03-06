/*
 * Copyright 2010-2017, Tarantool AUTHORS, please see AUTHORS file.
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

#include "tuple_hash.h"

#include "third_party/PMurHash.h"

/* Tuple and key hasher */
namespace {

enum {
	HASH_SEED = 13U
};

template <int TYPE>
static inline uint32_t
field_hash(uint32_t *ph, uint32_t *pcarry, const char **field)
{
	/*
	* (!) All fields, except TYPE_STRING hashed **including** MsgPack format
	* identifier (e.g. 0xcc). This was done **intentionally**
	* for performance reasons. Please follow MsgPack specification
	* and pack all your numbers to the most compact representation.
	* If you still want to add support for broken MsgPack,
	* please don't forget to patch tuple_compare_field().
	*/
	const char *f = *field;
	uint32_t size;
	mp_next(field);
	size = *field - f;  /* calculate the size of field */
	assert(size < INT32_MAX);
	PMurHash32_Process(ph, pcarry, f, size);
	return size;
}

template <>
inline uint32_t
field_hash<FIELD_TYPE_STRING>(uint32_t *ph, uint32_t *pcarry,
			      const char **pfield)
{
	/*
	* (!) MP_STR fields hashed **excluding** MsgPack format
	* indentifier. We have to do that to keep compatibility
	* with old third-party MsgPack (spec-old.md) implementations.
	* \sa https://github.com/tarantool/tarantool/issues/522
	*/
	uint32_t size;
	const char *f = mp_decode_str(pfield, &size);
	assert(size < INT32_MAX);
	PMurHash32_Process(ph, pcarry, f, size);
	return size;
}

template <int TYPE, int ...MORE_TYPES> struct KeyFieldHash {};

template <int TYPE, int TYPE2, int ...MORE_TYPES>
struct KeyFieldHash<TYPE, TYPE2, MORE_TYPES...> {
	static void hash(uint32_t *ph, uint32_t *pcarry,
			 const char **pfield, uint32_t *ptotal_size)
	{
		*ptotal_size += field_hash<TYPE>(ph, pcarry, pfield);
		KeyFieldHash<TYPE2, MORE_TYPES...>::
			hash(ph, pcarry, pfield, ptotal_size);
	}
};

template <int TYPE>
struct KeyFieldHash<TYPE> {
	static void hash(uint32_t *ph, uint32_t *pcarry,
			 const char **pfield, uint32_t *ptotal_size)
	{
		*ptotal_size += field_hash<TYPE>(ph, pcarry, pfield);
	}
};

template <int TYPE, int ...MORE_TYPES>
struct KeyHash {
	static uint32_t hash(const char *key, const struct key_def *)
	{
		uint32_t h = HASH_SEED;
		uint32_t carry = 0;
		uint32_t total_size = 0;
		KeyFieldHash<TYPE, MORE_TYPES...>::hash(&h, &carry, &key,
							&total_size);
		return PMurHash32_Result(h, carry, total_size);
	}
};

template <>
struct KeyHash<FIELD_TYPE_UNSIGNED> {
	static uint32_t hash(const char *key, const struct key_def *key_def)
	{
		uint64_t val = mp_decode_uint(&key);
		(void) key_def;
		if (likely(val <= UINT32_MAX))
			return val;
		return ((uint32_t)((val)>>33^(val)^(val)<<11));
	}
};

template <int TYPE, int ...MORE_TYPES> struct TupleFieldHash { };

template <int TYPE, int TYPE2, int ...MORE_TYPES>
struct TupleFieldHash<TYPE, TYPE2, MORE_TYPES...> {
	static void hash(const char **pfield, uint32_t *ph, uint32_t *pcarry,
			 uint32_t *ptotal_size)
	{
		*ptotal_size += field_hash<TYPE>(ph, pcarry, pfield);
		TupleFieldHash<TYPE2, MORE_TYPES...>::
			hash(pfield, ph, pcarry, ptotal_size);
	}
};

template <int TYPE>
struct TupleFieldHash<TYPE> {
	static void hash(const char **pfield, uint32_t *ph, uint32_t *pcarry,
			 uint32_t *ptotal_size)
	{
		*ptotal_size += field_hash<TYPE>(ph, pcarry, pfield);
	}
};

template <int TYPE, int ...MORE_TYPES>
struct TupleHash
{
	static uint32_t hash(const struct tuple *tuple,
			     const struct key_def *key_def)
	{
		uint32_t h = HASH_SEED;
		uint32_t carry = 0;
		uint32_t total_size = 0;
		const char *field = tuple_field(tuple, key_def->parts->fieldno);
		TupleFieldHash<TYPE, MORE_TYPES...>::
			hash(&field, &h, &carry, &total_size);
		return PMurHash32_Result(h, carry, total_size);
	}
};

template <>
struct TupleHash<FIELD_TYPE_UNSIGNED> {
	static uint32_t	hash(const struct tuple *tuple,
			     const struct key_def *key_def)
	{
		const char *field = tuple_field(tuple, key_def->parts->fieldno);
		uint64_t val = mp_decode_uint(&field);
		if (likely(val <= UINT32_MAX))
			return val;
		return ((uint32_t)((val)>>33^(val)^(val)<<11));
	}
};

}; /* namespace { */

#define HASHER(...) \
	{ KeyHash<__VA_ARGS__>::hash, TupleHash<__VA_ARGS__>::hash, \
		{ __VA_ARGS__, UINT32_MAX } },

struct hasher_signature {
	key_hash_t kf;
	tuple_hash_t tf;
	uint32_t p[64];
};

/**
 * field1 type,  field2 type, ...
 */
static const hasher_signature hash_arr[] = {
	HASHER(FIELD_TYPE_UNSIGNED)
	HASHER(FIELD_TYPE_STRING)
	HASHER(FIELD_TYPE_UNSIGNED, FIELD_TYPE_UNSIGNED)
	HASHER(FIELD_TYPE_STRING  , FIELD_TYPE_UNSIGNED)
	HASHER(FIELD_TYPE_UNSIGNED, FIELD_TYPE_STRING)
	HASHER(FIELD_TYPE_STRING  , FIELD_TYPE_STRING)
	HASHER(FIELD_TYPE_UNSIGNED, FIELD_TYPE_UNSIGNED, FIELD_TYPE_UNSIGNED)
	HASHER(FIELD_TYPE_STRING  , FIELD_TYPE_UNSIGNED, FIELD_TYPE_UNSIGNED)
	HASHER(FIELD_TYPE_UNSIGNED, FIELD_TYPE_STRING  , FIELD_TYPE_UNSIGNED)
	HASHER(FIELD_TYPE_STRING  , FIELD_TYPE_STRING  , FIELD_TYPE_UNSIGNED)
	HASHER(FIELD_TYPE_UNSIGNED, FIELD_TYPE_UNSIGNED, FIELD_TYPE_STRING)
	HASHER(FIELD_TYPE_STRING  , FIELD_TYPE_UNSIGNED, FIELD_TYPE_STRING)
	HASHER(FIELD_TYPE_UNSIGNED, FIELD_TYPE_STRING  , FIELD_TYPE_STRING)
	HASHER(FIELD_TYPE_STRING  , FIELD_TYPE_STRING  , FIELD_TYPE_STRING)
};

#undef HASHER

uint32_t
tuple_hash_slowpath(const struct tuple *tuple, const struct key_def *key_def);

uint32_t
key_hash_slowpath(const char *key, const struct key_def *key_def);

void
tuple_hash_func_set(struct key_def *key_def) {
	/*
	 * Check that key_def defines sequential a key without holes
	 * starting from **arbitrary** field.
	 */
	for (uint32_t i = 1; i < key_def->part_count; i++) {
		if (key_def->parts[i - 1].fieldno + 1 !=
		    key_def->parts[i].fieldno)
			goto slowpath;
	}
	/*
	 * Try to find pre-generated tuple_hash() and key_hash()
	 * implementations
	 */
	for (uint32_t k = 0; k < sizeof(hash_arr) / sizeof(hash_arr[0]); k++) {
		uint32_t i = 0;
		for (; i < key_def->part_count; i++) {
			if (key_def->parts[i].type != hash_arr[k].p[i]) {
				break;
			}
		}
		if (i == key_def->part_count && hash_arr[k].p[i] == UINT32_MAX){
			key_def->tuple_hash = hash_arr[k].tf;
			key_def->key_hash = hash_arr[k].kf;
			return;
		}
	}

slowpath:
	key_def->tuple_hash = tuple_hash_slowpath;
	key_def->key_hash = key_hash_slowpath;
}

static uint32_t
tuple_hash_field(uint32_t *ph1, uint32_t *pcarry, const char **field,
		enum field_type type)
{
	const char *f = *field;
	uint32_t size;

	switch (type) {
	case FIELD_TYPE_STRING:
		/*
		 * (!) MP_STR fields hashed **excluding** MsgPack format
		 * indentifier. We have to do that to keep compatibility
		 * with old third-party MsgPack (spec-old.md) implementations.
		 * \sa https://github.com/tarantool/tarantool/issues/522
		 */
		f = mp_decode_str(field, &size);
		break;
	default:
		mp_next(field);
		size = *field - f;  /* calculate the size of field */
		/*
		 * (!) All other fields hashed **including** MsgPack format
		 * identifier (e.g. 0xcc). This was done **intentionally**
		 * for performance reasons. Please follow MsgPack specification
		 * and pack all your numbers to the most compact representation.
		 * If you still want to add support for broken MsgPack,
		 * please don't forget to patch tuple_compare_field().
		 */
		break;
	}
	assert(size < INT32_MAX);
	PMurHash32_Process(ph1, pcarry, f, size);
	return size;
}


uint32_t
tuple_hash_slowpath(const struct tuple *tuple, const struct key_def *key_def)
{
	uint32_t h = HASH_SEED;
	uint32_t carry = 0;
	uint32_t total_size = 0;
	uint32_t prev_fieldno = key_def->parts[0].fieldno;
	const char* field = tuple_field(tuple, key_def->parts[0].fieldno);
	total_size += tuple_hash_field(&h, &carry, &field,
		key_def->parts[0].type);
	for (uint32_t part_id = 1; part_id < key_def->part_count; part_id++) {
		/* If parts of key_def are not sequential we need to call
		 * tuple_field. Otherwise, tuple is hashed sequentially without
		 * need of tuple_field
		 */
		if (prev_fieldno + 1 != key_def->parts[part_id].fieldno) {
			field = tuple_field(tuple, key_def->parts[part_id].fieldno);
		}
		total_size += tuple_hash_field(&h, &carry, &field,
					key_def->parts[part_id].type);
		prev_fieldno = key_def->parts[part_id].fieldno;
	}

	return PMurHash32_Result(h, carry, total_size);
}

uint32_t
key_hash_slowpath(const char *key, const struct key_def *key_def)
{
	uint32_t h = HASH_SEED;
	uint32_t carry = 0;
	uint32_t total_size = 0;

	for (const struct key_part *part = key_def->parts;
	     part < key_def->parts + key_def->part_count; part++) {
		total_size += tuple_hash_field(&h, &carry, &key, part->type);
	}

	return PMurHash32_Result(h, carry, total_size);
}
