/*
 * SysDB - t/unit/core/store_test.c
 * Copyright (C) 2013 Sebastian 'tokkee' Harl <sh@tokkee.org>
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

#if HAVE_CONFIG_H
#	include "config.h"
#endif

#include "core/plugin.h"
#include "core/store.h"
#include "core/memstore-private.h"
#include "testutils.h"

#include <check.h>
#include <string.h>
#include <strings.h>

static sdb_memstore_t *store;

static void
init(void)
{
	store = sdb_memstore_create();
	ck_assert(store != NULL);
}

static void
populate(void)
{
	sdb_data_t datum;

	sdb_memstore_host(store, "h1", 1, 0);
	sdb_memstore_host(store, "h2", 3, 0);

	datum.type = SDB_TYPE_STRING;
	datum.data.string = "v1";
	sdb_memstore_attribute(store, "h1", "k1", &datum, 1, 0);
	datum.data.string = "v2";
	sdb_memstore_attribute(store, "h1", "k2", &datum, 2, 0);
	datum.data.string = "v3";
	sdb_memstore_attribute(store, "h1", "k3", &datum, 2, 0);

	/* make sure that older updates don't overwrite existing values */
	datum.data.string = "fail";
	sdb_memstore_attribute(store, "h1", "k2", &datum, 1, 0);
	sdb_memstore_attribute(store, "h1", "k3", &datum, 2, 0);

	sdb_memstore_metric(store, "h1", "m1", /* store */ NULL, 2, 0);
	sdb_memstore_metric(store, "h1", "m2", /* store */ NULL, 1, 0);
	sdb_memstore_metric(store, "h2", "m1", /* store */ NULL, 1, 0);

	sdb_memstore_service(store, "h2", "s1", 1, 0);
	sdb_memstore_service(store, "h2", "s2", 2, 0);

	datum.type = SDB_TYPE_INTEGER;
	datum.data.integer = 42;
	sdb_memstore_metric_attr(store, "h1", "m1", "k3", &datum, 2, 0);

	datum.data.integer = 123;
	sdb_memstore_service_attr(store, "h2", "s2", "k1", &datum, 2, 0);
	datum.data.integer = 4711;
	sdb_memstore_service_attr(store, "h2", "s2", "k2", &datum, 1, 0);

	/* don't overwrite k1 */
	datum.data.integer = 666;
	sdb_memstore_service_attr(store, "h2", "s2", "k1", &datum, 2, 0);
} /* populate */

static void
turndown(void)
{
	sdb_object_deref(SDB_OBJ(store));
	store = NULL;
} /* turndown */

START_TEST(test_store_host)
{
	struct {
		const char *name;
		sdb_time_t  last_update;
		int         expected;
	} golden_data[] = {
		{ "a", 1, 0 },
		{ "a", 2, 0 },
		{ "b", 1, 0 },
	};

	struct {
		const char *name;
		bool        have;
	} golden_hosts[] = {
		{ "a", 1 },
		{ "b", 1 },
		{ "c", 0 },
		{ "A", 1 },
	};

	size_t i;

	for (i = 0; i < SDB_STATIC_ARRAY_LEN(golden_data); ++i) {
		int status;

		status = sdb_memstore_host(store, golden_data[i].name,
				golden_data[i].last_update, 0);
		fail_unless(status == golden_data[i].expected,
				"sdb_memstore_host(%s, %d, 0) = %d; expected: %d",
				golden_data[i].name, (int)golden_data[i].last_update,
				status, golden_data[i].expected);
	}

	for (i = 0; i < SDB_STATIC_ARRAY_LEN(golden_hosts); ++i) {
		sdb_memstore_obj_t *have;

		have = sdb_memstore_get_host(store, golden_hosts[i].name);
		fail_unless((have != NULL) == golden_hosts[i].have,
				"sdb_memstore_get_host(%s) = %p; expected: %s",
				golden_hosts[i].name, have,
				golden_hosts[i].have ? "<host>" : "NULL");
		sdb_object_deref(SDB_OBJ(have));
	}
}
END_TEST

