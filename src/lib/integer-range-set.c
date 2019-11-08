/*
 * Copyright 2019 Philippe Proulx <pproulx@efficios.com>
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

#define BT_LOG_TAG "LIB/INT-RANGE-SET"
#include "lib/logging.h"

#include <stdbool.h>

#include <babeltrace2/babeltrace.h>

#include "lib/assert-pre.h"
#include "common/assert.h"
#include "func-status.h"
#include "integer-range-set.h"

uint64_t bt_integer_range_unsigned_get_lower(
		const struct bt_integer_range_unsigned *u_range)
{
	const struct bt_integer_range *range = (const void *) u_range;

	BT_ASSERT_PRE_DEV_NON_NULL(range, "Integer range");
	return range->lower.u;
}

uint64_t bt_integer_range_unsigned_get_upper(
		const struct bt_integer_range_unsigned *u_range)
{
	const struct bt_integer_range *range = (const void *) u_range;

	BT_ASSERT_PRE_DEV_NON_NULL(range, "Integer range");
	return range->upper.u;
}

int64_t bt_integer_range_signed_get_lower(
		const struct bt_integer_range_signed *i_range)
{
	const struct bt_integer_range *range = (const void *) i_range;

	BT_ASSERT_PRE_DEV_NON_NULL(range, "Integer range");
	return range->lower.i;
}

int64_t bt_integer_range_signed_get_upper(
		const struct bt_integer_range_signed *i_range)
{
	const struct bt_integer_range *range = (const void *) i_range;

	BT_ASSERT_PRE_DEV_NON_NULL(range, "Integer range");
	return range->upper.i;
}

static
bool compare_ranges(const struct bt_integer_range *range_a,
		const struct bt_integer_range *range_b)
{
	return range_a->lower.u == range_b->lower.u &&
		range_a->upper.u == range_b->upper.u;
}

bt_bool bt_integer_range_unsigned_is_equal(
		const struct bt_integer_range_unsigned *range_a,
		const struct bt_integer_range_unsigned *range_b)
{
	BT_ASSERT_PRE_DEV_NON_NULL(range_a, "Integer range A");
	BT_ASSERT_PRE_DEV_NON_NULL(range_b, "Integer range B");
	return (bt_bool) compare_ranges((const void *) range_a,
		(const void *) range_b);
}

bt_bool bt_integer_range_signed_is_equal(
		const struct bt_integer_range_signed *range_a,
		const struct bt_integer_range_signed *range_b)
{
	BT_ASSERT_PRE_DEV_NON_NULL(range_a, "Integer range A");
	BT_ASSERT_PRE_DEV_NON_NULL(range_b, "Integer range B");
	return (bt_bool) compare_ranges((const void *) range_a,
		(const void *) range_b);
}

uint64_t bt_integer_range_set_get_range_count(
		const bt_integer_range_set *range_set)
{
	BT_ASSERT_PRE_DEV_NON_NULL(range_set, "Integer range set");
	return (uint64_t) range_set->ranges->len;
}

const struct bt_integer_range_unsigned *
bt_integer_range_set_unsigned_borrow_range_by_index_const(
		const bt_integer_range_set_unsigned *u_range_set,
		uint64_t index)
{
	const struct bt_integer_range_set *range_set =
		(const void *) u_range_set;

	BT_ASSERT_PRE_DEV_NON_NULL(range_set, "Integer range set");
	BT_ASSERT_PRE_DEV_VALID_INDEX(index, range_set->ranges->len);
	return (const void *) BT_INTEGER_RANGE_SET_RANGE_AT_INDEX(range_set,
		index);
}

const struct bt_integer_range_signed *
bt_integer_range_set_signed_borrow_range_by_index_const(
		const bt_integer_range_set_signed *i_range_set, uint64_t index)
{
	const struct bt_integer_range_set *range_set =
		(const void *) i_range_set;

	BT_ASSERT_PRE_DEV_NON_NULL(range_set, "Integer range set");
	BT_ASSERT_PRE_DEV_VALID_INDEX(index, range_set->ranges->len);
	return (const void *) BT_INTEGER_RANGE_SET_RANGE_AT_INDEX(range_set,
		index);
}

static
void destroy_range_set(struct bt_object *obj)
{
	struct bt_integer_range_set *range_set = (void *) obj;

	BT_LIB_LOGD("Destroying integer range set: %!+R", range_set);

	if (range_set->ranges) {
		g_array_free(range_set->ranges, TRUE);
		range_set->ranges = NULL;
	}

	g_free(range_set);
}

static
struct bt_integer_range_set *create_range_set(void)
{
	struct bt_integer_range_set *range_set;

	BT_LOGD_STR("Creating empty integer range set.");
	range_set = g_new0(struct bt_integer_range_set, 1);

	if (!range_set) {
		BT_LIB_LOGE_APPEND_CAUSE(
			"Failed to allocate one integer range set.");
		goto error;
	}

	bt_object_init_shared(&range_set->base, destroy_range_set);
	range_set->ranges = g_array_new(FALSE, TRUE,
		sizeof(struct bt_integer_range));
	if (!range_set->ranges) {
		BT_LIB_LOGE_APPEND_CAUSE(
			"Failed to allocate integer range set's range array.");
		goto error;
	}

	BT_LOGD_STR("Created empty integer range set.");
	goto end;

error:
	BT_OBJECT_PUT_REF_AND_RESET(range_set);

end:
	return range_set;
}

struct bt_integer_range_set_unsigned *bt_integer_range_set_unsigned_create(void)
{
	BT_ASSERT_PRE_NO_ERROR();

	return (void *) create_range_set();
}

struct bt_integer_range_set_signed *bt_integer_range_set_signed_create(void)
{
	BT_ASSERT_PRE_NO_ERROR();

	return (void *) create_range_set();
}

static
void add_range_to_range_set(struct bt_integer_range_set *range_set,
		uint64_t u_lower, uint64_t u_upper)
{
	struct bt_integer_range range = {
		.lower.u = u_lower,
		.upper.u = u_upper,
	};

	BT_ASSERT_PRE_NON_NULL(range_set, "Integer range set");
	BT_ASSERT_PRE_DEV_HOT(range_set, "Integer range set", ": %!+R",
		range_set);
	g_array_append_val(range_set->ranges, range);
	BT_LIB_LOGD("Added integer range to integer range set: "
		"%![range-set-]+R, lower-unsigned=%" PRIu64 ", "
		"upper-unsigned=%" PRIu64, range_set, u_lower, u_upper);
}

enum bt_integer_range_set_add_range_status
bt_integer_range_set_unsigned_add_range(
		struct bt_integer_range_set_unsigned *range_set,
		uint64_t lower, uint64_t upper)
{
	BT_ASSERT_PRE_NO_ERROR();
	BT_ASSERT_PRE(lower <= upper,
		"Range's upper bound is less than lower bound: "
		"upper=%" PRIu64 ", lower=%" PRIu64, lower, upper);
	add_range_to_range_set((void *) range_set, lower, upper);
	return BT_FUNC_STATUS_OK;
}

enum bt_integer_range_set_add_range_status
bt_integer_range_set_signed_add_range(
		struct bt_integer_range_set_signed *range_set,
		int64_t lower, int64_t upper)
{
	BT_ASSERT_PRE_NO_ERROR();
	BT_ASSERT_PRE(lower <= upper,
		"Range's upper bound is less than lower bound: "
		"upper=%" PRId64 ", lower=%" PRId64, lower, upper);
	add_range_to_range_set((void *) range_set,
		(int64_t) lower, (int64_t) upper);
	return BT_FUNC_STATUS_OK;
}

BT_HIDDEN
void _bt_integer_range_set_freeze(const struct bt_integer_range_set *range_set)
{
	BT_ASSERT(range_set);
	BT_LIB_LOGD("Freezing integer range set: %!+R", range_set);
	((struct bt_integer_range_set *) range_set)->frozen = true;
}

BT_HIDDEN
bool bt_integer_range_set_unsigned_has_overlaps(
		const struct bt_integer_range_set *range_set)
{
	uint64_t i, j;
	bool has_overlap = false;

	BT_ASSERT(range_set);

	for (i = 0; i < range_set->ranges->len; i++) {
		const struct bt_integer_range *range_i =
			BT_INTEGER_RANGE_SET_RANGE_AT_INDEX(range_set, i);

		for (j = 0; j < range_set->ranges->len; j++) {
			const struct bt_integer_range *range_j =
				BT_INTEGER_RANGE_SET_RANGE_AT_INDEX(
					range_set, j);

			if (i == j) {
				continue;
			}

			if (range_i->lower.u <= range_j->upper.u &&
					range_j->lower.u <= range_i->upper.u) {
				has_overlap = true;
				goto end;
			}
		}
	}

end:
	return has_overlap;
}

BT_HIDDEN
bool bt_integer_range_set_signed_has_overlaps(
		const struct bt_integer_range_set *range_set)
{
	uint64_t i, j;
	bool has_overlap = false;

	BT_ASSERT(range_set);

	for (i = 0; i < range_set->ranges->len; i++) {
		const struct bt_integer_range *range_i =
			BT_INTEGER_RANGE_SET_RANGE_AT_INDEX(range_set, i);

		for (j = 0; j < range_set->ranges->len; j++) {
			const struct bt_integer_range *range_j =
				BT_INTEGER_RANGE_SET_RANGE_AT_INDEX(
					range_set, j);

			if (i == j) {
				continue;
			}

			if (range_i->lower.i <= range_j->upper.i &&
					range_j->lower.i <= range_i->upper.i) {
				has_overlap = true;
				goto end;
			}
		}
	}

end:
	return has_overlap;
}

static
bool compare_range_sets(const struct bt_integer_range_set *range_set_a,
		const struct bt_integer_range_set *range_set_b)
{
	uint64_t a_i, b_i;
	bool is_equal = true;

	BT_ASSERT_DBG(range_set_a);
	BT_ASSERT_DBG(range_set_b);

	if (range_set_a == range_set_b) {
		goto end;
	}

	/*
	 * Not super effective for the moment: do a O(N²) compare, also
	 * checking that the sizes match.
	 */
	if (range_set_a->ranges->len != range_set_b->ranges->len) {
		is_equal = false;
		goto end;
	}

	for (a_i = 0; a_i < range_set_a->ranges->len; a_i++) {
		const struct bt_integer_range *range_a =
			BT_INTEGER_RANGE_SET_RANGE_AT_INDEX(range_set_a,
				a_i);
		bool b_has_range = false;

		for (b_i = 0; b_i < range_set_b->ranges->len; b_i++) {
			const struct bt_integer_range *range_b =
				BT_INTEGER_RANGE_SET_RANGE_AT_INDEX(
					range_set_b, b_i);

			if (compare_ranges(range_a, range_b)) {
				b_has_range = true;
				break;
			}
		}

		if (!b_has_range) {
			is_equal = false;
			goto end;
		}
	}

