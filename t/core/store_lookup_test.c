/*
 * SysDB - t/core/store_lookup_test.c
 * Copyright (C) 2014 Sebastian 'tokkee' Harl <sh@tokkee.org>
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

#include "core/store.h"
#include "core/store-private.h"
#include "libsysdb_test.h"

#include <check.h>
#include <string.h>

static void
populate(void)
{
	const char *hosts[] = { "a", "b", "c" };

	struct {
		const char *host;
		const char *service;
	} services[] = {
		{ "a", "s1" },
		{ "a", "s2" },
		{ "b", "s1" },
		{ "b", "s3" },
	};

	struct {
		const char *host;
		const char *name;
		sdb_data_t  value;
	} attrs[] = {
		{ "a", "k1", { SDB_TYPE_STRING, { .string = "v1" } } },
	};

	size_t i;

	for (i = 0; i < SDB_STATIC_ARRAY_LEN(hosts); ++i) {
		int status = sdb_store_host(hosts[i], 1);
		fail_unless(status == 0,
				"sdb_store_host(%s, 1) = %d; expected: 0",
				hosts[i], status);
	}

	for (i = 0; i < SDB_STATIC_ARRAY_LEN(services); ++i) {
		int status = sdb_store_service(services[i].host,
				services[i].service, 1);
		fail_unless(status == 0,
				"sdb_store_service(%s, %s, 1) = %d; expected: 0",
				services[i].host, services[i].service, status);
	}

	for (i = 0; i < SDB_STATIC_ARRAY_LEN(attrs); ++i) {
		int status = sdb_store_attribute(attrs[i].host,
				attrs[i].name, &attrs[i].value, 1);
		fail_unless(status == 0,
				"sdb_store_attribute(%s, %s, <val>, 1) = %d; expected: 0",
				attrs[i].host, attrs[i].name, status);
	}
} /* populate */