START_TEST(test_store_get_host)
{
	char *golden_hosts[] = { "a", "b", "c" };
	char *unknown_hosts[] = { "x", "y", "z" };
	size_t i;

	for (i = 0; i < SDB_STATIC_ARRAY_LEN(golden_hosts); ++i) {
		int status = sdb_memstore_host(store, golden_hosts[i], 1, 0);
		fail_unless(status >= 0,
				"sdb_memstore_host(%s) = %d; expected: >=0",
				golden_hosts[i], status);
	}

	for (i = 0; i < SDB_STATIC_ARRAY_LEN(golden_hosts); ++i) {
		sdb_memstore_obj_t *sobj1, *sobj2;
		int ref_cnt;

		sobj1 = sdb_memstore_get_host(store, golden_hosts[i]);
		fail_unless(sobj1 != NULL,
				"sdb_memstore_get_host(%s) = NULL; expected: <host>",
				golden_hosts[i]);
		ref_cnt = SDB_OBJ(sobj1)->ref_cnt;

		fail_unless(ref_cnt > 1,
				"sdb_memstore_get_host(%s) did not increment ref count: "
				"got: %d; expected: >1", golden_hosts[i], ref_cnt);

		sobj2 = sdb_memstore_get_host(store, golden_hosts[i]);
		fail_unless(sobj2 != NULL,
				"sdb_memstore_get_host(%s) = NULL; expected: <host>",
				golden_hosts[i]);

		fail_unless(sobj1 == sobj2,
				"sdb_memstore_get_host(%s) returned different objects "
				"in successive calls", golden_hosts[i]);
		fail_unless(SDB_OBJ(sobj2)->ref_cnt == ref_cnt + 1,
				"sdb_memstore_get_hosts(%s) did not increment ref count "
				"(first call: %d; second call: %d)",
				golden_hosts[i], ref_cnt, SDB_OBJ(sobj2)->ref_cnt);

		sdb_object_deref(SDB_OBJ(sobj1));
		sdb_object_deref(SDB_OBJ(sobj2));
	}
	for (i = 0; i < SDB_STATIC_ARRAY_LEN(unknown_hosts); ++i) {
		sdb_memstore_obj_t *sobj;

		sobj = sdb_memstore_get_host(store, unknown_hosts[i]);
		fail_unless(!sobj, "sdb_memstore_get_host(%s) = <host:%s>; expected: NULL",
				unknown_hosts[i], sobj ? SDB_OBJ(sobj)->name : "NULL");
	}
}
END_TEST

START_TEST(test_store_attr)
{
	struct {
		const char *host;
		const char *key;
		char       *value;
		sdb_time_t  last_update;
		int         expected;
	} golden_data[] = {
		{ "k", "k",  "v",  1, -1 },
		{ "k", "k",  "v",  1, -1 }, /* retry to ensure the host is not created */
		{ "l", "k1", "v1", 1,  0 },
		{ "l", "k1", "v2", 2,  0 },
		{ "l", "k2", "v1", 1,  0 },
		{ "m", "k",  "v1", 1,  0 },
	};

	size_t i;

	sdb_memstore_host(store, "l", 1, 0);
	sdb_memstore_host(store, "m", 1, 0);
	for (i = 0; i < SDB_STATIC_ARRAY_LEN(golden_data); ++i) {
		sdb_data_t datum;
		int status;

		/* XXX: test other types as well */
		datum.type = SDB_TYPE_STRING;
		datum.data.string = golden_data[i].value;

		status = sdb_memstore_attribute(store, golden_data[i].host,
				golden_data[i].key, &datum,
				golden_data[i].last_update, 0);
		fail_unless(status == golden_data[i].expected,
				"sdb_memstore_attribute(%s, %s, %s, %d) = %d; expected: %d",
				golden_data[i].host, golden_data[i].key, golden_data[i].value,
				golden_data[i].last_update, status, golden_data[i].expected, 0);
	}
}
END_TEST

