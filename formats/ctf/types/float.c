/*
 * Common Trace Format
 *
 * Floating point read/write functions.
 *
 * Copyright 2010-2011 EfficiOS Inc. and Linux Foundation
 *
 * Author: Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
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
#include <babeltrace/endian.h>
#include <pthread.h>

/*
 * This library is limited to binary representation of floating point values.
 * We use hardware support for conversion between 32 and 64-bit floating
 * point values.
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
	double vd;
	float vf;
#ifdef HAS_TYPE_PRUNING
	unsigned long bits[(sizeof(double) + sizeof(unsigned long) - 1) / sizeof(unsigned long)];
#else
	unsigned char bits[sizeof(double)];
#endif
};

/*
 * This mutex protects the static temporary float and double
 * declarations (static_float_declaration and static_double_declaration).
 */
static pthread_mutex_t float_mutex = PTHREAD_MUTEX_INITIALIZER;

static struct declaration_float *static_float_declaration,
		*static_double_declaration;

struct pos_len {
	size_t sign_start, exp_start, mantissa_start, len;
};

static void float_lock(void)
{
	int ret;

	ret = pthread_mutex_lock(&float_mutex);
	assert(!ret);
}

static void float_unlock(void)
{
	int ret;

	ret = pthread_mutex_unlock(&float_mutex);
	assert(!ret);
}

int _ctf_float_copy(struct stream_pos *destp,
		    struct definition_float *dest_definition,
		    struct stream_pos *srcp,
		    const struct definition_float *src_definition)
{
	int ret;

	/* We only support copy of same-size floats for now */
	assert(src_definition->declaration->sign->len ==
		dest_definition->declaration->sign->len);
	assert(src_definition->declaration->exp->len ==
		dest_definition->declaration->exp->len);
	assert(src_definition->declaration->mantissa->len ==
		dest_definition->declaration->mantissa->len);
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
	union doubleIEEE754 u;
	struct definition *tmpdef;
	struct definition_float *tmpfloat;
	struct ctf_stream_pos destp;
	struct mmap_align mma;
	int ret;

	float_lock();
	switch (float_declaration->mantissa->len + 1) {
	case FLT_MANT_DIG:
		tmpdef = static_float_declaration->p.definition_new(
				&static_float_declaration->p,
				NULL, 0, 0, "__tmpfloat");
		break;
	case DBL_MANT_DIG:
		tmpdef = static_double_declaration->p.definition_new(
				&static_double_declaration->p,
				NULL, 0, 0, "__tmpfloat");
		break;
	default:
		ret = -EINVAL;
		goto end;
	}
	tmpfloat = container_of(tmpdef, struct definition_float, p);
	memset(&destp, 0, sizeof(destp));
	ctf_init_pos(&destp, -1, O_RDWR);
	mmap_align_set_addr(&mma, (char *) u.bits);
	destp.base_mma = &mma;
	destp.packet_size = sizeof(u) * CHAR_BIT;
	ctf_align_pos(pos, float_declaration->p.alignment);
	ret = _ctf_float_copy(&destp.parent, tmpfloat, ppos, float_definition);
	switch (float_declaration->mantissa->len + 1) {
	case FLT_MANT_DIG:
		float_definition->value = u.vf;
		break;
	case DBL_MANT_DIG:
		float_definition->value = u.vd;
		break;
	default:
		ret = -EINVAL;
		goto end_unref;
	}

end_unref:
	definition_unref(tmpdef);
end:
	float_unlock();
	return ret;
}

int ctf_float_write(struct stream_pos *ppos, struct definition *definition)
{
	struct definition_float *float_definition =
		container_of(definition, struct definition_float, p);
	const struct declaration_float *float_declaration =
		float_definition->declaration;
	struct ctf_stream_pos *pos = ctf_pos(ppos);
	union doubleIEEE754 u;
	struct definition *tmpdef;
	struct definition_float *tmpfloat;
	struct ctf_stream_pos srcp;
	struct mmap_align mma;
	int ret;

	float_lock();
	switch (float_declaration->mantissa->len + 1) {
	case FLT_MANT_DIG:
		tmpdef = static_float_declaration->p.definition_new(
				&static_float_declaration->p,
				NULL, 0, 0, "__tmpfloat");
		break;
	case DBL_MANT_DIG:
		tmpdef = static_double_declaration->p.definition_new(
				&static_double_declaration->p,
				NULL, 0, 0, "__tmpfloat");
		break;
	default:
		ret = -EINVAL;
		goto end;
	}
	tmpfloat = container_of(tmpdef, struct definition_float, p);
	ctf_init_pos(&srcp, -1, O_RDONLY);
	mmap_align_set_addr(&mma, (char *) u.bits);
	srcp.base_mma = &mma;
	srcp.packet_size = sizeof(u) * CHAR_BIT;
	switch (float_declaration->mantissa->len + 1) {
	case FLT_MANT_DIG:
		u.vf = float_definition->value;
		break;
	case DBL_MANT_DIG:
		u.vd = float_definition->value;
		break;
	default:
		ret = -EINVAL;
		goto end_unref;
	}
	ctf_align_pos(pos, float_declaration->p.alignment);
	ret = _ctf_float_copy(ppos, float_definition, &srcp.parent, tmpfloat);

end_unref:
	definition_unref(tmpdef);
end:
	float_unlock();
	return ret;
}

void __attribute__((constructor)) ctf_float_init(void)
{
	static_float_declaration =
		float_declaration_new(FLT_MANT_DIG,
				sizeof(float) * CHAR_BIT - FLT_MANT_DIG,
				BYTE_ORDER,
				__alignof__(float));
	static_double_declaration =
		float_declaration_new(DBL_MANT_DIG,
				sizeof(double) * CHAR_BIT - DBL_MANT_DIG,
				BYTE_ORDER,
				__alignof__(double));
}

void __attribute__((destructor)) ctf_float_fini(void)
{
	declaration_unref(&static_float_declaration->p);
	declaration_unref(&static_double_declaration->p);
}