START_TEST(test_store_match)
{
	sdb_store_base_t *obj;

	struct {
		const char *hostname;
		const char *hostname_re;

		const char *service_name;
		const char *service_name_re;

		const char *attr_name;
		const char *attr_name_re;
		const char *attr_value;
		const char *attr_value_re;

		int expected;
	} golden_data[] = {
		{
			/* host */ NULL, NULL,
			/* svc  */ NULL, NULL,
			/* attr */ NULL, NULL, NULL, NULL, 1
		},
		{
			/* host */ "a", NULL,
			/* svc  */ NULL, NULL,
			/* attr */ NULL, NULL, NULL, NULL, 1
		},
		{
			/* host */ "b", NULL,
			/* svc  */ NULL, NULL,
			/* attr */ NULL, NULL, NULL, NULL, 0
		},
		{
			/* host */ NULL, "^a$",
			/* svc  */ NULL, NULL,
			/* attr */ NULL, NULL, NULL, NULL, 1
		},
		{
			/* host */ NULL, "^b$",
			/* svc  */ NULL, NULL,
			/* attr */ NULL, NULL, NULL, NULL, 0
		},
		{
			/* host */ "a", "^a$",
			/* svc  */ NULL, NULL,
			/* attr */ NULL, NULL, NULL, NULL, 1
		},
		{
			/* host */ "a", "^b$",
			/* svc  */ NULL, NULL,
			/* attr */ NULL, NULL, NULL, NULL, 0
		},
		{
			/* host */ "b", "^a$",
			/* svc  */ NULL, NULL,
			/* attr */ NULL, NULL, NULL, NULL, 0
		},
		{
			/* host */ "a", "^a$",
			/* svc  */ "s1", NULL,
			/* attr */ NULL, NULL, NULL, NULL, 1
		},
		{
			/* host */ "a", "^a$",
			/* svc  */ NULL, "^s1$",
			/* attr */ NULL, NULL, NULL, NULL, 1
		},
		{
			/* host */ "a", "^a$",
			/* svc  */ "s1", "^s1$",
			/* attr */ NULL, NULL, NULL, NULL, 1
		},
		{
			/* host */ "a", "^a$",
			/* svc  */ "x1", NULL,
			/* attr */ NULL, NULL, NULL, NULL, 0
		},
		{
			/* host */ "a", "^a$",
			/* svc  */ NULL, "x",
			/* attr */ NULL, NULL, NULL, NULL, 0
		},
		{
			/* host */ "a", "^a$",
			/* svc  */ "x1", "x",
			/* attr */ NULL, NULL, NULL, NULL, 0
		},
		{
			/* host */ "a", "^a$",
			/* svc  */ "s1", "x",
			/* attr */ NULL, NULL, NULL, NULL, 0
		},
		{
			/* host */ "a", "^a$",
			/* svc  */ "x1", "s",
			/* attr */ NULL, NULL, NULL, NULL, 0
		},
		{
			/* host */ "a", "^a$",
			/* svc  */ "s1", "^s1$",
			/* attr */ "k1", NULL, NULL, NULL, 1
		},
		{
			/* host */ "a", "^a$",
			/* svc  */ "s1", "^s1$",
			/* attr */ NULL, "^k", NULL, NULL, 1
		},
		{
			/* host */ "a", "^a$",
			/* svc  */ "s1", "^s1$",
			/* attr */ NULL, NULL, "v1", NULL, 1
		},
		{
			/* host */ "a", "^a$",
			/* svc  */ "s1", "^s1$",
			/* attr */ NULL, NULL, NULL, "^v1$", 1
		},
		{
			/* host */ "a", "^a$",
			/* svc  */ "s1", "^s1$",
			/* attr */ "k1", "1", "v1", "1", 1
		},
	};

	size_t i;

	obj = sdb_store_get_host("a");
	fail_unless(obj != NULL,
			"sdb_store_get_host(a) = NULL; expected: <host>");

	for (i = 0; i < SDB_STATIC_ARRAY_LEN(golden_data); ++i) {
		sdb_store_matcher_t *h, *s, *a;
		int status;

		s = sdb_store_service_matcher(golden_data[i].service_name,
				golden_data[i].service_name_re, NULL);
		fail_unless(s != NULL,
				"sdb_store_service_matcher() = NULL; expected: <matcher>");

		a = sdb_store_attr_matcher(golden_data[i].attr_name,
				golden_data[i].attr_name_re, golden_data[i].attr_value,
				golden_data[i].attr_value_re);
		fail_unless(a != NULL,
				"sdb_store_attr_matcher() = NULL; expected: <matcher>");

		h = sdb_store_host_matcher(golden_data[i].hostname,
				golden_data[i].hostname_re, s, a);
		fail_unless(h != NULL,
				"sdb_store_host_matcher() = NULL: expected: <matcher>");
		/* pass ownership to the host matcher */
		sdb_object_deref(SDB_OBJ(s));
		sdb_object_deref(SDB_OBJ(a));

		status = sdb_store_matcher_matches(h, obj);
		fail_unless(status == golden_data[i].expected,
				"sdb_store_matcher_matches({{%s, %s},{%s, %s},"
				"{%s, %s, %s, %s}}, <host a>) = %d; expected: %d",
				golden_data[i].hostname, golden_data[i].hostname_re,
				golden_data[i].service_name, golden_data[i].service_name_re,
				golden_data[i].attr_name, golden_data[i].attr_name_re,
				golden_data[i].attr_value, golden_data[i].attr_value_re,
				status, golden_data[i].expected);

		sdb_object_deref(SDB_OBJ(h));
	}
}
END_TEST

START_TEST(test_store_match_op)
{
	sdb_store_base_t *obj;

	sdb_store_matcher_t *always = sdb_store_host_matcher(NULL, NULL, NULL, NULL);
	sdb_store_matcher_t *never = sdb_store_host_matcher("a", "b", NULL, NULL);

	struct {
		const char *op;
		sdb_store_matcher_t *left;
		sdb_store_matcher_t *right;
		int expected;
	} golden_data[] = {
		{ "OR",  always, always, 1 },
		{ "OR",  always, never,  1 },
		{ "OR",  never,  always, 1 },
		{ "OR",  never,  never,  0 },
		{ "AND", always, always, 1 },
		{ "AND", always, never,  0 },
		{ "AND", never,  always, 0 },
		{ "AND", never,  never,  0 },
	};

	int status;
	size_t i;

	obj = sdb_store_get_host("a");

	status = sdb_store_matcher_matches(always, obj);
	fail_unless(status == 1,
			"INTERNAL ERROR: 'always' did not match host");
	status = sdb_store_matcher_matches(never, obj);
	fail_unless(status == 0,
			"INTERNAL ERROR: 'never' matches host");

	for (i = 0; i < SDB_STATIC_ARRAY_LEN(golden_data); ++i) {
		sdb_store_matcher_t *m;

		if (! strcmp(golden_data[i].op, "OR"))
			m = sdb_store_dis_matcher(golden_data[i].left,
					golden_data[i].right);
		else if (! strcmp(golden_data[i].op, "AND"))
			m = sdb_store_con_matcher(golden_data[i].left,
					golden_data[i].right);
		else {
			fail("INTERNAL ERROR: unexpected operator %s", golden_data[i].op);
			continue;
		}

#define TO_NAME(v) (((v) == always) ? "always" \
		: ((v) == never) ? "never" : "<unknown>")

		status = sdb_store_matcher_matches(m, obj);
		fail_unless(status == golden_data[i].expected,
				"%s(%s, %s) = %d; expected: %d", golden_data[i].op,
				TO_NAME(golden_data[i].left), TO_NAME(golden_data[i].right),
				status, golden_data[i].expected);

#undef TO_NAME

		sdb_object_deref(SDB_OBJ(m));
	}

	sdb_object_deref(SDB_OBJ(always));
	sdb_object_deref(SDB_OBJ(never));
}
END_TEST

