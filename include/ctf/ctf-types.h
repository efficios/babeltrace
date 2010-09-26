#ifndef _CTF_TYPES_H
#define _CTF_TYPES_H

/*
 * Common Trace Format
 *
 * Type header
 *
 * Copyright 2010 - Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
 *
 * Dual LGPL v2.1/GPL v2 license.
 */

#include <stdint.h>
#include <glib.h>

/*
 * All write primitives,(as well as read for dynamically sized entities, can
 * receive a NULL ptr/dest parameter. In this case, no write is performed, but
 * the size is returned.
 */

uint64_t ctf_uint_read(const uint8_t *ptr, int byte_order, size_t len);
int64_t ctf_int_read(const uint8_t *ptr, int byte_order, size_t len);
size_t ctf_uint_write(uint8_t *ptr, int byte_order, size_t len, uint64_t v);
size_t ctf_int_write(uint8_t *ptr, int byte_order, size_t len, int64_t v);

/*
 * ctf-types-bitfield.h declares:
 *
 * ctf_bitfield_unsigned_read
 * ctf_bitfield_signed_read
 * ctf_bitfield_unsigned_write
 * ctf_bitfield_signed_write
 */
#include <ctf/ctf-types-bitfield.h>

double ctf_float_read(const uint8_t *ptr, int byte_order, size_t len);
size_t ctf_float_write(uint8_t *ptr, int byte_order, size_t len, double v);

size_t ctf_string_copy(char *dest, const char *src);

/*
 * A GQuark can be translated to/from strings with g_quark_from_string() and
 * g_quark_to_string().
 */
GQuark ctf_enum_uint_to_quark(const struct enum_table *table, uint64_t v);
GQuark ctf_enum_int_to_quark(const struct enum_table *table, uint64_t v);
uint64_t ctf_enum_quark_to_uint(size_t len, int byte_order, GQuark q);
int64_t ctf_enum_quark_to_int(size_t len, int byte_order, GQuark q);
void ctf_enum_signed_insert(struct enum_table *table, int64_t v, GQuark q);
void ctf_enum_unsigned_insert(struct enum_table *table, uint64_t v, GQuark q);
struct enum_table *ctf_enum_new(void);
void ctf_enum_destroy(struct enum_table *table);

#endif /* _CTF_TYPES_H */
