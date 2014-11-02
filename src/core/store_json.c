/*
 * SysDB - src/core/store_json.c
 * Copyright (C) 2013-2014 Sebastian 'tokkee' Harl <sh@tokkee.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * This module implements JSON support.
 */

#if HAVE_CONFIG_H
#	include "config.h"
#endif /* HAVE_CONFIG_H */

#include "sysdb.h"
#include "core/store-private.h"
#include "utils/error.h"

#include <assert.h>

#include <stdlib.h>

/*
 * private data types
 */

struct sdb_store_json_formatter {
	sdb_strbuf_t *buf;

	/* The context describes the state of the formatter through
	 * the path pointing to the current object */
	int context[8];
	size_t current;

	int flags;
};

/*
 * private helper functions
 */

static int
json_emit(sdb_store_json_formatter_t *f, sdb_store_obj_t *obj)
{
	char time_str[64];
	char interval_str[64];
	size_t i;

	assert(f && obj);

	sdb_strbuf_append(f->buf, "{\"name\": \"%s\", ", SDB_OBJ(obj)->name);
	if (obj->type == SDB_ATTRIBUTE) {
		char tmp[sdb_data_strlen(&ATTR(obj)->value) + 1];
		if (sdb_data_format(&ATTR(obj)->value, tmp, sizeof(tmp),
					SDB_DOUBLE_QUOTED) < 0)
			snprintf(tmp, sizeof(tmp), "<error>");
		sdb_strbuf_append(f->buf, "\"value\": %s, ", tmp);
	}

	/* TODO: make time and interval formats configurable */
	if (! sdb_strftime(time_str, sizeof(time_str),
				"%F %T %z", obj->last_update))
		snprintf(time_str, sizeof(time_str), "<error>");
	time_str[sizeof(time_str) - 1] = '\0';

	if (! sdb_strfinterval(interval_str, sizeof(interval_str),
				obj->interval))
		snprintf(interval_str, sizeof(interval_str), "<error>");
	interval_str[sizeof(interval_str) - 1] = '\0';

	sdb_strbuf_append(f->buf, "\"last_update\": \"%s\", "
			"\"update_interval\": \"%s\", \"backends\": [",
			time_str, interval_str);

	for (i = 0; i < obj->backends_num; ++i) {
		sdb_strbuf_append(f->buf, "\"%s\"", obj->backends[i]);
		if (i < obj->backends_num - 1)
			sdb_strbuf_append(f->buf, ",");
	}
	sdb_strbuf_append(f->buf, "]");
	return 0;
} /* json_emit */

/*
 * public API
 */

sdb_store_json_formatter_t *
sdb_store_json_formatter(sdb_strbuf_t *buf, int flags)
{
	sdb_store_json_formatter_t *f;

	if (! buf)
		return NULL;

	f = calloc(1, sizeof(*f));
	if (! f)
		return NULL;

	f->buf = buf;
	f->context[0] = 0;
	f->current = 0;
	f->flags = flags;
	return f;
} /* sdb_store_json_formatter */

int
sdb_store_json_emit(sdb_store_json_formatter_t *f, sdb_store_obj_t *obj)
{
	if ((! f) || (! obj))
		return -1;

	/* first host */
	if (! f->context[0]) {
		if (f->flags & SDB_WANT_ARRAY)
			sdb_strbuf_append(f->buf, "[");
		assert(f->current == 0);
		f->context[0] = SDB_HOST;
		return json_emit(f, obj);
	}

	if (obj->type == f->context[f->current]) {
		/* new entry of the same type */
		sdb_strbuf_append(f->buf, "},");
	}
	else if ((f->context[f->current] == SDB_HOST)
			|| (obj->type == SDB_ATTRIBUTE)) {
		assert(obj->type != SDB_HOST);
		/* all object types may be children of a host;
		 * attributes may be children of any type */
		sdb_strbuf_append(f->buf, ", \"%ss\": [",
				SDB_STORE_TYPE_TO_NAME(obj->type));
		++f->current;
	}
	else if (f->current >= 1) {
		/* new entry of a previous type or a new type on the same level
		 * -> rewind to the right state and then handle the new object */
		assert(obj->type != SDB_ATTRIBUTE);
		while (f->current > 0) {
			if (f->context[f->current] == obj->type)
				break;
			assert(f->context[f->current] != SDB_HOST);
			sdb_strbuf_append(f->buf, "}]");
			--f->current;
		}
		return sdb_store_json_emit(f, obj);
	}
	else {
		sdb_log(SDB_LOG_ERR, "store: Unexpected object of type %s "
				"on level %zu during JSON serialization",
				SDB_STORE_TYPE_TO_NAME(obj->type), f->current);
		return -1;
	}

	json_emit(f, obj);
	assert(f->current < SDB_STATIC_ARRAY_LEN(f->context));
	f->context[f->current] = obj->type;
	return 0;
} /* sdb_store_json_emit */

int
sdb_store_json_emit_full(sdb_store_json_formatter_t *f, sdb_store_obj_t *obj,
		sdb_store_matcher_t *filter)
{
	sdb_avltree_t *trees[] = { NULL, NULL, NULL };
	size_t i;

	if (sdb_store_json_emit(f, obj))
		return -1;

	if (obj->type == SDB_HOST) {
		trees[0] = HOST(obj)->attributes;
		trees[1] = HOST(obj)->metrics;
		trees[2] = HOST(obj)->services;
	}
	else if (obj->type == SDB_SERVICE)
		trees[0] = SVC(obj)->attributes;
	else if (obj->type == SDB_METRIC)
		trees[0] = METRIC(obj)->attributes;
	else if (obj->type == SDB_ATTRIBUTE)
		return 0;
	else
		return -1;

	for (i = 0; i < SDB_STATIC_ARRAY_LEN(trees); ++i) {
		sdb_avltree_iter_t *iter;

		if (! trees[i])
			continue;

		iter = sdb_avltree_get_iter(trees[i]);
		while (sdb_avltree_iter_has_next(iter)) {
			sdb_store_obj_t *child;
			child = STORE_OBJ(sdb_avltree_iter_get_next(iter));

			if (filter && (! sdb_store_matcher_matches(filter, child, NULL)))
				continue;

			if (sdb_store_json_emit_full(f, child, filter)) {
				sdb_avltree_iter_destroy(iter);
				return -1;
			}
		}
		sdb_avltree_iter_destroy(iter);
	}
	return 0;
} /* sdb_store_json_emit_full */

int
sdb_store_json_finish(sdb_store_json_formatter_t *f)
{
	if (! f)
		return -1;

	if (! f->context[0]) {
		/* no content */
		if (f->flags & SDB_WANT_ARRAY)
			sdb_strbuf_append(f->buf, "[]");
		return 0;
	}

	while (f->current > 0) {
		sdb_strbuf_append(f->buf, "}]");
		--f->current;
	}

	sdb_strbuf_append(f->buf, "}");
	if (f->flags & SDB_WANT_ARRAY)
		sdb_strbuf_append(f->buf, "]");
	return 0;
} /* sdb_store_json_finish */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

