/*
 * test-bitfield.c
 *
 * BabelTrace - bitfield test program
 *
 * Copyright 2010 - Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; under version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <babeltrace/bitfield.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>

#include <tap/tap.h>

unsigned int glob;

/*
 * This function is only declared to show the size of a bitfield write in
 * objdump.
 */
void fct(void)
{
	bt_bitfield_write(&glob, unsigned int, 12, 15, 0x12345678);
}

/* Test array size, in bytes */
#define TEST_LEN 128
#define NR_TESTS 10
#define SIGNED_TEST_DESC_FMT_STR "Writing and reading back 0x%X, signed"
#define UNSIGNED_TEST_DESC_FMT_STR "Writing and reading back 0x%X, unsigned"
#define DIAG_FMT_STR "Failed reading value written \"%s\"-wise, with start=%i" \
	" and length=%i. Read %llX"

unsigned int srcrand;

#if defined(__i386) || defined(__x86_64)

static inline int fls(int x)
{
	int r;
	asm("bsrl %1,%0\n\t"
	    "cmovzl %2,%0"
	    : "=&r" (r) : "rm" (x), "rm" (-1));
	return r + 1;
}

#elif defined(__PPC__)

static __inline__ int fls(unsigned int x)
{
	int lz;

	asm ("cntlzw %0,%1" : "=r" (lz) : "r" (x));
	return 32 - lz;
}

#else

static int fls(unsigned int x)
{
	int r = 32;

	if (!x)
		return 0;
	if (!(x & 0xFFFF0000U)) {
		x <<= 16;
		r -= 16;
	}
	if (!(x & 0xFF000000U)) {
		x <<= 8;
		r -= 8;
	}
	if (!(x & 0xF0000000U)) {
		x <<= 4;
		r -= 4;
	}
	if (!(x & 0xC0000000U)) {
		x <<= 2;
		r -= 2;
	}
	if (!(x & 0x80000000U)) {
		x <<= 1;
		r -= 1;
	}
	return r;
}

#endif

#define print_byte_array(c, len)	\
do {					\
	unsigned long i;		\
					\
	for (i = 0; i < (len); i++) {	\
		printf("0x%X", (c)[i]);	\
		if (i != (len) - 1)	\
			printf(" ");	\
	}				\
	printf("\n");			\
} while (0)

#define init_byte_array(c, len, val)	\
do {					\
	unsigned long i;		\
					\
	for (i = 0; i < (len); i++)	\
		(c)[i] = (val);		\
} while (0)

#define check_result(ref, val, buffer, typename, start, len,		\
		     desc_fmt_str)					\
