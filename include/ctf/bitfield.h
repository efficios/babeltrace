#ifndef _CTF_BITFIELD_H
#define _CTF_BITFIELD_H

/*
 * Common Trace Format
 *
 * Bitfields read/write functions.
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
 */

#include <stdint.h>	/* C99 5.2.4.2 Numerical limits */
#include <limits.h>	/* C99 5.2.4.2 Numerical limits */
#include <endian.h>	/* Non-standard BIG_ENDIAN, LITTLE_ENDIAN, BYTE_ORDER */
#include <assert.h>

/* We can't shift a int from 32 bit, >> 32 and << 32 on int is undefined */
#define _ctf_piecewise_rshift(v, shift)					\
({									\
	unsigned long sb = (shift) / (sizeof(v) * CHAR_BIT - 1);	\
	unsigned long final = (shift) % (sizeof(v) * CHAR_BIT - 1);	\
	typeof(v) _v = (v);						\
									\
	for (; sb; sb--)						\
		_v >>= sizeof(v) * CHAR_BIT - 1;			\
	_v >>= final;							\
})

#define _ctf_piecewise_lshift(v, shift)					\
({									\
	unsigned long sb = (shift) / (sizeof(v) * CHAR_BIT - 1);	\
	unsigned long final = (shift) % (sizeof(v) * CHAR_BIT - 1);	\
	typeof(v) _v = (v);						\
									\
	for (; sb; sb--)						\
		_v <<= sizeof(v) * CHAR_BIT - 1;			\
	_v <<= final;							\
})

#define _ctf_is_signed_type(type)	(((type)(-1)) < 0)

#define _ctf_unsigned_cast(type, v)					\
({									\
	(sizeof(v) < sizeof(type)) ?					\
		((type) (v)) & (~(~(type) 0 << (sizeof(v) * CHAR_BIT))) : \
		(type) (v);						\
})

/*
 * ctf_bitfield_write - write integer to a bitfield in native endianness
 *
 * Save integer to the bitfield, which starts at the "start" bit, has "len"
 * bits.
 * The inside of a bitfield is from high bits to low bits.
 * Uses native endianness.
 * For unsigned "v", pad MSB with 0 if bitfield is larger than v.
 * For signed "v", sign-extend v if bitfield is larger than v.
 *
 * On little endian, bytes are placed from the less significant to the most
 * significant. Also, consecutive bitfields are placed from lower bits to higher
 * bits.
 *
 * On big endian, bytes are places from most significant to less significant.
 * Also, consecutive bitfields are placed from higher to lower bits.
 */

#define _ctf_bitfield_write_le(ptr, _start, _length, _v)		\
do {									\
	typeof(_v) v = (_v);						\
	typeof(*(ptr)) mask, cmask;					\
	unsigned long ts = sizeof(typeof(*(ptr))) * CHAR_BIT; /* type size */ \
	unsigned long start = (_start), length = (_length);		\
	unsigned long start_unit, end_unit, this_unit;			\
	unsigned long end, cshift; /* cshift is "complement shift" */	\
									\
	if (!length)							\
		break;							\
									\
	end = start + length;						\
	start_unit = start / ts;					\
	end_unit = (end + (ts - 1)) / ts;				\
									\
	/* Trim v high bits */						\
	if (length < sizeof(v) * CHAR_BIT)				\
		v &= ~((~(typeof(v)) 0) << length);			\
									\
	/* We can now append v with a simple "or", shift it piece-wise */ \
	this_unit = start_unit;						\
	if (start_unit == end_unit - 1) {				\
		mask = ~((~(typeof(*(ptr))) 0) << (start % ts));	\
		if (end % ts)						\
			mask |= (~(typeof(*(ptr))) 0) << (end % ts);	\
		cmask = (typeof(*(ptr))) v << (start % ts);		\
		cmask &= ~mask;						\
		(ptr)[this_unit] &= mask;				\
		(ptr)[this_unit] |= cmask;				\
		break;							\
	}								\
	if (start % ts) {						\
		cshift = start % ts;					\
		mask = ~((~(typeof(*(ptr))) 0) << cshift);		\
		cmask = (typeof(*(ptr))) v << cshift;			\
		cmask &= ~mask;						\
		(ptr)[this_unit] &= mask;				\
		(ptr)[this_unit] |= cmask;				\
		v = _ctf_piecewise_rshift(v, ts - cshift);		\
		start += ts - cshift;					\
		this_unit++;						\
	}								\
	for (; this_unit < end_unit - 1; this_unit++) {			\
		(ptr)[this_unit] = (typeof(*(ptr))) v;			\
		v = _ctf_piecewise_rshift(v, ts);			\
		start += ts;						\
	}								\
	if (end % ts) {							\
		mask = (~(typeof(*(ptr))) 0) << (end % ts);		\
		cmask = (typeof(*(ptr))) v;				\
		cmask &= ~mask;						\
		(ptr)[this_unit] &= mask;				\
		(ptr)[this_unit] |= cmask;				\
	} else								\
		(ptr)[this_unit] = (typeof(*(ptr))) v;			\
} while (0)

