/*
 * Common Trace Format
 *
 * Floating point read/write functions.
 *
 * Copyright 2010 - Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * Reference: ISO C99 standard 5.2.4
 */

#include <babeltrace/ctf/types.h>
#include <glib.h>
#include <float.h>	/* C99 floating point definitions */
#include <limits.h>	/* C99 limits */
#include <endian.h>

/*
 * This library is limited to binary representation of floating point values.
 * Sign-extension of the exponents is assumed to keep the NaN, +inf, -inf
 * values, but this should be double-checked (TODO).
 */

/*
 * Aliasing float/double and unsigned long is not strictly permitted by strict
 * aliasing, but in practice declaration prunning is well supported, and this permits
 * us to use per-word read/writes rather than per-byte.
 */

#if defined(__GNUC__) || defined(__MINGW32__) || defined(_MSC_VER)
#define HAS_TYPE_PRUNING
#endif

#if (FLT_RADIX != 2)

#error "Unsupported floating point radix"

#endif

union doubleIEEE754 {
	double v;
#ifdef HAS_TYPE_PRUNING
	unsigned long bits[(sizeof(double) + sizeof(unsigned long) - 1) / sizeof(unsigned long)];
#else
	unsigned char bits[sizeof(double)];
#endif
};

union ldoubleIEEE754 {
	long double v;
#ifdef HAS_TYPE_PRUNING
	unsigned long bits[(sizeof(long double) + sizeof(unsigned long) - 1) / sizeof(unsigned long)];
#else
	unsigned char bits[sizeof(long double)];
#endif
};

struct pos_len {
	size_t sign_start, exp_start, mantissa_start, len;
};

void _ctf_float_copy(struct stream_pos *destp,
		     const struct declaration_float *dest_declaration,
		     struct stream_pos *srcp,
		     const struct declaration_float *src_declaration)
{
	uint8_t sign;
	int64_t exp;
	uint64_t mantissa;

	/* Read */
	if (src_declaration->byte_order == LITTLE_ENDIAN) {
		mantissa = ctf_uint_read(srcp, src_declaration->mantissa);
		exp = ctf_int_read(srcp, src_declaration->exp);
		sign = ctf_uint_read(srcp, src_declaration->sign);
	} else {
		sign = ctf_uint_read(srcp, src_declaration->sign);
		exp = ctf_int_read(srcp, src_declaration->exp);
		mantissa = ctf_uint_read(srcp, src_declaration->mantissa);
	}
	/* Write */
	if (dest_declaration->byte_order == LITTLE_ENDIAN) {
		ctf_uint_write(destp, dest_declaration->mantissa, mantissa);
		ctf_int_write(destp, dest_declaration->exp, exp);
		ctf_uint_write(destp, dest_declaration->sign, sign);
	} else {
		ctf_uint_write(destp, dest_declaration->sign, sign);
		ctf_int_write(destp, dest_declaration->exp, exp);
		ctf_uint_write(destp, dest_declaration->mantissa, mantissa);
	}
}

void ctf_float_copy(struct stream_pos *dest, struct stream_pos *src,
		    const struct declaration_float *float_declaration)
{
	align_pos(src, float_declaration->p.alignment);
	align_pos(dest, float_declaration->p.alignment);
	_ctf_float_copy(dest, float_declaration, src, float_declaration);
}

double ctf_double_read(struct stream_pos *srcp,
		       const struct declaration_float *float_declaration)
{
	union doubleIEEE754 u;
	struct declaration_float *dest_declaration = float_declaration_new(NULL,
				DBL_MANT_DIG,
				sizeof(double) * CHAR_BIT - DBL_MANT_DIG,
				BYTE_ORDER,
				__alignof__(double));
	struct stream_pos destp;

	align_pos(srcp, float_declaration->p.alignment);
	init_pos(&destp, (char *) u.bits);
	_ctf_float_copy(&destp, dest_declaration, srcp, float_declaration);
	declaration_unref(&dest_declaration->p);
	return u.v;
}

void ctf_double_write(struct stream_pos *destp,
		      const struct declaration_float *float_declaration,
		      double v)
{
	union doubleIEEE754 u;
	struct declaration_float *src_declaration = float_declaration_new(NULL,
				DBL_MANT_DIG,
				sizeof(double) * CHAR_BIT - DBL_MANT_DIG,
				BYTE_ORDER,
				__alignof__(double));
	struct stream_pos srcp;

	u.v = v;
	align_pos(destp, float_declaration->p.alignment);
	init_pos(&srcp, (char *) u.bits);
	_ctf_float_copy(destp, float_declaration, &srcp, src_declaration);
	declaration_unref(&src_declaration->p);
}

long double ctf_ldouble_read(struct stream_pos *srcp,
			     const struct declaration_float *float_declaration)
{
	union ldoubleIEEE754 u;
	struct declaration_float *dest_declaration = float_declaration_new(NULL,
				LDBL_MANT_DIG,
				sizeof(long double) * CHAR_BIT - LDBL_MANT_DIG,
				BYTE_ORDER,
				__alignof__(long double));
	struct stream_pos destp;

	align_pos(srcp, float_declaration->p.alignment);
	init_pos(&destp, (char *) u.bits);
	_ctf_float_copy(&destp, dest_declaration, srcp, float_declaration);
	declaration_unref(&dest_declaration->p);
	return u.v;
}

void ctf_ldouble_write(struct stream_pos *destp,
		       const struct declaration_float *float_declaration,
		       long double v)
{
	union ldoubleIEEE754 u;
	struct declaration_float *src_declaration = float_declaration_new(NULL,
				LDBL_MANT_DIG,
				sizeof(long double) * CHAR_BIT - LDBL_MANT_DIG,
				BYTE_ORDER,
				__alignof__(long double));
	struct stream_pos srcp;

	u.v = v;
	align_pos(destp, float_declaration->p.alignment);
	init_pos(&srcp, (char *) u.bits);
	_ctf_float_copy(destp, float_declaration, &srcp, src_declaration);
	declaration_unref(&src_declaration->p);
}