START_TEST(test_store_metric)
{
	sdb_metric_store_t store1 = { "dummy-type1", "dummy-id1", NULL, 0 };
	sdb_metric_store_t store2 = { "dummy-type2", "dummy-id2", NULL, 0 };

	struct {
		const char *host;
		const char *metric;
		sdb_metric_store_t *store;
		sdb_time_t  last_update;
		int         expected;
	} golden_data[] = {
		{ "k", "m",  NULL,    1, -1 },
		{ "k", "m",  NULL,    1, -1 }, /* retry to ensure the host is not created */
		{ "k", "m",  &store1, 1, -1 },
		{ "l", "m1", NULL,    1,  0 },
		{ "l", "m1", &store1, 2,  0 },
		{ "l", "m1", &store1, 3,  0 },
		{ "l", "m2", &store1, 1,  0 },
		{ "l", "m2", &store2, 2,  0 },
		{ "l", "m2", NULL,    3,  0 },
		{ "m", "m",  &store1, 1,  0 },
		{ "m", "m",  NULL,    2,  0 },
		{ "m", "m",  &store1, 3,  0 },
		{ "m", "m",  &store2, 4,  0 },
		{ "m", "m",  NULL,    5,  0 },
	};

	size_t i;

	sdb_memstore_host(store, "m", 1, 0);
	sdb_memstore_host(store, "l", 1, 0);
	for (i = 0; i < SDB_STATIC_ARRAY_LEN(golden_data); ++i) {
		int status;

		status = sdb_memstore_metric(store, golden_data[i].host,
				golden_data[i].metric, golden_data[i].store,
				golden_data[i].last_update, 0);
		fail_unless(status == golden_data[i].expected,
				"sdb_memstore_metric(%s, %s, %p, %d, 0) = %d; expected: %d",
				golden_data[i].host, golden_data[i].metric,
				golden_data[i].store, golden_data[i].last_update,
				status, golden_data[i].expected);
	}
}
END_TEST

START_TEST(test_store_metric_attr)
{
	struct {
		const char *host;
		const char *metric;
		const char *attr;
		const sdb_data_t value;
		sdb_time_t  last_update;
		int expected;
	} golden_data[] = {
		{ "k", "m1", "a1", { SDB_TYPE_INTEGER, { .integer = 123 } }, 1, -1 },
		/* retry, it should still fail */
		{ "k", "m1", "a1", { SDB_TYPE_INTEGER, { .integer = 123 } }, 1, -1 },
		{ "l", "mX", "a1", { SDB_TYPE_INTEGER, { .integer = 123 } }, 1, -1 },
		/* retry, it should still fail */
		{ "l", "mX", "a1", { SDB_TYPE_INTEGER, { .integer = 123 } }, 1, -1 },
		{ "l", "m1", "a1", { SDB_TYPE_INTEGER, { .integer = 123 } }, 1,  0 },
		{ "l", "m1", "a1", { SDB_TYPE_INTEGER, { .integer = 123 } }, 2,  0 },
		{ "l", "m1", "a2", { SDB_TYPE_INTEGER, { .integer = 123 } }, 1,  0 },
		{ "l", "m2", "a2", { SDB_TYPE_INTEGER, { .integer = 123 } }, 1,  0 },
		{ "m", "m1", "a1", { SDB_TYPE_INTEGER, { .integer = 123 } }, 1,  0 },
	};

	size_t i;

	sdb_memstore_host(store, "m", 1, 0);
	sdb_memstore_host(store, "l", 1, 0);
	sdb_memstore_metric(store, "m", "m1", NULL, 1, 0);
	sdb_memstore_metric(store, "l", "m1", NULL, 1, 0);
	sdb_memstore_metric(store, "l", "m2", NULL, 1, 0);

	for (i = 0; i < SDB_STATIC_ARRAY_LEN(golden_data); ++i) {
		int status;

		status = sdb_memstore_metric_attr(store, golden_data[i].host,
				golden_data[i].metric, golden_data[i].attr,
				&golden_data[i].value, golden_data[i].last_update, 0);
		fail_unless(status == golden_data[i].expected,
				"sdb_memstore_metric_attr(%s, %s, %s, %d, %d, 0) = %d; "
				"expected: %d", golden_data[i].host, golden_data[i].metric,
				golden_data[i].attr, golden_data[i].value.data.integer,
				golden_data[i].last_update, status, golden_data[i].expected);
	}
}
END_TEST

START_TEST(test_store_service)
{
	struct {
		const char *host;
		const char *svc;
		sdb_time_t  last_update;
		int         expected;
	} golden_data[] = {
		{ "k", "s",  1, -1 },
		{ "k", "s",  1, -1 }, /* retry to ensure the host is not created */
		{ "l", "s1", 1,  0 },
		{ "l", "s1", 2,  0 },
		{ "l", "s2", 1,  0 },
		{ "m", "s",  1,  0 },
	};

	size_t i;

	sdb_memstore_host(store, "m", 1, 0);
	sdb_memstore_host(store, "l", 1, 0);
	for (i = 0; i < SDB_STATIC_ARRAY_LEN(golden_data); ++i) {
		int status;

		status = sdb_memstore_service(store, golden_data[i].host,
				golden_data[i].svc, golden_data[i].last_update, 0);
		fail_unless(status == golden_data[i].expected,
				"sdb_memstore_service(%s, %s, %d, 0) = %d; expected: %d",
				golden_data[i].host, golden_data[i].svc,
				golden_data[i].last_update, status, golden_data[i].expected);
	}
}
END_TEST