START_TEST(test_parse_cmp)
{
	sdb_store_matcher_t *check;

	size_t i;

	struct {
		const char *obj_type;
		const char *attr;
		const char *op;
		const char *value;
		int expected;
	} golden_data[] = {
		{ "host",      "name", "=",  "hostname", MATCHER_HOST },
		{ "host",      "name", "=~", "hostname", MATCHER_HOST },
		{ "host",      "attr", "=",  "hostname", -1 },
		{ "host",      "name", "&^", "hostname", -1 },
		{ "service",   "name", "=",  "srvname",  MATCHER_SERVICE },
		{ "service",   "name", "=",  "srvname",  MATCHER_SERVICE },
		{ "service",   "attr", "=",  "srvname",  -1 },
		{ "service",   "name", "&^", "srvname",  -1 },
		{ "attribute", "name", "=",  "attrname", MATCHER_ATTR },
		{ "attribute", "name", "=~", "attrname", MATCHER_ATTR },
		{ "attribute", "attr", "=",  "attrname", MATCHER_ATTR },
		{ "attribute", "attr", "=~", "attrname", MATCHER_ATTR },
		{ "attribute", "attr", "&^", "attrname", -1 },
	};

	for (i = 0; i < SDB_STATIC_ARRAY_LEN(golden_data); ++i) {
		check = sdb_store_matcher_parse_cmp(golden_data[i].obj_type,
				golden_data[i].attr, golden_data[i].op, golden_data[i].value);

		if (golden_data[i].expected == -1) {
			fail_unless(check == NULL,
					"sdb_store_matcher_parse_cmp(%s, %s, %s, %s) = %p; "
					"expected: NULL", golden_data[i].obj_type,
					golden_data[i].attr, golden_data[i].op,
					golden_data[i].value, check);
			continue;
		}

		fail_unless(check != NULL,
				"sdb_store_matcher_parse_cmp(%s, %s, %s, %s) = %p; "
				"expected: NULL", golden_data[i].obj_type,
				golden_data[i].attr, golden_data[i].op,
				golden_data[i].value, check);
		fail_unless(M(check)->type == golden_data[i].expected,
				"sdb_store_matcher_parse_cmp(%s, %s, %s, %s) returned matcher "
				"of type %d; expected: %d", golden_data[i].obj_type,
				golden_data[i].attr, golden_data[i].op, golden_data[i].value,
				M(check)->type, golden_data[i].expected);

		sdb_object_deref(SDB_OBJ(check));
	}
}
END_TEST

static int
lookup_cb(sdb_store_base_t *obj, void *user_data)
{
	intptr_t *i = user_data;

	fail_unless(obj != NULL,
			"sdb_store_lookup callback received NULL obj; expected: "
			"<store base obj>");
	fail_unless(i != NULL,
			"sdb_store_lookup callback received NULL user_data; "
			"expected: <pointer to data>");

	++(*i);
	return 0;
} /* lookup_cb */

START_TEST(test_lookup)
{
	intptr_t i = 0;
	int check;

	check = sdb_store_lookup(NULL, lookup_cb, &i);
	fail_unless(check == 0,
			"sdb_store_lookup() = %d; expected: 0", check);
	fail_unless(i == 3,
			"sdb_store_lookup called callback %d times; expected: 3", (int)i);
}
END_TEST

Suite *
core_store_lookup_suite(void)
{
	Suite *s = suite_create("core::store_lookup");
	TCase *tc;

	tc = tcase_create("core");
	tcase_add_checked_fixture(tc, populate, sdb_store_clear);
	tcase_add_test(tc, test_store_match);
	tcase_add_test(tc, test_store_match_op);
	tcase_add_test(tc, test_parse_cmp);
	tcase_add_test(tc, test_lookup);
	suite_add_tcase(s, tc);

	return s;
} /* core_store_lookup_suite */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */
