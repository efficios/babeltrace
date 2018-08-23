#ifndef BABELTRACE_CTF_IR_UTILS_INTERNAL_H
#define BABELTRACE_CTF_IR_UTILS_INTERNAL_H

/*
 * Babeltrace - Internal CTF IR utilities
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/ctf-ir/field-types.h>
#include <babeltrace/ctf-ir/clock-class-internal.h>
#include <stdint.h>

struct search_query {
	gpointer value;
	int found;
};

static inline
uint64_t bt_util_ns_from_value(uint64_t frequency, uint64_t value_cycles)
{
	uint64_t ns;

	if (frequency == UINT64_C(1000000000)) {
		ns = value_cycles;
	} else {
		double dblres = ((1e9 * (double) value_cycles) / (double) frequency);

		if (dblres >= (double) UINT64_MAX) {
			/* Overflows uint64_t */
			ns = UINT64_C(-1);
		} else {
			ns = (uint64_t) dblres;
		}
	}

	return ns;
}

static inline
int bt_util_ns_from_origin(struct bt_clock_class *clock_class, uint64_t value,
		int64_t *ns_from_origin)
{
	int ret = 0;
	uint64_t value_ns_unsigned;
	int64_t value_ns_signed;

	if (clock_class->base_offset.overflows) {
		ret = -1;
		goto end;
	}

	/* Initialize to clock class's base offset */
	*ns_from_origin = clock_class->base_offset.value_ns;

	/* Add given value in cycles */
	value_ns_unsigned = bt_util_ns_from_value(clock_class->frequency, value);
	if (value_ns_unsigned >= (uint64_t) INT64_MAX) {
		/*
		 * FIXME: `value_ns_unsigned` could be greater than
		 * `INT64_MAX` in fact: in this case, we need to
		 * subtract `INT64_MAX` from `value_ns_unsigned`, make
		 * sure that the difference is less than `INT64_MAX`,
		 * and try to add them one after the other to
		 * `*ns_from_origin`.
		 */
		ret = -1;
		goto end;
	}

	value_ns_signed = (int64_t) value_ns_unsigned;
	BT_ASSERT(value_ns_signed >= 0);

	if (*ns_from_origin <= 0) {
		goto add_value;
	}

	if (value_ns_signed > INT64_MAX - *ns_from_origin) {
		ret = -1;
		goto end;
	}

add_value:
	*ns_from_origin += value_ns_signed;

end:
	return ret;
}

static inline
bool bt_util_value_is_in_range_signed(uint64_t size, int64_t value)
{
	int64_t min_value = UINT64_C(-1) << (size - 1);
	int64_t max_value = (UINT64_C(1) << (size - 1)) - 1;
	return value >= min_value && value <= max_value;
}

static inline
bool bt_util_value_is_in_range_unsigned(unsigned int size, uint64_t value)
{
	uint64_t max_value = (size == 64) ? UINT64_MAX :
		(UINT64_C(1) << size) - 1;
	return value <= max_value;
}

#endif /* BABELTRACE_CTF_IR_UTILS_INTERNAL_H */