START_TEST(test_store_service_attr)
{
	struct {
		const char *host;
		const char *svc;
		const char *attr;
		const sdb_data_t value;
		sdb_time_t  last_update;
		int expected;
	} golden_data[] = {
		{ "k", "s1", "a1", { SDB_TYPE_INTEGER, { .integer = 123 } }, 1, -1 },
		/* retry, it should still fail */
		{ "k", "s1", "a1", { SDB_TYPE_INTEGER, { .integer = 123 } }, 1, -1 },
		{ "l", "sX", "a1", { SDB_TYPE_INTEGER, { .integer = 123 } }, 1, -1 },
		/* retry, it should still fail */
		{ "l", "sX", "a1", { SDB_TYPE_INTEGER, { .integer = 123 } }, 1, -1 },
		{ "l", "s1", "a1", { SDB_TYPE_INTEGER, { .integer = 123 } }, 1,  0 },
		{ "l", "s1", "a1", { SDB_TYPE_INTEGER, { .integer = 123 } }, 2,  0 },
		{ "l", "s1", "a2", { SDB_TYPE_INTEGER, { .integer = 123 } }, 1,  0 },
		{ "l", "s2", "a2", { SDB_TYPE_INTEGER, { .integer = 123 } }, 1,  0 },
		{ "m", "s1", "a1", { SDB_TYPE_INTEGER, { .integer = 123 } }, 1,  0 },
	};

	size_t i;

	sdb_memstore_host(store, "m", 1, 0);
	sdb_memstore_host(store, "l", 1, 0);
	sdb_memstore_service(store, "m", "s1", 1, 0);
	sdb_memstore_service(store, "l", "s1", 1, 0);
	sdb_memstore_service(store, "l", "s2", 1, 0);

	for (i = 0; i < SDB_STATIC_ARRAY_LEN(golden_data); ++i) {
		int status;

		status = sdb_memstore_service_attr(store, golden_data[i].host,
				golden_data[i].svc, golden_data[i].attr,
				&golden_data[i].value, golden_data[i].last_update, 0);
		fail_unless(status == golden_data[i].expected,
				"sdb_memstore_service_attr(%s, %s, %s, %d, %d, 0) = %d; "
				"expected: %d", golden_data[i].host, golden_data[i].svc,
				golden_data[i].attr, golden_data[i].value.data.integer,
				golden_data[i].last_update, status, golden_data[i].expected);
	}
}
END_TEST