#define _ctf_bitfield_write_be(ptr, _start, _length, _v)		\
do {									\
	typeof(_v) v = (_v);						\
	typeof(*(ptr)) mask, cmask;					\
	unsigned long ts = sizeof(typeof(*(ptr))) * CHAR_BIT; /* type size */ \
	unsigned long start = _start, length = _length;			\
	unsigned long start_unit, end_unit, this_unit;			\
	unsigned long end, cshift; /* cshift is "complement shift" */	\
									\
	if (!length)							\
		break;							\
									\
	end = start + length;						\
	start_unit = start / ts;					\
	end_unit = (end + (ts - 1)) / ts;				\
									\
	/* Trim v high bits */						\
	if (length < sizeof(v) * CHAR_BIT)				\
		v &= ~((~(typeof(v)) 0) << length);			\
									\
	/* We can now append v with a simple "or", shift it piece-wise */ \
	this_unit = end_unit - 1;					\
	if (start_unit == end_unit - 1) {				\
		mask = ~((~(typeof(*(ptr))) 0) << ((ts - (end % ts)) % ts)); \
		if (start % ts)						\
			mask |= (~((typeof(*(ptr))) 0)) << (ts - (start % ts)); \
		cmask = (typeof(*(ptr))) v << ((ts - (end % ts)) % ts);	\
		cmask &= ~mask;						\
		(ptr)[this_unit] &= mask;				\
		(ptr)[this_unit] |= cmask;				\
		break;							\
	}								\
	if (end % ts) {							\
		cshift = end % ts;					\
		mask = ~((~(typeof(*(ptr))) 0) << (ts - cshift));	\
		cmask = (typeof(*(ptr))) v << (ts - cshift);		\
		cmask &= ~mask;						\
		(ptr)[this_unit] &= mask;				\
		(ptr)[this_unit] |= cmask;				\
		v = _ctf_piecewise_rshift(v, cshift);			\
		end -= cshift;						\
		this_unit--;						\
	}								\
	for (; (long) this_unit >= (long) start_unit + 1; this_unit--) { \
		(ptr)[this_unit] = (typeof(*(ptr))) v;			\
		v = _ctf_piecewise_rshift(v, ts);			\
		end -= ts;						\
	}								\
	if (start % ts) {						\
		mask = (~(typeof(*(ptr))) 0) << (ts - (start % ts));	\
		cmask = (typeof(*(ptr))) v;				\
		cmask &= ~mask;						\
		(ptr)[this_unit] &= mask;				\
		(ptr)[this_unit] |= cmask;				\
	} else								\
		(ptr)[this_unit] = (typeof(*(ptr))) v;			\
} while (0)

/*
 * ctf_bitfield_write - write integer to a bitfield in native endianness
 * ctf_bitfield_write_le - write integer to a bitfield in little endian
 * ctf_bitfield_write_be - write integer to a bitfield in big endian
 */

#if (BYTE_ORDER == LITTLE_ENDIAN)

#define ctf_bitfield_write(ptr, _start, _length, _v)			\
	_ctf_bitfield_write_le(ptr, _start, _length, _v)

