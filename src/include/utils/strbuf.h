/*
 * SysDB - src/include/utils/strbuf.h
 * Copyright (C) 2012 Sebastian 'tokkee' Harl <sh@tokkee.org>
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
 * SysDB string buffer:
 * This is an implementation of an automatically growing string. Whenever
 * writing to the buffer, it will be ensured that enough space is allocated to
 * store all of the string.
 */

#ifndef SDB_UTILS_STRBUF_H
#define SDB_UTILS_STRBUF_H 1

#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sdb_strbuf sdb_strbuf_t;

/*
 * SDB_STRBUF_STR:
 * Return a tuple of a character array and its length representing the content
 * of the string buffer.
 */
#define SDB_STRBUF_STR(buf) sdb_strbuf_string(buf), sdb_strbuf_len(buf)

/*
 * sdb_strbuf_create, sdb_strbuf_destroy:
 * Allocate / deallocate string buffer objects. The initial size of a newly
 * created string buffer is determined by the 'size' argument of the create
 * function.
 *
 * sdb_strbuf_create returns:
 *  - the new string buffer object on success
 *  - NULL else
 */
sdb_strbuf_t *
sdb_strbuf_create(size_t size);

void
sdb_strbuf_destroy(sdb_strbuf_t *strbuf);

/*
 * sdb_strbuf_vappend, sdb_strbuf_append:
 * Append to an existing string buffer. The new text will be added at the end
 * of the current content of the buffer.
 *
 * The 'fmt' and all following arguments are identical to those passed to the
 * sprintf / vsprintf functions.
 *
 * Returns:
 *  - the number of bytes written
 *  - a negative value on error
 */
ssize_t
sdb_strbuf_vappend(sdb_strbuf_t *strbuf, const char *fmt, va_list ap)
		__attribute__((format(printf, 2, 0)));
ssize_t
sdb_strbuf_append(sdb_strbuf_t *strbuf, const char *fmt, ...)
		__attribute__((format(printf, 2, 3)));

/*
 * sdb_strbuf_vsprintf, sdb_strbuf_sprintf:
 * Write to an existing string buffer, overwriting any previous content.
 *
 * The 'fmt' and all following arguments are identical to those passed to the
 * sprintf / vsprintf functions.
 *
 * Returns:
 *  - the number of bytes written
 *  - a negative value on error
 */
ssize_t
sdb_strbuf_vsprintf(sdb_strbuf_t *strbuf, const char *fmt, va_list ap)
		__attribute__((format(printf, 2, 0)));
ssize_t
sdb_strbuf_sprintf(sdb_strbuf_t *strbuf, const char *fmt, ...)
		__attribute__((format(printf, 2, 3)));

/*
 * sdb_strbuf_memcpy, sdb_strbuf_memappend:
 * Copy or a append a memory area to the buffer. These functions do not
 * interpret any information in the data pointer (including \0 bytes).
 *
 * These functions may be used to handle arbitrary byte arrays. Mixing these
 * function calls with any of the printf-style function works fine but the
 * entire buffer content should then be treated as arbitrary bytes.
 *
 * Returns:
 *  - the number of bytes written
 *  - a negative value on error
 */
ssize_t
sdb_strbuf_memcpy(sdb_strbuf_t *strbuf, const void *data, size_t n);
ssize_t
sdb_strbuf_memappend(sdb_strbuf_t *strbuf, const void *data, size_t n);

/*
 * sdb_strbuf_read:
 * Read from an open file-descriptor and append the data to the buffer. The
 * function does not handle *any* read errors. This allows for more
 * flexibility for the caller regarding the handling of EAGAIN or EWOULDBLOCK.
 *
 * Returns:
 *  - the number of bytes read (zero on EOF)
 *  - a negative value on error
 */
ssize_t
sdb_strbuf_read(sdb_strbuf_t *strbuf, int fd, size_t n);

/*
 * sdb_strbuf_chomp:
 * Remove all consecutive newline characters from the end of the string buffer
 * content.
 *
 * Returns:
 *  - the number of bytes removed
 *  - a negative value on error
 */
ssize_t
sdb_strbuf_chomp(sdb_strbuf_t *strbuf);

/*
 * sdb_strbuf_skip:
 * Removes 'n' bytes from the buffer starting at offset 'offset'.
 */
void
sdb_strbuf_skip(sdb_strbuf_t *strbuf, size_t offset, size_t n);

/*
 * sdb_strbuf_clear:
 * Clear the buffer but do not deallocate memory.
 */
void
sdb_strbuf_clear(sdb_strbuf_t *strbuf);

/*
 * sdb_strbuf_string:
 * Returns the content of the string buffer. The caller may not modify the
 * string.
 */
const char *
sdb_strbuf_string(sdb_strbuf_t *strbuf);

/*
 * sdb_strbuf_len:
 * Returns the length of the string buffer's content.
 */
size_t
sdb_strbuf_len(sdb_strbuf_t *strbuf);

/*
 * sdb_strbuf_cap:
 * Returns the current capacity of the string buffer. It describes the max
 * length of the buffer's content (including terminating nul byte) that may be
 * stored in the buffer without resizing it.
 */
size_t
sdb_strbuf_cap(sdb_strbuf_t *strbuf);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* ! SDB_UTILS_STRBUF_H */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