({									\
	if ((val) != (ref)) {						\
		fail(desc_fmt_str, ref);				\
		diag(DIAG_FMT_STR, #typename, start, len, val);		\
		printf("# ");						\
		print_byte_array(buffer, TEST_LEN);			\
	}								\
	(val) != (ref);							\
})

void run_test_unsigned(void)
{
	unsigned int src, nrbits;
	union {
		unsigned char c[TEST_LEN];
		unsigned short s[TEST_LEN/sizeof(unsigned short)];
		unsigned int i[TEST_LEN/sizeof(unsigned int)];
		unsigned long l[TEST_LEN/sizeof(unsigned long)];
		unsigned long long ll[TEST_LEN/sizeof(unsigned long long)];
	} target;
	unsigned long long readval;
	unsigned int s, l;

	src = srcrand;
	nrbits = fls(src);

	for (s = 0; s < CHAR_BIT * TEST_LEN; s++) {
		for (l = nrbits; l < (CHAR_BIT * TEST_LEN) - s; l++) {
			init_byte_array(target.c, TEST_LEN, 0xFF);
			bt_bitfield_write(target.c, unsigned char, s, l, src);
			bt_bitfield_read(target.c, unsigned char, s, l, &readval);
			if (check_result(src, readval, target.c, unsigned char,
					  s, l, UNSIGNED_TEST_DESC_FMT_STR)) {
				return;
			}

			init_byte_array(target.c, TEST_LEN, 0xFF);
			bt_bitfield_write(target.s, unsigned short, s, l, src);
			bt_bitfield_read(target.c, unsigned char, s, l, &readval);
			if (check_result(src, readval, target.c, unsigned short,
					  s, l, UNSIGNED_TEST_DESC_FMT_STR)) {
				return;
			}

			init_byte_array(target.c, TEST_LEN, 0xFF);
			bt_bitfield_write(target.i, unsigned int, s, l, src);
			bt_bitfield_read(target.c, unsigned char, s, l, &readval);
			if (check_result(src, readval, target.c, unsigned int,
					   s, l, UNSIGNED_TEST_DESC_FMT_STR)) {
				return;
			}

			init_byte_array(target.c, TEST_LEN, 0xFF);
			bt_bitfield_write(target.l, unsigned long, s, l, src);
			bt_bitfield_read(target.c, unsigned char, s, l, &readval);
			if (check_result(src, readval, target.c, unsigned long,
					  s, l, UNSIGNED_TEST_DESC_FMT_STR)) {
				return;
			}

			init_byte_array(target.c, TEST_LEN, 0xFF);
			bt_bitfield_write(target.ll, unsigned long long, s, l, src);
			bt_bitfield_read(target.c, unsigned char, s, l, &readval);
			if (check_result(src, readval, target.c, unsigned long long,
				     s, l, UNSIGNED_TEST_DESC_FMT_STR)) {
				return;
			}
		}
	}

	pass(UNSIGNED_TEST_DESC_FMT_STR, src);
}

void run_test_signed(void)
{
	int src, nrbits;
	union {
		signed char c[TEST_LEN];
		short s[TEST_LEN/sizeof(short)];
		int i[TEST_LEN/sizeof(int)];
		long l[TEST_LEN/sizeof(long)];
		long long ll[TEST_LEN/sizeof(long long)];
	} target;
	long long readval;
	unsigned int s, l;

	src = srcrand;
	if (src & 0x80000000U)
		nrbits = fls(~src) + 1;	/* Find least significant bit conveying sign */
	else
		nrbits = fls(src) + 1;	/* Keep sign at 0 */

	for (s = 0; s < CHAR_BIT * TEST_LEN; s++) {
		for (l = nrbits; l < (CHAR_BIT * TEST_LEN) - s; l++) {
			init_byte_array(target.c, TEST_LEN, 0x0);
			bt_bitfield_write(target.c, signed char, s, l, src);
			bt_bitfield_read(target.c, signed char, s, l, &readval);
			if (check_result(src, readval, target.c, signed char,
					  s, l, SIGNED_TEST_DESC_FMT_STR)) {
				return;
			}

			init_byte_array(target.c, TEST_LEN, 0x0);
			bt_bitfield_write(target.s, short, s, l, src);
			bt_bitfield_read(target.c, signed char, s, l, &readval);
			if (check_result(src, readval, target.c, short,
					  s, l, SIGNED_TEST_DESC_FMT_STR)) {
				return;
			}

			init_byte_array(target.c, TEST_LEN, 0x0);
			bt_bitfield_write(target.i, int, s, l, src);
			bt_bitfield_read(target.c, signed char, s, l, &readval);
			if (check_result(src, readval, target.c, int,
					  s, l, SIGNED_TEST_DESC_FMT_STR)) {
				return;
			}

			init_byte_array(target.c, TEST_LEN, 0x0);
			bt_bitfield_write(target.l, long, s, l, src);
			bt_bitfield_read(target.c, signed char, s, l, &readval);
			if (check_result(src, readval, target.c, long,
					  s, l, SIGNED_TEST_DESC_FMT_STR)) {
				return;
			}

			init_byte_array(target.c, TEST_LEN, 0x0);
			bt_bitfield_write(target.ll, long long, s, l, src);
			bt_bitfield_read(target.c, signed char, s, l, &readval);
			if (check_result(src, readval, target.c, long long,
					  s, l, SIGNED_TEST_DESC_FMT_STR)) {
				return;
			}
		}
	}

	pass(SIGNED_TEST_DESC_FMT_STR, src);
}

void run_test(void)
{
	int i;
	plan_tests(NR_TESTS * 2 + 6);

	srand(time(NULL));

	srcrand = 0;
	run_test_unsigned();
	srcrand = 0;
	run_test_signed();

	srcrand = 1;
	run_test_unsigned();

	srcrand = ~0U;
	run_test_unsigned();

	srcrand = -1;
	run_test_signed();

	srcrand = (int)0x80000000U;
	run_test_signed();

	for (i = 0; i < NR_TESTS; i++) {
		srcrand = rand();
		run_test_unsigned();
		run_test_signed();
	}
}

static
int print_encodings(unsigned long src, unsigned int shift, unsigned int len)
{
	union {
		unsigned char c[8];
		unsigned short s[4];
		unsigned int i[2];
		unsigned long l[2];
		unsigned long long ll[1];
	} target;
	unsigned long long readval;

	init_byte_array(target.c, 8, 0xFF);
	bt_bitfield_write(target.c, unsigned char, shift, len, src);
	printf("bytewise\n");
	print_byte_array(target.c, 8);

	init_byte_array(target.c, 8, 0xFF);
	bt_bitfield_write(target.s, unsigned short, shift, len, src);
	printf("shortwise\n");
	print_byte_array(target.c, 8);

	init_byte_array(target.c, 8, 0xFF);
	bt_bitfield_write(target.i, unsigned int, shift, len, src);
	printf("intwise\n");
	print_byte_array(target.c, 8);

	init_byte_array(target.c, 8, 0xFF);
	bt_bitfield_write(target.l, unsigned long, shift, len, src);
	printf("longwise\n");
	print_byte_array(target.c, 8);

	init_byte_array(target.c, 8, 0xFF);
	bt_bitfield_write(target.ll, unsigned long long, shift, len, src);
	printf("lluwise\n");
	print_byte_array(target.c, 8);

	bt_bitfield_read(target.c, unsigned char, shift, len, &readval);
	printf("read: %llX\n", readval);
	print_byte_array(target.c, 8);

	return 0;
}

int main(int argc, char **argv)
{
	if (argc > 1) {
		/* Print encodings */
		unsigned long src;
		unsigned int shift, len;

		src = atoi(argv[1]);
		if (argc > 2)
			shift = atoi(argv[2]);
		else
			shift = 12;
		if (argc > 3)
			len = atoi(argv[3]);
		else
			len = 40;
		return print_encodings(src, shift, len);
	}

	/* Run tap-formated tests */
	run_test();
	return exit_status();
}
