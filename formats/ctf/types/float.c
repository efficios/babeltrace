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
 * aliasing, but in practice type prunning is well supported, and this permits
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
		     const struct type_float *dest_type,
		     struct stream_pos *srcp,
		     const struct type_float *src_type)
{
	uint8_t sign;
	int64_t exp;
	uint64_t mantissa;

	/* Read */
	if (src_type->byte_order == LITTLE_ENDIAN) {
		mantissa = ctf_uint_read(srcp, src_type->mantissa);
		exp = ctf_int_read(srcp, src_type->exp);
		sign = ctf_uint_read(srcp, src_type->sign);
	} else {
		sign = ctf_uint_read(srcp, src_type->sign);
		exp = ctf_int_read(srcp, src_type->exp);
		mantissa = ctf_uint_read(srcp, src_type->mantissa);
	}
	/* Write */
	if (dest_type->byte_order == LITTLE_ENDIAN) {
		ctf_uint_write(destp, dest_type->mantissa, mantissa);
		ctf_int_write(destp, dest_type->exp, exp);
		ctf_uint_write(destp, dest_type->sign, sign);
	} else {
		ctf_uint_write(destp, dest_type->sign, sign);
		ctf_int_write(destp, dest_type->exp, exp);
		ctf_uint_write(destp, dest_type->mantissa, mantissa);
	}
}

void ctf_float_copy(struct stream_pos *dest, struct stream_pos *src,
		    const struct type_float *float_type)
{
	align_pos(src, float_type->p.alignment);
	align_pos(dest, float_type->p.alignment);
	_ctf_float_copy(dest, float_type, src, float_type);
}

double ctf_double_read(struct stream_pos *srcp,
		       const struct type_float *float_type)
{
	union doubleIEEE754 u;
	struct type_float *dest_type = float_type_new(NULL,
				DBL_MANT_DIG,
				sizeof(double) * CHAR_BIT - DBL_MANT_DIG,
				BYTE_ORDER,
				__alignof__(double));
	struct stream_pos destp;

	align_pos(srcp, float_type->p.alignment);
	init_pos(&destp, (char *) u.bits);
	_ctf_float_copy(&destp, dest_type, srcp, float_type);
	type_unref(&dest_type->p);
	return u.v;
}

void ctf_double_write(struct stream_pos *destp,
		      const struct type_float *float_type,
		      double v)
{
	union doubleIEEE754 u;
	struct type_float *src_type = float_type_new(NULL,
				DBL_MANT_DIG,
				sizeof(double) * CHAR_BIT - DBL_MANT_DIG,
				BYTE_ORDER,
				__alignof__(double));
	struct stream_pos srcp;

	u.v = v;
	align_pos(destp, float_type->p.alignment);
	init_pos(&srcp, (char *) u.bits);
	_ctf_float_copy(destp, float_type, &srcp, src_type);
	type_unref(&src_type->p);
}

long double ctf_ldouble_read(struct stream_pos *srcp,
			     const struct type_float *float_type)
{
	union ldoubleIEEE754 u;
	struct type_float *dest_type = float_type_new(NULL,
				LDBL_MANT_DIG,
				sizeof(long double) * CHAR_BIT - LDBL_MANT_DIG,
				BYTE_ORDER,
				__alignof__(long double));
	struct stream_pos destp;

	align_pos(srcp, float_type->p.alignment);
	init_pos(&destp, (char *) u.bits);
	_ctf_float_copy(&destp, dest_type, srcp, float_type);
	type_unref(&dest_type->p);
	return u.v;
}

void ctf_ldouble_write(struct stream_pos *destp,
		       const struct type_float *float_type,
		       long double v)
{
	union ldoubleIEEE754 u;
	struct type_float *src_type = float_type_new(NULL,
				LDBL_MANT_DIG,
				sizeof(long double) * CHAR_BIT - LDBL_MANT_DIG,
				BYTE_ORDER,
				__alignof__(long double));
	struct stream_pos srcp;

	u.v = v;
	align_pos(destp, float_type->p.alignment);
	init_pos(&srcp, (char *) u.bits);
	_ctf_float_copy(destp, float_type, &srcp, src_type);
	type_unref(&src_type->p);
}