#define ctf_bitfield_write_le(ptr, _start, _length, _v)			\
	_ctf_bitfield_write_le(ptr, _start, _length, _v)
	
#define ctf_bitfield_write_be(ptr, _start, _length, _v)			\
	_ctf_bitfield_write_be((unsigned char *) (ptr), _start, _length, _v)

#elif (BYTE_ORDER == BIG_ENDIAN)

#define ctf_bitfield_write(ptr, _start, _length, _v)			\
	_ctf_bitfield_write_be(ptr, _start, _length, _v)

#define ctf_bitfield_write_le(ptr, _start, _length, _v)			\
	_ctf_bitfield_write_le((unsigned char *) (ptr), _start, _length, _v)
	
#define ctf_bitfield_write_be(ptr, _start, _length, _v)			\
	_ctf_bitfield_write_be(ptr, _start, _length, _v)

#else /* (BYTE_ORDER == PDP_ENDIAN) */

#error "Byte order not supported"

#endif

#define _ctf_bitfield_read_le(ptr, _start, _length, vptr)		\
do {									\
	typeof(*(vptr)) v;						\
	typeof(*(ptr)) mask, cmask;					\
	unsigned long ts = sizeof(typeof(*(ptr))) * CHAR_BIT; /* type size */ \
	unsigned long start = _start, length = _length;			\
	unsigned long start_unit, end_unit, this_unit;			\
	unsigned long end, cshift; /* cshift is "complement shift" */	\
									\
	if (!length) {							\
		*(vptr) = 0;						\
		break;							\
	}								\
									\
	end = start + length;						\
	start_unit = start / ts;					\
	end_unit = (end + (ts - 1)) / ts;				\
 \
	this_unit = end_unit - 1; \
	if (_ctf_is_signed_type(typeof(v)) \
	    && ((ptr)[this_unit] & ((typeof(*(ptr))) 1 << ((end % ts ? : ts) - 1)))) \
		v = ~(typeof(v)) 0; \
	else \
		v = 0; \
	if (start_unit == end_unit - 1) { \
		cmask = (ptr)[this_unit]; \
		cmask >>= (start % ts); \
		if ((end - start) % ts) { \
			mask = ~((~(typeof(*(ptr))) 0) << (end - start)); \
			cmask &= mask; \
		} \
		v = _ctf_piecewise_lshift(v, end - start); \
		v |= _ctf_unsigned_cast(typeof(v), cmask); \
		*(vptr) = v; \
		break; \
	} \
	if (end % ts) { \
		cshift = end % ts; \
		mask = ~((~(typeof(*(ptr))) 0) << cshift); \
		cmask = (ptr)[this_unit]; \
		cmask &= mask; \
		v = _ctf_piecewise_lshift(v, cshift); \
		v |= _ctf_unsigned_cast(typeof(v), cmask); \
		end -= cshift; \
		this_unit--; \
	} \
	for (; (long) this_unit >= (long) start_unit + 1; this_unit--) { \
		v = _ctf_piecewise_lshift(v, ts); \
		v |= _ctf_unsigned_cast(typeof(v), (ptr)[this_unit]); \
		end -= ts; \
	} \
	if (start % ts) { \
		mask = ~((~(typeof(*(ptr))) 0) << (ts - (start % ts))); \
		cmask = (ptr)[this_unit]; \
		cmask >>= (start % ts); \
		cmask &= mask; \
		v = _ctf_piecewise_lshift(v, ts - (start % ts)); \
		v |= _ctf_unsigned_cast(typeof(v), cmask); \
	} else { \
		v = _ctf_piecewise_lshift(v, ts); \
		v |= _ctf_unsigned_cast(typeof(v), (ptr)[this_unit]); \
	} \
	*(vptr) = v; \
} while (0)

