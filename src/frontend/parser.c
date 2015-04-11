/*
 * SysDB - src/frontend/parser.c
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

#include "sysdb.h"

#include "frontend/connection-private.h"
#include "frontend/parser.h"
#include "frontend/grammar.h"

#include "core/store.h"

#include "utils/llist.h"
#include "utils/strbuf.h"

#include <assert.h>
#include <string.h>

/*
 * private helper functions
 */

static int
scanner_init(const char *input, int len,
		sdb_fe_yyscan_t *scanner, sdb_fe_yyextra_t *extra,
		sdb_strbuf_t *errbuf)
{
	if (! input) {
		sdb_strbuf_sprintf(errbuf, "Missing scanner input");
		return -1;
	}

	memset(extra, 0, sizeof(*extra));
	extra->parsetree = sdb_llist_create();
	extra->mode = SDB_PARSE_DEFAULT;
	extra->errbuf = errbuf;

	if (! extra->parsetree) {
		sdb_strbuf_sprintf(errbuf, "Failed to allocate parse-tree");
		return -1;
	}

	*scanner = sdb_fe_scanner_init(input, len, extra);
	if (! scanner) {
		sdb_llist_destroy(extra->parsetree);
		return -1;
	}
	return 0;
} /* scanner_init */

/*
 * public API
 */

sdb_llist_t *
sdb_fe_parse(const char *query, int len, sdb_strbuf_t *errbuf)
{
	sdb_fe_yyscan_t scanner;
	sdb_fe_yyextra_t yyextra;
	sdb_llist_iter_t *iter;
	int yyres;

	if (scanner_init(query, len, &scanner, &yyextra, errbuf))
		return NULL;

	yyres = sdb_fe_yyparse(scanner);
	sdb_fe_scanner_destroy(scanner);

	if (yyres) {
		sdb_llist_destroy(yyextra.parsetree);
		return NULL;
	}

	iter = sdb_llist_get_iter(yyextra.parsetree);
	while (sdb_llist_iter_has_next(iter)) {
		sdb_conn_node_t *node;
		node = SDB_CONN_NODE(sdb_llist_iter_get_next(iter));
		if (sdb_fe_analyze(node, errbuf)) {
			sdb_llist_iter_destroy(iter);
			sdb_llist_destroy(yyextra.parsetree);
			return NULL;
		}
	}
	sdb_llist_iter_destroy(iter);
	return yyextra.parsetree;
} /* sdb_fe_parse */

sdb_store_matcher_t *
sdb_fe_parse_matcher(const char *cond, int len, sdb_strbuf_t *errbuf)
{
	sdb_fe_yyscan_t scanner;
	sdb_fe_yyextra_t yyextra;

	sdb_conn_node_t *node;
	sdb_store_matcher_t *m;

	int yyres;

	if (scanner_init(cond, len, &scanner, &yyextra, errbuf))
		return NULL;

	yyextra.mode = SDB_PARSE_COND;

	yyres = sdb_fe_yyparse(scanner);
	sdb_fe_scanner_destroy(scanner);

	if (yyres) {
		sdb_llist_destroy(yyextra.parsetree);
		return NULL;
	}

	node = SDB_CONN_NODE(sdb_llist_get(yyextra.parsetree, 0));
	if (! node) {
		sdb_strbuf_sprintf(errbuf, "Empty matcher expression '%s'", cond);
		sdb_llist_destroy(yyextra.parsetree);
		return NULL;
	}

	assert(node->cmd == SDB_CONNECTION_MATCHER);
	m = CONN_MATCHER(node)->matcher;
	CONN_MATCHER(node)->matcher = NULL;

	sdb_llist_destroy(yyextra.parsetree);
	sdb_object_deref(SDB_OBJ(node));
	return m;
} /* sdb_fe_parse_matcher */

sdb_store_expr_t *
sdb_fe_parse_expr(const char *expr, int len, sdb_strbuf_t *errbuf)
{
	sdb_fe_yyscan_t scanner;
	sdb_fe_yyextra_t yyextra;

	sdb_conn_node_t *node;
	sdb_store_expr_t *e;

	int yyres;

	if (scanner_init(expr, len, &scanner, &yyextra, errbuf))
		return NULL;

	yyextra.mode = SDB_PARSE_ARITH;

	yyres = sdb_fe_yyparse(scanner);
	sdb_fe_scanner_destroy(scanner);

	if (yyres) {
		sdb_llist_destroy(yyextra.parsetree);
		return NULL;
	}

	node = SDB_CONN_NODE(sdb_llist_get(yyextra.parsetree, 0));
	if (! node) {
		sdb_strbuf_sprintf(errbuf, "Empty expression '%s'", expr);
		sdb_llist_destroy(yyextra.parsetree);
		return NULL;
	}

	assert(node->cmd == SDB_CONNECTION_EXPR);
	e = CONN_EXPR(node)->expr;
	CONN_EXPR(node)->expr = NULL;

	sdb_llist_destroy(yyextra.parsetree);
	sdb_object_deref(SDB_OBJ(node));
	return e;
} /* sdb_fe_parse_expr */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