static struct {
	const char *hostname;
	const char *attr; /* optional */
	int field;
	int expected;
	sdb_data_t value;
} get_field_data[] = {
	{ NULL,   NULL, 0, -1, { SDB_TYPE_NULL, { 0 } } },
	{ NULL,   NULL,   SDB_FIELD_LAST_UPDATE, -1, { SDB_TYPE_NULL, { 0 } } },
	{ NULL,   NULL,   SDB_FIELD_INTERVAL,    -1, { SDB_TYPE_NULL, { 0 } } },
	{ NULL,   NULL,   SDB_FIELD_AGE,         -1, { SDB_TYPE_NULL, { 0 } } },
	{ NULL,   NULL,   SDB_FIELD_NAME,        -1, { SDB_TYPE_NULL, { 0 } } },
	{ NULL,   NULL,   SDB_FIELD_BACKEND,     -1, { SDB_TYPE_NULL, { 0 } } },
	{ NULL,   NULL,   SDB_FIELD_VALUE,       -1, { SDB_TYPE_NULL, { 0 } } },
	{ "host", NULL,   SDB_FIELD_LAST_UPDATE,  0, { SDB_TYPE_DATETIME, { .datetime = 20 } } },
	{ "host", NULL,   SDB_FIELD_INTERVAL,     0, { SDB_TYPE_DATETIME, { .datetime = 10 } } },
	/* the test will handle AGE specially */
	{ "host", NULL,   SDB_FIELD_AGE,          0, { SDB_TYPE_NULL, { 0 } } },
	{ "host", NULL,   SDB_FIELD_NAME,         0, { SDB_TYPE_STRING, { .string = "host" } } },
	{ "host", NULL,   SDB_FIELD_BACKEND,      0, { SDB_TYPE_ARRAY | SDB_TYPE_STRING, { .array = { 0, NULL } } } },
	{ "host", NULL,   SDB_FIELD_VALUE,       -1, { SDB_TYPE_NULL, { 0 } } },
	{ "host", "attr", SDB_FIELD_LAST_UPDATE,  0, { SDB_TYPE_DATETIME, { .datetime = 20 } } },
	{ "host", "attr", SDB_FIELD_INTERVAL,     0, { SDB_TYPE_DATETIME, { .datetime = 10 } } },
	/* the test will handle AGE specially */
	{ "host", "attr", SDB_FIELD_AGE,          0, { SDB_TYPE_NULL, { 0 } } },
	{ "host", "attr", SDB_FIELD_NAME,         0, { SDB_TYPE_STRING, { .string = "attr" } } },
	{ "host", "attr", SDB_FIELD_BACKEND,      0, { SDB_TYPE_ARRAY | SDB_TYPE_STRING, { .array = { 0, NULL } } } },
	{ "host", "attr", SDB_FIELD_VALUE,        0, { SDB_TYPE_INTEGER, { .integer = 1 } } },
	{ "host", "attr", SDB_FIELD_VALUE,        0, { SDB_TYPE_DECIMAL, { .decimal = 2.0 } } },
	{ "host", "attr", SDB_FIELD_VALUE,        0, { SDB_TYPE_STRING, { .string = "foo" } } },
	{ "host", "attr", SDB_FIELD_VALUE,        0, { SDB_TYPE_DATETIME, { .datetime = 1234567890L } } },
	{ "host", "a",    SDB_FIELD_LAST_UPDATE, -1, { SDB_TYPE_NULL, { 0 } } },
	{ "host", "a",    SDB_FIELD_INTERVAL,    -1, { SDB_TYPE_NULL, { 0 } } },
	{ "host", "a",    SDB_FIELD_AGE,         -1, { SDB_TYPE_NULL, { 0 } } },
	{ "host", "a",    SDB_FIELD_NAME,        -1, { SDB_TYPE_NULL, { 0 } } },
	{ "host", "a",    SDB_FIELD_BACKEND,     -1, { SDB_TYPE_NULL, { 0 } } },
	{ "host", "a",    SDB_FIELD_VALUE,       -1, { SDB_TYPE_NULL, { 0 } } },
	{ "host", "a",    SDB_FIELD_VALUE,       -1, { SDB_TYPE_NULL, { 0 } } },
	{ "host", "a",    SDB_FIELD_VALUE,       -1, { SDB_TYPE_NULL, { 0 } } },
	{ "host", "a",    SDB_FIELD_VALUE,       -1, { SDB_TYPE_NULL, { 0 } } },
};

/* returns a tuple <type> <name> */
#define OBJ_NAME(obj) \
	(obj) ? SDB_STORE_TYPE_TO_NAME(obj->type) : "NULL", \
	(obj) ? SDB_OBJ(obj)->name : ""
START_TEST(test_get_field)
{
	sdb_memstore_obj_t *obj = NULL;
	sdb_data_t value = SDB_DATA_INIT;
	char value_str[128], expected_value_str[128];
	sdb_time_t now = sdb_gettime();
	int check;

	sdb_memstore_host(store, "host", 20, 10);
	sdb_memstore_attribute(store, "host", "attr", &get_field_data[_i].value, 20, 10);

	if (get_field_data[_i].hostname) {
		obj = sdb_memstore_get_host(store, get_field_data[_i].hostname);
		ck_assert(obj != NULL);

		if (get_field_data[_i].attr) {
			sdb_memstore_obj_t *tmp = sdb_memstore_get_child(obj,
					SDB_ATTRIBUTE, get_field_data[_i].attr);
			sdb_object_deref(SDB_OBJ(obj));
			obj = tmp;
		}
	}

	check = sdb_memstore_get_field(obj, get_field_data[_i].field, NULL);
	fail_unless(check == get_field_data[_i].expected,
			"sdb_memstore_get_field(%s %s, %s, NULL) = %d; expected: %d",
			OBJ_NAME(obj), SDB_FIELD_TO_NAME(get_field_data[_i].field),
			check, get_field_data[_i].expected);
	check = sdb_memstore_get_field(obj, get_field_data[_i].field, &value);
	fail_unless(check == get_field_data[_i].expected,
			"sdb_memstore_get_field(%s %s, %s, <value>) = %d; expected: %d",
			OBJ_NAME(obj), SDB_FIELD_TO_NAME(get_field_data[_i].field),
			check, get_field_data[_i].expected);

	if (get_field_data[_i].expected) {
		sdb_object_deref(SDB_OBJ(obj));
		return;
	}

	if (get_field_data[_i].field == SDB_FIELD_AGE) {
		get_field_data[_i].value.type = SDB_TYPE_DATETIME;
		get_field_data[_i].value.data.datetime = now;
	}

	sdb_data_format(&value, value_str, sizeof(value_str), 0);
	sdb_data_format(&get_field_data[_i].value, expected_value_str,
			sizeof(expected_value_str), 0);

	if (get_field_data[_i].field == SDB_FIELD_AGE) {
		fail_unless((value.type == SDB_TYPE_DATETIME)
				&& (value.data.datetime >= now),
				"sdb_memstore_get_field(%s %s, %s, <value>) "
				"returned value %s; expected >=%s", OBJ_NAME(obj),
				SDB_FIELD_TO_NAME(get_field_data[_i].field),
				value_str, expected_value_str);
	}
	else {
		fail_unless(! sdb_data_cmp(&value, &get_field_data[_i].value),
				"sdb_memstore_get_field(%s %s, %s, <value>) "
				"returned value %s; expected %s", OBJ_NAME(obj),
				SDB_FIELD_TO_NAME(get_field_data[_i].field),
				value_str, expected_value_str);
	}
	sdb_data_free_datum(&value);
	sdb_object_deref(SDB_OBJ(obj));
}
END_TEST
#undef OBJ_NAME

START_TEST(test_get_child)
{
	struct {
		const char *host;
		int parent_type;
		const char *parent;
		const char *name;
		int type;
		int expected;
	} golden_data[] = {
		{ "h1",          -1, NULL, NULL, SDB_HOST,       0 },
		{ "h1",          -1, NULL, NULL, SDB_SERVICE,   -1 },
		{ "h1",          -1, NULL, NULL, SDB_METRIC,    -1 },
		{ "h1",          -1, NULL, NULL, SDB_ATTRIBUTE, -1 },
		{ "h2",          -1, NULL, NULL, SDB_HOST,       0 },
		{ "h2",          -1, NULL, NULL, SDB_SERVICE,   -1 },
		{ "h2",          -1, NULL, NULL, SDB_METRIC,    -1 },
		{ "h2",          -1, NULL, NULL, SDB_ATTRIBUTE, -1 },
		{ "h3",          -1, NULL, NULL, SDB_HOST,      -1 },
		{ "h1",          -1, NULL, "k1", SDB_ATTRIBUTE,  0 },
		{ "h1",          -1, NULL, "x1", SDB_ATTRIBUTE, -1 },
		{ "h2",          -1, NULL, "k1", SDB_ATTRIBUTE, -1 },
		{ "h1",          -1, NULL, "k1", SDB_SERVICE,   -1 },
		{ "h1",          -1, NULL, "k1", SDB_METRIC,    -1 },
		{ "h1",          -1, NULL, "s1", SDB_SERVICE,   -1 },
		{ "h2",          -1, NULL, "s1", SDB_SERVICE,    0 },
		{ "h1",          -1, NULL, "m2", SDB_METRIC,     0 },
		{ "h2",          -1, NULL, "m2", SDB_METRIC,    -1 },
		{ "h1",  SDB_METRIC, "m1", "k3", SDB_ATTRIBUTE,  0 },
		{ "h1",  SDB_METRIC, "m1", "x1", SDB_ATTRIBUTE, -1 },
		{ "h2", SDB_SERVICE, "s2", "k1", SDB_ATTRIBUTE,  0 },
		{ "h2", SDB_SERVICE, "s2", "x1", SDB_ATTRIBUTE, -1 },
	};

	size_t i;

	populate();

	for (i = 0; i < SDB_STATIC_ARRAY_LEN(golden_data); ++i) {
		sdb_memstore_obj_t *obj;
		const char *expected_name = golden_data[i].host;

		obj = sdb_memstore_get_host(store, golden_data[i].host);
		if (golden_data[i].parent) {
			sdb_memstore_obj_t *o;
			o = sdb_memstore_get_child(obj,
					golden_data[i].parent_type, golden_data[i].parent);
			sdb_object_deref(SDB_OBJ(obj));
			obj = o;
		}
		if (golden_data[i].expected && (golden_data[i].type == SDB_HOST))
			fail_unless(obj == NULL,
					"sdb_memstore_get_host(%s) = %p; expected: NULL",
					golden_data[i].host, obj);
		else
			fail_unless(obj != NULL,
					"sdb_memstore_get_host(%s) = NULL; expected: <host>",
					golden_data[i].host);

		if (golden_data[i].type != SDB_HOST) {
			sdb_memstore_obj_t *tmp;

			expected_name = golden_data[i].name;

			tmp = sdb_memstore_get_child(obj,
					golden_data[i].type, golden_data[i].name);
			if (golden_data[i].expected)
				fail_unless(tmp == NULL,
						"sdb_memstore_get_child(<%s>, %s, %s) = %p; "
						"expected: NULL", golden_data[i].host,
						SDB_STORE_TYPE_TO_NAME(golden_data[i].type),
						golden_data[i].name, tmp);
			else
				fail_unless(tmp != NULL,
						"sdb_memstore_get_child(<%s>, %s, %s) = NULL; "
						"expected: <obj>", golden_data[i].host,
						SDB_STORE_TYPE_TO_NAME(golden_data[i].type),
						golden_data[i].name);

			sdb_object_deref(SDB_OBJ(obj));
			obj = tmp;
		}

		if (golden_data[i].expected)
			continue;

		fail_unless(obj->type == golden_data[i].type,
				"sdb_memstore_get_<%s>(%s, %s) returned object of type %d; "
				"expected: %d", SDB_STORE_TYPE_TO_NAME(golden_data[i].type),
				golden_data[i].host, golden_data[i].name, obj->type,
				golden_data[i].type);
		fail_unless(! strcasecmp(SDB_OBJ(obj)->name, expected_name),
				"sdb_memstore_get_<%s>(%s, %s) returned object named '%s'; "
				"expected: '%s'", SDB_STORE_TYPE_TO_NAME(golden_data[i].type),
				golden_data[i].host, golden_data[i].name, SDB_OBJ(obj)->name,
				expected_name);

		sdb_object_deref(SDB_OBJ(obj));
	}
}
END_TEST

/* TODO: move these tests into generic store tests */
#if 0
START_TEST(test_interval)
{
	sdb_memstore_obj_t *host;

	/* 10 us interval */
	sdb_memstore_host(store, "host", 10);
	sdb_memstore_host(store, "host", 20);
	sdb_memstore_host(store, "host", 30);
	sdb_memstore_host(store, "host", 40);

	host = sdb_memstore_get_host(store, "host");
	fail_unless(host != NULL,
			"INTERNAL ERROR: store doesn't have host after adding it");

	fail_unless(host->interval == 10,
			"sdb_memstore_host() did not calculate interval correctly: "
			"got: %"PRIsdbTIME"; expected: %"PRIsdbTIME, host->interval, 10);

	/* multiple updates for the same timestamp don't modify the interval */
	sdb_memstore_host(store, "host", 40);
	sdb_memstore_host(store, "host", 40);
	sdb_memstore_host(store, "host", 40);
	sdb_memstore_host(store, "host", 40);

	fail_unless(host->interval == 10,
			"sdb_memstore_host() changed interval when doing multiple updates "
			"using the same timestamp; got: %"PRIsdbTIME"; "
			"expected: %"PRIsdbTIME, host->interval, 10);

	/* multiple updates using an timestamp don't modify the interval */
	sdb_memstore_host(store, "host", 20);
	sdb_memstore_host(store, "host", 20);
	sdb_memstore_host(store, "host", 20);
	sdb_memstore_host(store, "host", 20);

	fail_unless(host->interval == 10,
			"sdb_memstore_host() changed interval when doing multiple updates "
			"using an old timestamp; got: %"PRIsdbTIME"; expected: %"PRIsdbTIME,
			host->interval, 10);

	/* new interval: 20 us */
	sdb_memstore_host(store, "host", 60);
	fail_unless(host->interval == 11,
			"sdb_memstore_host() did not calculate interval correctly: "
			"got: %"PRIsdbTIME"; expected: %"PRIsdbTIME, host->interval, 11);

	/* new interval: 40 us */
	sdb_memstore_host(store, "host", 100);
	fail_unless(host->interval == 13,
			"sdb_memstore_host() did not calculate interval correctly: "
			"got: %"PRIsdbTIME"; expected: %"PRIsdbTIME, host->interval, 11);

	sdb_object_deref(SDB_OBJ(host));
}
END_TEST
#endif

static int
scan_count(sdb_memstore_obj_t *obj, sdb_memstore_matcher_t *filter, void *user_data)
{
	intptr_t *i = user_data;

	if (! sdb_memstore_matcher_matches(filter, obj, NULL))
		return 0;

	fail_unless(obj != NULL,
			"sdb_memstore_scan callback received NULL obj; expected: "
			"<store base obj>");
	fail_unless(i != NULL,
			"sdb_memstore_scan callback received NULL user_data; "
			"expected: <pointer to data>");

	++(*i);
	return 0;
} /* scan_count */

static int
scan_error(sdb_memstore_obj_t *obj, sdb_memstore_matcher_t *filter, void *user_data)
{
	intptr_t *i = user_data;

	if (! sdb_memstore_matcher_matches(filter, obj, NULL))
		return 0;

	fail_unless(obj != NULL,
			"sdb_memstore_scan callback received NULL obj; expected: "
			"<store base obj>");
	fail_unless(i != NULL,
			"sdb_memstore_scan callback received NULL user_data; "
			"expected: <pointer to data>");

	++(*i);
	return -1;
} /* scan_error */

START_TEST(test_scan)
{
	intptr_t i = 0;
	int check;

	/* empty store */
	check = sdb_memstore_scan(store, SDB_HOST, /* m, filter = */ NULL, NULL,
			scan_count, &i);
	fail_unless(check == 0,
			"sdb_memstore_scan(HOST), empty store = %d; expected: 0", check);
	fail_unless(i == 0,
			"sdb_memstore_scan(HOST) called callback %d times; "
			"expected: 0", (int)i);

	populate();

	check = sdb_memstore_scan(store, SDB_HOST, /* m, filter = */ NULL, NULL,
			scan_count, &i);
	fail_unless(check == 0,
			"sdb_memstore_scan(HOST) = %d; expected: 0", check);
	fail_unless(i == 2,
			"sdb_memstore_scan(HOST) called callback %d times; "
			"expected: 1", (int)i);

	i = 0;
	check = sdb_memstore_scan(store, SDB_HOST, /* m, filter = */ NULL, NULL,
			scan_error, &i);
	fail_unless(check == -1,
			"sdb_memstore_scan(HOST), error callback = %d; expected: -1", check);
	fail_unless(i == 1,
			"sdb_memstore_scan(HOST) called callback %d times "
			"(callback returned error); expected: 1", (int)i);

	i = 0;
	check = sdb_memstore_scan(store, SDB_SERVICE, /* m, filter = */ NULL, NULL,
			scan_count, &i);
	fail_unless(check == 0,
			"sdb_memstore_scan(SERVICE) = %d; expected: 0", check);
	fail_unless(i == 2,
			"sdb_memstore_scan(SERVICE) called callback %d times; "
			"expected: 2", (int)i);

	i = 0;
	check = sdb_memstore_scan(store, SDB_METRIC, /* m, filter = */ NULL, NULL,
			scan_count, &i);
	fail_unless(check == 0,
			"sdb_memstore_scan(METRIC) = %d; expected: 0", check);
	fail_unless(i == 3,
			"sdb_memstore_scan(METRIC) called callback %d times; "
			"expected: 3", (int)i);
}
END_TEST

TEST_MAIN("core::store")
{
	TCase *tc = tcase_create("core");
	tcase_add_unchecked_fixture(tc, init, turndown);
	tcase_add_test(tc, test_store_host);
	tcase_add_test(tc, test_store_get_host);
	tcase_add_test(tc, test_store_attr);
	tcase_add_test(tc, test_store_metric);
	tcase_add_test(tc, test_store_metric_attr);
	tcase_add_test(tc, test_store_service);
	tcase_add_test(tc, test_store_service_attr);
	TC_ADD_LOOP_TEST(tc, get_field);
	tcase_add_test(tc, test_get_child);
	tcase_add_test(tc, test_scan);
	ADD_TCASE(tc);
}
TEST_MAIN_END

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