end:
	return is_equal;
}

bt_bool bt_integer_range_set_unsigned_is_equal(
		const struct bt_integer_range_set_unsigned *range_set_a,
		const struct bt_integer_range_set_unsigned *range_set_b)
{
	BT_ASSERT_PRE_DEV_NON_NULL(range_set_a, "Range set A");
	BT_ASSERT_PRE_DEV_NON_NULL(range_set_b, "Range set B");
	return (bt_bool) compare_range_sets((const void *) range_set_a,
		(const void *) range_set_b);
}

bt_bool bt_integer_range_set_signed_is_equal(
		const struct bt_integer_range_set_signed *range_set_a,
		const struct bt_integer_range_set_signed *range_set_b)
{
	BT_ASSERT_PRE_DEV_NON_NULL(range_set_a, "Range set A");
	BT_ASSERT_PRE_DEV_NON_NULL(range_set_b, "Range set B");
	return (bt_bool) compare_range_sets((const void *) range_set_a,
		(const void *) range_set_b);
}

void bt_integer_range_set_unsigned_get_ref(
		const struct bt_integer_range_set_unsigned *range_set)
{
	bt_object_get_ref(range_set);
}

void bt_integer_range_set_unsigned_put_ref(
		const struct bt_integer_range_set_unsigned *range_set)
{
	bt_object_put_ref(range_set);
}

void bt_integer_range_set_signed_get_ref(
		const struct bt_integer_range_set_signed *range_set)
{
	bt_object_get_ref(range_set);
}

void bt_integer_range_set_signed_put_ref(
		const struct bt_integer_range_set_signed *range_set)
{
	bt_object_put_ref(range_set);
}
