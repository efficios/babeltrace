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

static struct declaration_float *static_ldouble_declaration;

struct pos_len {
	size_t sign_start, exp_start, mantissa_start, len;
};

int _ctf_float_copy(struct stream_pos *destp,
		    struct definition_float *dest_definition,
		    struct stream_pos *srcp,
		    const struct definition_float *src_definition)
{
	int ret;

	/* Read */
	if (src_definition->declaration->byte_order == LITTLE_ENDIAN) {
		ret = ctf_integer_read(srcp, &src_definition->mantissa->p);
		if (ret)
			return ret;
		ret = ctf_integer_read(srcp, &src_definition->exp->p);
		if (ret)
			return ret;
		ret = ctf_integer_read(srcp, &src_definition->sign->p);
		if (ret)
			return ret;
	} else {
		ret = ctf_integer_read(srcp, &src_definition->sign->p);
		if (ret)
			return ret;
		ret = ctf_integer_read(srcp, &src_definition->exp->p);
		if (ret)
			return ret;
		ret = ctf_integer_read(srcp, &src_definition->mantissa->p);
		if (ret)
			return ret;
	}

	dest_definition->mantissa->value._unsigned =
		src_definition->mantissa->value._unsigned;
	dest_definition->exp->value._signed =
		src_definition->exp->value._signed;
	dest_definition->sign->value._unsigned =
		src_definition->sign->value._unsigned;

	/* Write */
	if (dest_definition->declaration->byte_order == LITTLE_ENDIAN) {
		ret = ctf_integer_write(destp, &dest_definition->mantissa->p);
		if (ret)
			return ret;
		ret = ctf_integer_write(destp, &dest_definition->exp->p);
		if (ret)
			return ret;
		ret = ctf_integer_write(destp, &dest_definition->sign->p);
		if (ret)
			return ret;
	} else {
		ret = ctf_integer_write(destp, &dest_definition->sign->p);
		if (ret)
			return ret;
		ret = ctf_integer_write(destp, &dest_definition->exp->p);
		if (ret)
			return ret;
		ret = ctf_integer_write(destp, &dest_definition->mantissa->p);
		if (ret)
			return ret;
	}
	return 0;
}

int ctf_float_read(struct stream_pos *ppos, struct definition *definition)
{
	struct definition_float *float_definition =
		container_of(definition, struct definition_float, p);
	const struct declaration_float *float_declaration =
		float_definition->declaration;
	struct ctf_stream_pos *pos = ctf_pos(ppos);
	union ldoubleIEEE754 u;
	struct definition *tmpdef =
		static_ldouble_declaration->p.definition_new(&static_ldouble_declaration->p,
				NULL, 0, 0);
	struct definition_float *tmpfloat =
		container_of(tmpdef, struct definition_float, p);
	struct ctf_stream_pos destp;
	int ret;

	ctf_init_pos(&destp, -1, O_WRONLY);
	destp.base = (char *) u.bits;

	ctf_align_pos(pos, float_declaration->p.alignment);
	ret = _ctf_float_copy(&destp.parent, tmpfloat, ppos, float_definition);
	float_definition->value = u.v;
	definition_unref(tmpdef);
	return ret;
}

int ctf_float_write(struct stream_pos *ppos, struct definition *definition)
{
	struct definition_float *float_definition =
		container_of(definition, struct definition_float, p);
	const struct declaration_float *float_declaration =
		float_definition->declaration;
	struct ctf_stream_pos *pos = ctf_pos(ppos);
	union ldoubleIEEE754 u;
	struct definition *tmpdef =
		static_ldouble_declaration->p.definition_new(&static_ldouble_declaration->p,
				NULL, 0, 0);
	struct definition_float *tmpfloat =
		container_of(tmpdef, struct definition_float, p);
	struct ctf_stream_pos srcp;
	int ret;

	ctf_init_pos(&srcp, -1, O_RDONLY);
	srcp.base = (char *) u.bits;

	u.v = float_definition->value;
	ctf_align_pos(pos, float_declaration->p.alignment);
	ret = _ctf_float_copy(ppos, float_definition, &srcp.parent, tmpfloat);
	definition_unref(tmpdef);
	return ret;
}

void __attribute__((constructor)) ctf_float_init(void)
{
	static_ldouble_declaration =
		float_declaration_new(LDBL_MANT_DIG,
				sizeof(long double) * CHAR_BIT - LDBL_MANT_DIG,
				BYTE_ORDER,
				__alignof__(long double));
}

void __attribute__((destructor)) ctf_float_fini(void)
{
	declaration_unref(&static_ldouble_declaration->p);
}
