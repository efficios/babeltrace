/*
 * Common Trace Format
 *
 * Floating point read/write functions.
 *
 * Copyright (c) 2010 Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 * Reference: ISO C99 standard 5.2.4
 */

#include <babeltrace/ctf/types.h>
#include <glib.h>
#include <float.h>	/* C99 floating point definitions */
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

void ctf_float_copy(unsigned char *destp, const struct ctf_float *dest,
		    const unsigned char *src, const struct ctf_float *src)
{
	struct pos_len destpos, srcpos;
	union {
		unsigned long long u;
		long long s;
	} tmp;

	destpos.len = dest.exp_len + dest.mantissa_len;
	if (dest.byte_order == LITTLE_ENDIAN) {
		destpos.sign_start = destpos.len - 1;
		destpos.exp_start = destpos.sign_start - dest->exp_len;
		destpos.mantissa_start = 0;
	} else {
		destpos.sign_start = 0;
		destpos.exp_start = 1;
		destpos.mantissa_start = destpos.exp_start + dest->exp_len;
	}

	srcpos.len = src.exp_len + src.mantissa_len;
	if (src.byte_order == LITTLE_ENDIAN) {
		srcpos.sign_start = srcpos.len - 1;
		srcpos.exp_start = srcpos.sign_start - src->exp_len;
		srcpos.mantissa_start = 0;
	} else {
		srcpos.sign_start = 0;
		srcpos.exp_start = 1;
		srcpos.mantissa_start = srcpos.exp_start + src->exp_len;
	}

	/* sign */
	tmp.u = ctf_bitfield_unsigned_read(ptr, srcpos.sign_start, 1,
					   src->byte_order);
	ctf_bitfield_unsigned_write(&u.bits, destpos.sign_start, 1,
				    dest->byte_order, tmp.u);

	/* mantissa (without leading 1). No sign extend. */
	tmp.u = ctf_bitfield_unsigned_read(ptr, srcpos.mantissa_start,
					   src->mantissa_len - 1,
					   src->byte_order);
	ctf_bitfield_unsigned_write(&u.bits, destpos.mantissa_start,
				    dest->mantissa_len - 1, dest->byte_order,
				    tmp.u);

	/* exponent, with sign-extend. */
	tmp.s = ctf_bitfield_signed_read(ptr, srcpos.exp_start, src->exp_len,
					 src->byte_order);
	ctf_bitfield_signed_write(&u.bits, destpos.exp_start, dest->exp_len,
				  dest->byte_order, tmp.s);
}

double ctf_double_read(const unsigned char *ptr, const struct ctf_float *src)
{
	union doubleIEEE754 u;
	struct ctf_float dest = {
		.exp_len = sizeof(double) * CHAR_BIT - DBL_MANT_DIG,
		.mantissa_len = DBL_MANT_DIG,
		.byte_order = BYTE_ORDER,
	};

	ctf_float_copy(&u.bits, &dest, ptr, src);
	return u.v;
}

size_t ctf_double_write(unsigned char *ptr, const struct ctf_float *dest,
		        double v)
{
	union doubleIEEE754 u;
	struct ctf_float src = {
		.exp_len = sizeof(double) * CHAR_BIT - DBL_MANT_DIG,
		.mantissa_len = DBL_MANT_DIG,
		.byte_order = BYTE_ORDER,
	};

	if (!ptr)
		goto end;
	u.v = v;
	ctf_float_copy(ptr, dest, &u.bits, &src);
end:
	return len;
}

long double ctf_ldouble_read(const unsigned char *ptr,
			     const struct ctf_float *src)
{
	union ldoubleIEEE754 u;
	struct ctf_float dest = {
		.exp_len = sizeof(double) * CHAR_BIT - LDBL_MANT_DIG,
		.mantissa_len = LDBL_MANT_DIG,
		.byte_order = BYTE_ORDER,
	};

	ctf_float_copy(&u.bits, &dest, ptr, src);
	return u.v;
}

size_t ctf_ldouble_write(unsigned char *ptr, const struct ctf_float *dest,
		         long double v)
{
	union ldoubleIEEE754 u;
	struct ctf_float src = {
		.exp_len = sizeof(double) * CHAR_BIT - LDBL_MANT_DIG,
		.mantissa_len = LDBL_MANT_DIG,
		.byte_order = BYTE_ORDER,
	};

	if (!ptr)
		goto end;
	u.v = v;
	ctf_float_copy(ptr, dest, &u.bits, &src);
end:
	return len;
}