#define _ctf_bitfield_read_be(ptr, _start, _length, vptr)		\
do {									\
	typeof(*(vptr)) v;						\
	typeof(*(ptr)) mask, cmask;					\
	unsigned long ts = sizeof(typeof(*(ptr))) * CHAR_BIT; /* type size */ \
	unsigned long start = _start, length = _length;			\
	unsigned long start_unit, end_unit, this_unit;			\
	unsigned long end, cshift; /* cshift is "complement shift" */	\
									\
	if (!length) {							\
		*(vptr) = 0;						\
		break;							\
	}								\
									\
	end = start + length;						\
	start_unit = start / ts;					\
	end_unit = (end + (ts - 1)) / ts;				\
 \
	this_unit = start_unit;						\
	if (_ctf_is_signed_type(typeof(v))				\
	    && ((ptr)[this_unit] & ((typeof(*(ptr))) 1 << (ts - (start % ts) - 1)))) \
		v = ~(typeof(v)) 0; \
	else \
		v = 0; \
	if (start_unit == end_unit - 1) { \
		cmask = (ptr)[this_unit]; \
		cmask >>= (ts - (end % ts)) % ts; \
		if ((end - start) % ts) {\
			mask = ~((~(typeof(*(ptr))) 0) << (end - start)); \
			cmask &= mask; \
		} \
		v = _ctf_piecewise_lshift(v, end - start); \
		v |= _ctf_unsigned_cast(typeof(v), cmask); \
		*(vptr) = v; \
		break; \
	} \
	if (start % ts) { \
		mask = ~((~(typeof(*(ptr))) 0) << (ts - (start % ts))); \
		cmask = (ptr)[this_unit]; \
		cmask &= mask; \
		v = _ctf_piecewise_lshift(v, ts - (start % ts)); \
		v |= _ctf_unsigned_cast(typeof(v), cmask); \
		start += ts - (start % ts); \
		this_unit++; \
	} \
	for (; this_unit < end_unit - 1; this_unit++) { \
		v = _ctf_piecewise_lshift(v, ts); \
		v |= _ctf_unsigned_cast(typeof(v), (ptr)[this_unit]); \
		start += ts; \
	} \
	if (end % ts) { \
		mask = ~((~(typeof(*(ptr))) 0) << (end % ts)); \
		cmask = (ptr)[this_unit]; \
		cmask >>= ts - (end % ts); \
		cmask &= mask; \
		v = _ctf_piecewise_lshift(v, end % ts); \
		v |= _ctf_unsigned_cast(typeof(v), cmask); \
	} else { \
		v = _ctf_piecewise_lshift(v, ts); \
		v |= _ctf_unsigned_cast(typeof(v), (ptr)[this_unit]); \
	} \
	*(vptr) = v; \
} while (0)

/*
 * ctf_bitfield_read - read integer from a bitfield in native endianness
 * ctf_bitfield_read_le - read integer from a bitfield in little endian
 * ctf_bitfield_read_be - read integer from a bitfield in big endian
 */

#if (BYTE_ORDER == LITTLE_ENDIAN)

#define ctf_bitfield_read(ptr, _start, _length, _vptr)			\
	_ctf_bitfield_read_le(ptr, _start, _length, _vptr)

#define ctf_bitfield_read_le(ptr, _start, _length, _vptr)		\
	_ctf_bitfield_read_le(ptr, _start, _length, _vptr)
	
#define ctf_bitfield_read_be(ptr, _start, _length, _vptr)		\
	_ctf_bitfield_read_be((const unsigned char *) (ptr), _start, _length, _vptr)

#elif (BYTE_ORDER == BIG_ENDIAN)

#define ctf_bitfield_read(ptr, _start, _length, _vptr)			\
	_ctf_bitfield_read_be(ptr, _start, _length, _vptr)

#define ctf_bitfield_read_le(ptr, _start, _length, _vptr)		\
	_ctf_bitfield_read_le((const unsigned char *) (ptr), _start, _length, _vptr)
	
#define ctf_bitfield_read_be(ptr, _start, _length, _vptr)		\
	_ctf_bitfield_read_be(ptr, _start, _length, _vptr)

#else /* (BYTE_ORDER == PDP_ENDIAN) */

#error "Byte order not supported"

#endif

#endif /* _CTF_BITFIELD_H */
