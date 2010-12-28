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
		     const struct type_class_float *dest_class,
		     struct stream_pos *srcp,
		     const struct type_class_float *src_class)
{
	uint8_t sign;
	int64_t exp;
	uint64_t mantissa;

	/* Read */
	if (src->byte_order == LITTLE_ENDIAN) {
		mantissa = ctf_uint_read(srcp, src_class->mantissa);
		exp = ctf_int_read(srcp, src_class->exp);
		sign = ctf_uint_read(srcp, src_class->sign);
	} else {
		sign = ctf_uint_read(srcp, src_class->sign);
		exp = ctf_int_read(srcp, src_class->exp);
		mantissa = ctf_uint_read(srcp, src_class->mantissa);
	}
	/* Write */
	if (dest->byte_order == LITTLE_ENDIAN) {
		ctf_uint_write(destp, dest_class->mantissa, mantissa);
		ctf_int_write(destp, dest_class->exp, exp);
		ctf_uint_write(destp, dest_class->sign, sign);
	} else {
		ctf_uint_write(destp, dest_class->sign, sign);
		ctf_int_write(destp, dest_class->exp, exp);
		ctf_uint_write(destp, dest_class->mantissa, mantissa);
	}
}

void ctf_float_copy(struct stream_pos *dest, struct stream_pos *src,
		    const struct type_class_float *float_class)
{
	align_pos(src, float_class->p.alignment);
	align_pos(dest, float_class->p.alignment);
	_ctf_float_copy(dest, float_class, src, float_class);
}

double ctf_double_read(struct stream_pos *srcp,
		       const struct type_class_float *float_class)
{
	union doubleIEEE754 u;
	struct ctf_float *dest_class = float_type_new(NULL,
				DBL_MANT_DIG,
				sizeof(double) * CHAR_BIT - DBL_MANT_DIG,
				BYTE_ORDER,
				__alignof__(double));
	struct stream_pos destp;

	align_pos(srcp, float_class->p.alignment);
	init_pos(&destp, &u.bits);
	_ctf_float_copy(&destp, dest_class, srcp, float_class);
	float_type_free(dest_class);
	return u.v;
}

void ctf_double_write(struct stream_pos *destp,
		      const struct type_class_float *float_class,
		      double v)
{
	union doubleIEEE754 u;
	struct ctf_float *src_class = float_type_new(NULL,
				DBL_MANT_DIG,
				sizeof(double) * CHAR_BIT - DBL_MANT_DIG,
				BYTE_ORDER,
				__alignof__(double));
	struct stream_pos srcp;

	u.v = v;
	align_pos(destp, float_class->p.alignment);
	init_pos(&srcp, &u.bits);
	_ctf_float_copy(destp, float_class, &srcp, src_class);
	float_type_free(src_class);
}

long double ctf_ldouble_read(struct stream_pos *srcp,
			     const struct type_class_float *float_class)
{
	union ldoubleIEEE754 u;
	struct ctf_float *dest_class = float_type_new(NULL,
				LDBL_MANT_DIG,
				sizeof(long double) * CHAR_BIT - LDBL_MANT_DIG,
				BYTE_ORDER,
				__alignof__(long double));
	struct stream_pos destp;

	align_pos(srcp, float_class->p.alignment);
	init_pos(&destp, &u.bits);
	_ctf_float_copy(&destp, dest_class, srcp, float_class);
	float_type_free(dest_class);
	return u.v;
}

void ctf_ldouble_write(struct stream_pos *destp,
		       const struct type_class_float *float_class,
		       long double v)
{
	union ldoubleIEEE754 u;
	struct ctf_float *src_class = float_type_new(NULL,
				LDBL_MANT_DIG,
				sizeof(long double) * CHAR_BIT - LDBL_MANT_DIG,
				BYTE_ORDER,
				__alignof__(long double));
	struct stream_pos srcp;

	u.v = v;
	align_pos(destp, float_class->p.alignment);
	init_pos(&srcp, &u.bits);
	_ctf_float_copy(destp, float_class, &srcp, src_class);
	float_type_free(src_class);
}
