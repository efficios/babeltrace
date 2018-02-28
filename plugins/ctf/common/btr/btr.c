/*
 * Babeltrace - CTF binary type reader (BTR)
 *
 * Copyright (c) 2015-2016 EfficiOS Inc. and Linux Foundation
 * Copyright (c) 2015-2016 Philippe Proulx <pproulx@efficios.com>
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

#define BT_LOG_TAG "PLUGIN-CTF-BTR"
#include "logging.h"

#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>
#include <stddef.h>
#include <stdbool.h>
#include <babeltrace/assert-internal.h>
#include <string.h>
#include <babeltrace/bitfield-internal.h>
#include <babeltrace/babeltrace.h>
#include <babeltrace/ctf-ir/field-types-internal.h>
#include <babeltrace/ref.h>
#include <babeltrace/align-internal.h>
#include <glib.h>

#include "btr.h"

#define DIV8(_x)			((_x) >> 3)
#define BYTES_TO_BITS(_x)		((_x) * 8)
#define BITS_TO_BYTES_FLOOR(_x)		DIV8(_x)
#define BITS_TO_BYTES_CEIL(_x)		DIV8((_x) + 7)
#define IN_BYTE_OFFSET(_at)		((_at) & 7)

/* A visit stack entry */
struct stack_entry {
	/*
	 * Current type of base field, one of:
	 *
	 *   * Structure
	 *   * Array
	 *   * Sequence
	 *   * Variant
	 *
	 * Owned by this.
	 */
	struct bt_field_type *base_type;

	/* Length of base field (always 1 for variant types) */
	int64_t base_len;

	/* Lndex of next field to read */
	int64_t index;
};

/* Visit stack */
struct stack {
	/* Entries (struct stack_entry *) (top is last element) */
	GPtrArray *entries;
};

/* Reading states */
enum btr_state {
	BTR_STATE_NEXT_FIELD,
	BTR_STATE_ALIGN_BASIC,
	BTR_STATE_ALIGN_COMPOUND,
	BTR_STATE_READ_BASIC_BEGIN,
	BTR_STATE_READ_BASIC_CONTINUE,
	BTR_STATE_DONE,
};

/* Binary type reader */
struct bt_btr {
	/* Bisit stack */
	struct stack *stack;

	/* Current basic field type */
	struct bt_field_type *cur_basic_field_type;

	/* Current state */
	enum btr_state state;

	/*
	 * Last basic field type's byte order.
	 *
	 * This is used to detect errors since two contiguous basic
	 * types for which the common boundary is not the boundary of
	 * a byte cannot have different byte orders.
	 *
	 * This is set to BT_BYTE_ORDER_UNKNOWN on reset and when
	 * the last basic field type was a string type.
	 */
	enum bt_byte_order last_bo;

	/* Current byte order (copied to last_bo after a successful read) */
	enum bt_byte_order cur_bo;

	/* Stitch buffer infos */
	struct {
		/* Stitch buffer */
		uint8_t buf[16];

		/* Offset, within stitch buffer, of first bit */
		size_t offset;

		/* Length (bits) of data in stitch buffer from offset */
		size_t at;
	} stitch;

	/* User buffer infos */
	struct {
		/* Address */
		const uint8_t *addr;

		/* Offset of data from address (bits) */
		size_t offset;

		/* Current position from offset (bits) */
		size_t at;

		/* Offset of offset within whole packet (bits) */
		size_t packet_offset;

		/* Data size in buffer (bits) */
		size_t sz;

		/* Buffer size (bytes) */
		size_t buf_sz;
	} buf;

	/* User stuff */
	struct {
		/* Callback functions */
		struct bt_btr_cbs cbs;

		/* Private data */
		void *data;
	} user;
};

static inline
const char *btr_state_string(enum btr_state state)
{
	switch (state) {
	case BTR_STATE_NEXT_FIELD:
		return "BTR_STATE_NEXT_FIELD";
	case BTR_STATE_ALIGN_BASIC:
		return "BTR_STATE_ALIGN_BASIC";
	case BTR_STATE_ALIGN_COMPOUND:
		return "BTR_STATE_ALIGN_COMPOUND";
	case BTR_STATE_READ_BASIC_BEGIN:
		return "BTR_STATE_READ_BASIC_BEGIN";
	case BTR_STATE_READ_BASIC_CONTINUE:
		return "BTR_STATE_READ_BASIC_CONTINUE";
	case BTR_STATE_DONE:
		return "BTR_STATE_DONE";
	default:
		return "(unknown)";
	}
}

static
void stack_entry_free_func(gpointer data)
{
	struct stack_entry *entry = data;

	BT_PUT(entry->base_type);
	g_free(entry);
}

static
struct stack *stack_new(void)
{
	struct stack *stack = NULL;

	stack = g_new0(struct stack, 1);
	if (!stack) {
		BT_LOGE_STR("Failed to allocate one stack.");
		goto error;
	}

	stack->entries = g_ptr_array_new_with_free_func(stack_entry_free_func);
	if (!stack->entries) {
		BT_LOGE_STR("Failed to allocate a GPtrArray.");
		goto error;
	}

	BT_LOGD("Created stack: addr=%p", stack);
	return stack;

error:
	g_free(stack);

	return NULL;
}

static
void stack_destroy(struct stack *stack)
{
	if (!stack) {
		return;
	}

	BT_LOGD("Destroying stack: addr=%p", stack);
	g_ptr_array_free(stack->entries, TRUE);
	g_free(stack);
}

static inline
int64_t get_compound_field_type_length(struct bt_btr *btr,
		struct bt_field_type *field_type)
{
	int64_t length;

	switch (bt_field_type_get_type_id(field_type)) {
	case BT_FIELD_TYPE_ID_STRUCT:
		length = (int64_t) bt_field_type_structure_get_field_count(
			field_type);
		break;
	case BT_FIELD_TYPE_ID_VARIANT:
		/* Variant field types always "contain" a single type */
		length = 1;
		break;
	case BT_FIELD_TYPE_ID_ARRAY:
		length = bt_field_type_array_get_length(field_type);
		break;
	case BT_FIELD_TYPE_ID_SEQUENCE:
		length = btr->user.cbs.query.get_sequence_length(field_type,
			btr->user.data);
		break;
	default:
		BT_LOGW("Cannot get field type's field count: btr-addr=%p, "
			"ft-addr=%p, ft-id=%s",
			btr, field_type,
			bt_field_type_id_string(
				bt_field_type_get_type_id(field_type)));
		length = BT_BTR_STATUS_ERROR;
	}

	return length;
}

static
int stack_push(struct stack *stack, struct bt_field_type *base_type,
	size_t base_len)
{
	int ret = 0;
	struct stack_entry *entry;

	BT_ASSERT(stack);
	BT_ASSERT(base_type);

	BT_LOGV("Pushing field type on stack: stack-addr=%p, "
		"ft-addr=%p, ft-id=%s, base-length=%zu, "
		"stack-size-before=%u, stack-size-after=%u",
		stack, base_type, bt_field_type_id_string(
			bt_field_type_get_type_id(base_type)),
		base_len, stack->entries->len, stack->entries->len + 1);
	entry = g_new0(struct stack_entry, 1);
	if (!entry) {
		BT_LOGE("Failed to allocate one stack entry: stack-addr=%p",
			stack);
		ret = BT_BTR_STATUS_ERROR;
		goto end;
	}

	entry->base_type = base_type;
	bt_get(entry->base_type);
	entry->base_len = base_len;
	g_ptr_array_add(stack->entries, entry);

end:
	return ret;
}

static
int stack_push_with_len(struct bt_btr *btr,
		struct bt_field_type *base_type)
{
	int ret = 0;
	int64_t base_len = get_compound_field_type_length(btr, base_type);

	if (base_len < 0) {
		BT_LOGW("Cannot get compound field type's field count: "
			"btr-addr=%p, ft-addr=%p, ft-id=%s",
			btr, base_type, bt_field_type_id_string(
				bt_field_type_get_type_id(base_type)));
		ret = BT_BTR_STATUS_ERROR;
		goto end;
	}

	ret = stack_push(btr->stack, base_type, (size_t) base_len);

end:
	return ret;
}

static inline
unsigned int stack_size(struct stack *stack)
{
	BT_ASSERT(stack);

	return stack->entries->len;
}

static
void stack_pop(struct stack *stack)
{
	BT_ASSERT(stack);
	BT_ASSERT(stack_size(stack));
	BT_LOGV("Popping from stack: "
		"stack-addr=%p, stack-size-before=%u, stack-size-after=%u",
		stack, stack->entries->len, stack->entries->len - 1);
	g_ptr_array_remove_index(stack->entries, stack->entries->len - 1);
}

static inline
bool stack_empty(struct stack *stack)
{
	return stack_size(stack) == 0;
}

static
void stack_clear(struct stack *stack)
{
	BT_ASSERT(stack);

	if (!stack_empty(stack)) {
		g_ptr_array_remove_range(stack->entries, 0, stack_size(stack));
	}

	BT_ASSERT(stack_empty(stack));
}

static inline
struct stack_entry *stack_top(struct stack *stack)
{
	BT_ASSERT(stack);
	BT_ASSERT(stack_size(stack));

	return g_ptr_array_index(stack->entries, stack->entries->len - 1);
}

static inline
size_t available_bits(struct bt_btr *btr)
{
	return btr->buf.sz - btr->buf.at;
}

static inline
void consume_bits(struct bt_btr *btr, size_t incr)
{
	BT_LOGV("Advancing cursor: btr-addr=%p, cur-before=%zu, cur-after=%zu",
		btr, btr->buf.at, btr->buf.at + incr);
	btr->buf.at += incr;
}

static inline
bool has_enough_bits(struct bt_btr *btr, size_t sz)
{
	return available_bits(btr) >= sz;
}

static inline
bool at_least_one_bit_left(struct bt_btr *btr)
{
	return has_enough_bits(btr, 1);
}

static inline
size_t packet_at(struct bt_btr *btr)
{
	return btr->buf.packet_offset + btr->buf.at;
}

static inline
size_t buf_at_from_addr(struct bt_btr *btr)
{
	/*
	 * Considering this:
	 *
	 *     ====== offset ===== (17)
	 *
	 *     xxxxxxxx xxxxxxxx xxxxxxxx xxxxxxxx xxxxxxxx
	 *     ^
	 *     addr (0)           ==== at ==== (12)
	 *
	 * We want this:
	 *
	 *     =============================== (29)
	 */
	return btr->buf.offset + btr->buf.at;
}

static inline
int get_basic_field_type_size(struct bt_btr *btr,
		struct bt_field_type *field_type)
{
	int size;

	switch (bt_field_type_get_type_id(field_type)) {
	case BT_FIELD_TYPE_ID_INTEGER:
		size = bt_field_type_integer_get_size(field_type);
		break;
	case BT_FIELD_TYPE_ID_FLOAT:
	{
		int exp_dig, mant_dig;

		exp_dig =
			bt_field_type_floating_point_get_exponent_digits(
				field_type);
		mant_dig =
			bt_field_type_floating_point_get_mantissa_digits(
				field_type);
		BT_ASSERT(exp_dig >= 0);
		BT_ASSERT(mant_dig >= 0);
		size = exp_dig + mant_dig;
		break;
	}
	case BT_FIELD_TYPE_ID_ENUM:
	{
		struct bt_field_type *int_type;

		int_type = bt_field_type_enumeration_get_container_type(
			field_type);
		BT_ASSERT(int_type);
		size = get_basic_field_type_size(btr, int_type);
		BT_PUT(int_type);
		break;
	}
	default:
		size = BT_BTR_STATUS_ERROR;
		break;
	}

	return size;
}

static
void stitch_reset(struct bt_btr *btr)
{
	btr->stitch.offset = 0;
	btr->stitch.at = 0;
}

static inline
size_t stitch_at_from_addr(struct bt_btr *btr)
{
	return btr->stitch.offset + btr->stitch.at;
}

static
void stitch_append_from_buf(struct bt_btr *btr, size_t sz)
{
	size_t stitch_byte_at;
	size_t buf_byte_at;
	size_t nb_bytes;;

	if (sz == 0) {
		return;
	}

	stitch_byte_at =
		BITS_TO_BYTES_FLOOR(stitch_at_from_addr(btr));
	buf_byte_at = BITS_TO_BYTES_FLOOR(buf_at_from_addr(btr));
	nb_bytes = BITS_TO_BYTES_CEIL(sz);
	BT_ASSERT(nb_bytes > 0);
	BT_ASSERT(btr->buf.addr);
	memcpy(&btr->stitch.buf[stitch_byte_at], &btr->buf.addr[buf_byte_at],
		nb_bytes);
	btr->stitch.at += sz;
	consume_bits(btr, sz);
}

static
void stitch_append_from_remaining_buf(struct bt_btr *btr)
{
	stitch_append_from_buf(btr, available_bits(btr));
}

static
void stitch_set_from_remaining_buf(struct bt_btr *btr)
{
	stitch_reset(btr);
	btr->stitch.offset = IN_BYTE_OFFSET(buf_at_from_addr(btr));
	stitch_append_from_remaining_buf(btr);
}

static inline
enum bt_btr_status read_unsigned_bitfield(const uint8_t *buf, size_t at,
		int64_t field_size, enum bt_byte_order bo, uint64_t *v)
{
	enum bt_btr_status status = BT_BTR_STATUS_OK;

	switch (bo) {
	case BT_BYTE_ORDER_BIG_ENDIAN:
	case BT_BYTE_ORDER_NETWORK:
		bt_bitfield_read_be(buf, uint8_t, at, field_size, v);
		break;
	case BT_BYTE_ORDER_LITTLE_ENDIAN:
		bt_bitfield_read_le(buf, uint8_t, at, field_size, v);
		break;
	default:
		BT_LOGF("Cannot read unsigned bit array: unknown byte order: bo=%d", bo);
		abort();
	}

	BT_LOGV("Read unsigned bit array: cur=%zu, size=%" PRId64 ", "
		"bo=%s, val=%" PRIu64, at, field_size,
		bt_byte_order_string(bo), *v);
	return status;
}

static inline
enum bt_btr_status read_signed_bitfield(const uint8_t *buf, size_t at,
		int64_t field_size, enum bt_byte_order bo, int64_t *v)
{
	enum bt_btr_status status = BT_BTR_STATUS_OK;

	switch (bo) {
	case BT_BYTE_ORDER_BIG_ENDIAN:
	case BT_BYTE_ORDER_NETWORK:
		bt_bitfield_read_be(buf, uint8_t, at, field_size, v);
		break;
	case BT_BYTE_ORDER_LITTLE_ENDIAN:
		bt_bitfield_read_le(buf, uint8_t, at, field_size, v);
		break;
	default:
		BT_LOGF("Cannot read signed bit array: unknown byte order: bo=%d", bo);
		abort();
	}

	BT_LOGV("Read signed bit array: cur=%zu, size=%" PRId64 ", "
		"bo=%s, val=%" PRId64, at, field_size,
		bt_byte_order_string(bo), *v);
	return status;
}

typedef enum bt_btr_status (* read_basic_and_call_cb_t)(struct bt_btr *,
		const uint8_t *, size_t);

static inline
enum bt_btr_status validate_contiguous_bo(struct bt_btr *btr,
		enum bt_byte_order next_bo)
{
	enum bt_btr_status status = BT_BTR_STATUS_OK;

	/* Always valid when at a byte boundary */
	if (packet_at(btr) % 8 == 0) {
		goto end;
	}

	/* Always valid if last byte order is unknown */
	if (btr->last_bo == BT_BYTE_ORDER_UNKNOWN) {
		goto end;
	}

	/* Always valid if next byte order is unknown */
	if (next_bo == BT_BYTE_ORDER_UNKNOWN) {
		goto end;
	}

	/* Make sure last byte order is compatible with the next byte order */
	switch (btr->last_bo) {
	case BT_BYTE_ORDER_BIG_ENDIAN:
	case BT_BYTE_ORDER_NETWORK:
		if (next_bo != BT_BYTE_ORDER_BIG_ENDIAN &&
				next_bo != BT_BYTE_ORDER_NETWORK) {
			status = BT_BTR_STATUS_ERROR;
		}
		break;
	case BT_BYTE_ORDER_LITTLE_ENDIAN:
		if (next_bo != BT_BYTE_ORDER_LITTLE_ENDIAN) {
			status = BT_BTR_STATUS_ERROR;
		}
		break;
	default:
		status = BT_BTR_STATUS_ERROR;
	}

end:
	if (status < 0) {
		BT_LOGW("Cannot read bit array: two different byte orders not at a byte boundary: "
			"btr-addr=%p, last-bo=%s, next-bo=%s",
			btr, bt_byte_order_string(btr->last_bo),
			bt_byte_order_string(next_bo));
	}

	return status;
}

static
enum bt_btr_status read_basic_float_and_call_cb(struct bt_btr *btr,
		const uint8_t *buf, size_t at)
{
	int ret;
	double dblval;
	int64_t field_size;
	enum bt_byte_order bo;
	enum bt_btr_status status = BT_BTR_STATUS_OK;

	field_size = get_basic_field_type_size(btr, btr->cur_basic_field_type);
	bo = bt_field_type_get_byte_order(btr->cur_basic_field_type);
	btr->cur_bo = bo;

	switch (field_size) {
	case 32:
	{
		uint64_t v;
		union {
			uint32_t u;
			float f;
		} f32;

		ret = bt_field_type_floating_point_get_mantissa_digits(
			btr->cur_basic_field_type);
		BT_ASSERT(ret == 24);
		ret = bt_field_type_floating_point_get_exponent_digits(
			btr->cur_basic_field_type);
		BT_ASSERT(ret == 8);
		status = read_unsigned_bitfield(buf, at, field_size, bo, &v);
		if (status != BT_BTR_STATUS_OK) {
			BT_LOGW("Cannot read unsigned 32-bit bit array for floating point number field: "
				"btr-addr=%p, status=%s",
				btr, bt_btr_status_string(status));
			goto end;
		}

		f32.u = (uint32_t) v;
		dblval = (double) f32.f;
		break;
	}
	case 64:
	{
		union {
			uint64_t u;
			double d;
		} f64;

		ret = bt_field_type_floating_point_get_mantissa_digits(
			btr->cur_basic_field_type);
		BT_ASSERT(ret == 53);
		ret = bt_field_type_floating_point_get_exponent_digits(
			btr->cur_basic_field_type);
		BT_ASSERT(ret == 11);
		status = read_unsigned_bitfield(buf, at, field_size, bo,
			&f64.u);
		if (status != BT_BTR_STATUS_OK) {
			BT_LOGW("Cannot read unsigned 64-bit bit array for floating point number field: "
				"btr-addr=%p, status=%s",
				btr, bt_btr_status_string(status));
			goto end;
		}

		dblval = f64.d;
		break;
	}
	default:
		/* Only 32-bit and 64-bit fields are supported currently */
		BT_LOGW("Only 32-bit and 64-bit floating point number fields are supported: "
			"btr-addr=%p", btr);
		status = BT_BTR_STATUS_ERROR;
		goto end;
	}

	BT_LOGV("Read floating point number value: btr=%p, cur=%zu, val=%f",
		btr, at, dblval);

	if (btr->user.cbs.types.floating_point) {
		BT_LOGV("Calling user function (floating point number).");
		status = btr->user.cbs.types.floating_point(dblval,
			btr->cur_basic_field_type, btr->user.data);
		BT_LOGV("User function returned: status=%s",
			bt_btr_status_string(status));
		if (status != BT_BTR_STATUS_OK) {
			BT_LOGW("User function failed: btr-addr=%p, status=%s",
				btr, bt_btr_status_string(status));
		}
	}

end:
	return status;
}

static inline
enum bt_btr_status read_basic_int_and_call(struct bt_btr *btr,
		const uint8_t *buf, size_t at,
		struct bt_field_type *int_type,
		struct bt_field_type *orig_type)
{
	bt_bool signd;
	int64_t field_size;
	enum bt_byte_order bo;
	enum bt_btr_status status = BT_BTR_STATUS_OK;

	signd = bt_field_type_integer_is_signed(int_type);
	field_size = get_basic_field_type_size(btr, int_type);
	if (field_size < 1) {
		BT_LOGW("Cannot get integer field type's size: "
			"btr=%p, at=%zu, ft-addr=%p",
			btr, at, int_type);
		status = BT_BTR_STATUS_ERROR;
		goto end;
	}

	bo = bt_field_type_get_byte_order(int_type);

	/*
	 * Update current byte order now because we could be reading
	 * the integer value of an enumeration type, and thus we know
	 * here the actual supporting integer type's byte order.
	 */
	btr->cur_bo = bo;

	if (signd) {
		int64_t v;

		status = read_signed_bitfield(buf, at, field_size, bo, &v);
		if (status != BT_BTR_STATUS_OK) {
			BT_LOGW("Cannot read signed bit array for signed integer field: "
				"btr-addr=%p, status=%s",
				btr, bt_btr_status_string(status));
			goto end;
		}

		if (btr->user.cbs.types.signed_int) {
			BT_LOGV("Calling user function (signed integer).");
			status = btr->user.cbs.types.signed_int(v,
				btr->cur_basic_field_type, btr->user.data);
			BT_LOGV("User function returned: status=%s",
				bt_btr_status_string(status));
			if (status != BT_BTR_STATUS_OK) {
				BT_LOGW("User function failed: "
					"btr-addr=%p, status=%s",
					btr, bt_btr_status_string(status));
			}
		}
	} else {
		uint64_t v;

		status = read_unsigned_bitfield(buf, at, field_size, bo, &v);
		if (status != BT_BTR_STATUS_OK) {
			BT_LOGW("Cannot read unsigned bit array for unsigned integer field: "
				"btr-addr=%p, status=%s",
				btr, bt_btr_status_string(status));
			goto end;
		}

		if (btr->user.cbs.types.unsigned_int) {
			BT_LOGV("Calling user function (unsigned integer).");
			status = btr->user.cbs.types.unsigned_int(v,
				btr->cur_basic_field_type, btr->user.data);
			BT_LOGV("User function returned: status=%s",
				bt_btr_status_string(status));
			if (status != BT_BTR_STATUS_OK) {
				BT_LOGW("User function failed: "
					"btr-addr=%p, status=%s",
					btr, bt_btr_status_string(status));
			}
		}
	}

end:
	return status;
}

static
enum bt_btr_status read_basic_int_and_call_cb(struct bt_btr *btr,
		const uint8_t *buf, size_t at)
{
	return read_basic_int_and_call(btr, buf, at, btr->cur_basic_field_type,
		btr->cur_basic_field_type);
}

static
enum bt_btr_status read_basic_enum_and_call_cb(struct bt_btr *btr,
		const uint8_t *buf, size_t at)
{
	struct bt_field_type *int_field_type;
	enum bt_btr_status status = BT_BTR_STATUS_OK;

	int_field_type = bt_field_type_enumeration_get_container_type(
		btr->cur_basic_field_type);
	BT_ASSERT(int_field_type);
	status = read_basic_int_and_call(btr, buf, at,
		int_field_type, btr->cur_basic_field_type);
	bt_put(int_field_type);
	return status;
}

static inline
enum bt_btr_status read_basic_type_and_call_continue(struct bt_btr *btr,
		read_basic_and_call_cb_t read_basic_and_call_cb)
{
	size_t available;
	int64_t field_size;
	int64_t needed_bits;
	enum bt_btr_status status = BT_BTR_STATUS_OK;

	if (!at_least_one_bit_left(btr)) {
		BT_LOGV("Reached end of data: btr-addr=%p", btr);
		status = BT_BTR_STATUS_EOF;
		goto end;
	}

	field_size = get_basic_field_type_size(btr, btr->cur_basic_field_type);
	if (field_size < 1) {
		BT_LOGW("Cannot get basic field type's size: "
			"btr-addr=%p, ft-addr=%p",
			btr, btr->cur_basic_field_type);
		status = BT_BTR_STATUS_ERROR;
		goto end;
	}

	available = available_bits(btr);
	needed_bits = field_size - btr->stitch.at;
	BT_LOGV("Continuing basic field decoding: "
		"btr-addr=%p, field-size=%" PRId64 ", needed-size=%" PRId64 ", "
		"available-size=%zu",
		btr, field_size, needed_bits, available);
	if (needed_bits <= available) {
		/* We have all the bits; append to stitch, then decode */
		stitch_append_from_buf(btr, needed_bits);
		status = read_basic_and_call_cb(btr, btr->stitch.buf,
			btr->stitch.offset);
		if (status != BT_BTR_STATUS_OK) {
			BT_LOGW("Cannot read basic field: "
				"btr-addr=%p, ft-addr=%p, status=%s",
				btr, btr->cur_basic_field_type,
				bt_btr_status_string(status));
			goto end;
		}

		if (stack_empty(btr->stack)) {
			/* Root is a basic type */
			btr->state = BTR_STATE_DONE;
		} else {
			/* Go to next field */
			stack_top(btr->stack)->index++;
			btr->state = BTR_STATE_NEXT_FIELD;
			btr->last_bo = btr->cur_bo;
		}
		goto end;
	}

	/* We are here; it means we don't have enough data to decode this */
	BT_LOGV_STR("Not enough data to read the next basic field: appending to stitch buffer.");
	stitch_append_from_remaining_buf(btr);
	status = BT_BTR_STATUS_EOF;

end:
	return status;
}

static inline
enum bt_btr_status read_basic_type_and_call_begin(struct bt_btr *btr,
		read_basic_and_call_cb_t read_basic_and_call_cb)
{
	size_t available;
	int64_t field_size;
	enum bt_byte_order bo;
	enum bt_btr_status status = BT_BTR_STATUS_OK;

	if (!at_least_one_bit_left(btr)) {
		BT_LOGV("Reached end of data: btr-addr=%p", btr);
		status = BT_BTR_STATUS_EOF;
		goto end;
	}

	field_size = get_basic_field_type_size(btr, btr->cur_basic_field_type);
	if (field_size < 1) {
		BT_LOGW("Cannot get basic field type's size: "
			"btr-addr=%p, ft-addr=%p",
			btr, btr->cur_basic_field_type);
		status = BT_BTR_STATUS_ERROR;
		goto end;
	}

	bo = bt_field_type_get_byte_order(btr->cur_basic_field_type);
	status = validate_contiguous_bo(btr, bo);
	if (status != BT_BTR_STATUS_OK) {
		/* validate_contiguous_bo() logs errors */
		goto end;
	}

	available = available_bits(btr);

	if (field_size <= available) {
		/* We have all the bits; decode and set now */
		BT_ASSERT(btr->buf.addr);
		status = read_basic_and_call_cb(btr, btr->buf.addr,
			buf_at_from_addr(btr));
		if (status != BT_BTR_STATUS_OK) {
			BT_LOGW("Cannot read basic field: "
				"btr-addr=%p, ft-addr=%p, status=%s",
				btr, btr->cur_basic_field_type,
				bt_btr_status_string(status));
			goto end;
		}

		consume_bits(btr, field_size);

		if (stack_empty(btr->stack)) {
			/* Root is a basic type */
			btr->state = BTR_STATE_DONE;
		} else {
			/* Go to next field */
			stack_top(btr->stack)->index++;
			btr->state = BTR_STATE_NEXT_FIELD;
			btr->last_bo = btr->cur_bo;
		}

		goto end;
	}

	/* We are here; it means we don't have enough data to decode this */
	BT_LOGV_STR("Not enough data to read the next basic field: setting stitch buffer.");
	stitch_set_from_remaining_buf(btr);
	btr->state = BTR_STATE_READ_BASIC_CONTINUE;
	status = BT_BTR_STATUS_EOF;

end:
	return status;
}

static inline
enum bt_btr_status read_basic_int_type_and_call_begin(
		struct bt_btr *btr)
{
	return read_basic_type_and_call_begin(btr, read_basic_int_and_call_cb);
}

static inline
enum bt_btr_status read_basic_int_type_and_call_continue(
		struct bt_btr *btr)
{
	return read_basic_type_and_call_continue(btr,
		read_basic_int_and_call_cb);
}

static inline
enum bt_btr_status read_basic_float_type_and_call_begin(
		struct bt_btr *btr)
{
	return read_basic_type_and_call_begin(btr,
		read_basic_float_and_call_cb);
}

static inline
enum bt_btr_status read_basic_float_type_and_call_continue(
		struct bt_btr *btr)
{
	return read_basic_type_and_call_continue(btr,
		read_basic_float_and_call_cb);
}

static inline
enum bt_btr_status read_basic_enum_type_and_call_begin(
		struct bt_btr *btr)
{
	return read_basic_type_and_call_begin(btr,
		read_basic_enum_and_call_cb);
}

static inline
enum bt_btr_status read_basic_enum_type_and_call_continue(
		struct bt_btr *btr)
{
	return read_basic_type_and_call_continue(btr,
		read_basic_enum_and_call_cb);
}

static inline
enum bt_btr_status read_basic_string_type_and_call(
		struct bt_btr *btr, bool begin)
{
	size_t buf_at_bytes;
	const uint8_t *result;
	size_t available_bytes;
	const uint8_t *first_chr;
	enum bt_btr_status status = BT_BTR_STATUS_OK;

	if (!at_least_one_bit_left(btr)) {
		BT_LOGV("Reached end of data: btr-addr=%p", btr);
		status = BT_BTR_STATUS_EOF;
		goto end;
	}

	BT_ASSERT(buf_at_from_addr(btr) % 8 == 0);
	available_bytes = BITS_TO_BYTES_FLOOR(available_bits(btr));
	buf_at_bytes = BITS_TO_BYTES_FLOOR(buf_at_from_addr(btr));
	BT_ASSERT(btr->buf.addr);
	first_chr = &btr->buf.addr[buf_at_bytes];
	result = memchr(first_chr, '\0', available_bytes);

	if (begin && btr->user.cbs.types.string_begin) {
		BT_LOGV("Calling user function (string, beginning).");
		status = btr->user.cbs.types.string_begin(
			btr->cur_basic_field_type, btr->user.data);
		BT_LOGV("User function returned: status=%s",
			bt_btr_status_string(status));
		if (status != BT_BTR_STATUS_OK) {
			BT_LOGW("User function failed: btr-addr=%p, status=%s",
				btr, bt_btr_status_string(status));
			goto end;
		}
	}

	if (!result) {
		/* No null character yet */
		if (btr->user.cbs.types.string) {
			BT_LOGV("Calling user function (substring).");
			status = btr->user.cbs.types.string(
				(const char *) first_chr,
				available_bytes, btr->cur_basic_field_type,
				btr->user.data);
			BT_LOGV("User function returned: status=%s",
				bt_btr_status_string(status));
			if (status != BT_BTR_STATUS_OK) {
				BT_LOGW("User function failed: "
					"btr-addr=%p, status=%s",
					btr, bt_btr_status_string(status));
				goto end;
			}
		}

		consume_bits(btr, BYTES_TO_BITS(available_bytes));
		btr->state = BTR_STATE_READ_BASIC_CONTINUE;
		status = BT_BTR_STATUS_EOF;
	} else {
		/* Found the null character */
		size_t result_len = (size_t) (result - first_chr);

		if (btr->user.cbs.types.string && result_len) {
			BT_LOGV("Calling user function (substring).");
			status = btr->user.cbs.types.string(
				(const char *) first_chr,
				result_len, btr->cur_basic_field_type,
				btr->user.data);
			BT_LOGV("User function returned: status=%s",
				bt_btr_status_string(status));
			if (status != BT_BTR_STATUS_OK) {
				BT_LOGW("User function failed: "
					"btr-addr=%p, status=%s",
					btr, bt_btr_status_string(status));
				goto end;
			}
		}

		if (btr->user.cbs.types.string_end) {
			BT_LOGV("Calling user function (string, end).");
			status = btr->user.cbs.types.string_end(
				btr->cur_basic_field_type, btr->user.data);
			BT_LOGV("User function returned: status=%s",
				bt_btr_status_string(status));
			if (status != BT_BTR_STATUS_OK) {
				BT_LOGW("User function failed: "
					"btr-addr=%p, status=%s",
					btr, bt_btr_status_string(status));
				goto end;
			}
		}

		consume_bits(btr, BYTES_TO_BITS(result_len + 1));

		if (stack_empty(btr->stack)) {
			/* Root is a basic type */
			btr->state = BTR_STATE_DONE;
		} else {
			/* Go to next field */
			stack_top(btr->stack)->index++;
			btr->state = BTR_STATE_NEXT_FIELD;
			btr->last_bo = btr->cur_bo;
		}
	}

end:
	return status;
}

static inline
enum bt_btr_status read_basic_begin_state(struct bt_btr *btr)
{
	enum bt_btr_status status;

	BT_ASSERT(btr->cur_basic_field_type);

	switch (bt_field_type_get_type_id(btr->cur_basic_field_type)) {
	case BT_FIELD_TYPE_ID_INTEGER:
		status = read_basic_int_type_and_call_begin(btr);
		break;
	case BT_FIELD_TYPE_ID_FLOAT:
		status = read_basic_float_type_and_call_begin(btr);
		break;
	case BT_FIELD_TYPE_ID_ENUM:
		status = read_basic_enum_type_and_call_begin(btr);
		break;
	case BT_FIELD_TYPE_ID_STRING:
		status = read_basic_string_type_and_call(btr, true);
		break;
	default:
		BT_LOGF("Unknown basic field type ID: "
			"btr-addr=%p, ft-addr=%p, ft-id=%s",
			btr, btr->cur_basic_field_type,
			bt_field_type_id_string(
				bt_field_type_get_type_id(
					btr->cur_basic_field_type)));
		abort();
	}

	return status;
}

static inline
enum bt_btr_status read_basic_continue_state(struct bt_btr *btr)
{
	enum bt_btr_status status;

	BT_ASSERT(btr->cur_basic_field_type);

	switch (bt_field_type_get_type_id(btr->cur_basic_field_type)) {
	case BT_FIELD_TYPE_ID_INTEGER:
		status = read_basic_int_type_and_call_continue(btr);
		break;
	case BT_FIELD_TYPE_ID_FLOAT:
		status = read_basic_float_type_and_call_continue(btr);
		break;
	case BT_FIELD_TYPE_ID_ENUM:
		status = read_basic_enum_type_and_call_continue(btr);
		break;
	case BT_FIELD_TYPE_ID_STRING:
		status = read_basic_string_type_and_call(btr, false);
		break;
	default:
		BT_LOGF("Unknown basic field type ID: "
			"btr-addr=%p, ft-addr=%p, ft-id=%s",
			btr, btr->cur_basic_field_type,
			bt_field_type_id_string(
				bt_field_type_get_type_id(
					btr->cur_basic_field_type)));
		abort();
	}

	return status;
}

static inline
size_t bits_to_skip_to_align_to(struct bt_btr *btr, size_t align)
{
	size_t aligned_packet_at;

	aligned_packet_at = ALIGN(packet_at(btr), align);
	return aligned_packet_at - packet_at(btr);
}

static inline
enum bt_btr_status align_type_state(struct bt_btr *btr,
		struct bt_field_type *field_type, enum btr_state next_state)
{
	int field_alignment;
	size_t skip_bits;
	enum bt_btr_status status = BT_BTR_STATUS_OK;

	/* Get field's alignment */
	field_alignment = bt_field_type_get_alignment(field_type);
	if (field_alignment < 0) {
		BT_LOGW("Cannot get field type's alignment: "
			"btr-addr=%p, ft-addr=%p, ft-id=%s",
			btr, field_type,
			bt_field_type_id_string(
				bt_field_type_get_type_id(field_type)));
		status = BT_BTR_STATUS_ERROR;
		goto end;
	}

	/*
	 * 0 means "undefined" for variants; what we really want is 1
	 * (always aligned)
	 */
	if (field_alignment == 0) {
		field_alignment = 1;
	}

	/* Compute how many bits we need to skip */
	skip_bits = bits_to_skip_to_align_to(btr, field_alignment);

	/* Nothing to skip? aligned */
	if (skip_bits == 0) {
		btr->state = next_state;
		goto end;
	}

	/* Make sure there's at least one bit left */
	if (!at_least_one_bit_left(btr)) {
		status = BT_BTR_STATUS_EOF;
		goto end;
	}

	/* Consume as many bits as possible in what's left */
	consume_bits(btr, MIN(available_bits(btr), skip_bits));

	/* Are we done now? */
	skip_bits = bits_to_skip_to_align_to(btr, field_alignment);
	if (skip_bits == 0) {
		/* Yes: go to next state */
		btr->state = next_state;
		goto end;
	} else {
		/* No: need more data */
		BT_LOGV("Reached end of data when aligning: btr-addr=%p", btr);
		status = BT_BTR_STATUS_EOF;
	}

end:
	return status;
}

static inline
bool is_compound_type(struct bt_field_type *field_type)
{
	enum bt_field_type_id id = bt_field_type_get_type_id(field_type);

	return id == BT_FIELD_TYPE_ID_STRUCT || id == BT_FIELD_TYPE_ID_ARRAY ||
		id == BT_FIELD_TYPE_ID_SEQUENCE || id == BT_FIELD_TYPE_ID_VARIANT;
}

static inline
enum bt_btr_status next_field_state(struct bt_btr *btr)
{
	int ret;
	struct stack_entry *top;
	struct bt_field_type *next_field_type = NULL;
	enum bt_btr_status status = BT_BTR_STATUS_OK;

	if (stack_empty(btr->stack)) {
		goto end;
	}

	top = stack_top(btr->stack);

	/* Are we done with this base type? */
	while (top->index == top->base_len) {
		if (btr->user.cbs.types.compound_end) {
			BT_LOGV("Calling user function (compound, end).");
			status = btr->user.cbs.types.compound_end(
				top->base_type, btr->user.data);
			BT_LOGV("User function returned: status=%s",
				bt_btr_status_string(status));
			if (status != BT_BTR_STATUS_OK) {
				BT_LOGW("User function failed: btr-addr=%p, status=%s",
					btr, bt_btr_status_string(status));
				goto end;
			}
		}

		stack_pop(btr->stack);

		/* Are we done with the root type? */
		if (stack_empty(btr->stack)) {
			btr->state = BTR_STATE_DONE;
			goto end;
		}

		top = stack_top(btr->stack);
		top->index++;
	}

	/* Get next field's type */
	switch (bt_field_type_get_type_id(top->base_type)) {
	case BT_FIELD_TYPE_ID_STRUCT:
		ret = bt_field_type_structure_get_field_by_index(
			top->base_type, NULL, &next_field_type,
			top->index);
		if (ret) {
			next_field_type = NULL;
		}
		break;
	case BT_FIELD_TYPE_ID_ARRAY:
		next_field_type =
			bt_field_type_array_get_element_type(
				top->base_type);
		break;
	case BT_FIELD_TYPE_ID_SEQUENCE:
		next_field_type =
			bt_field_type_sequence_get_element_type(
				top->base_type);
		break;
	case BT_FIELD_TYPE_ID_VARIANT:
		/* Variant types are dynamic: query the user, he should know! */
		next_field_type =
			btr->user.cbs.query.get_variant_type(
				top->base_type, btr->user.data);
		break;
	default:
		break;
	}

	if (!next_field_type) {
		BT_LOGW("Cannot get the field type of the next field: "
			"btr-addr=%p, base-ft-addr=%p, base-ft-id=%s, "
			"index=%" PRId64,
			btr, top->base_type,
			bt_field_type_id_string(
				bt_field_type_get_type_id(top->base_type)),
			top->index);
		status = BT_BTR_STATUS_ERROR;
		goto end;
	}

	if (is_compound_type(next_field_type)) {
		if (btr->user.cbs.types.compound_begin) {
			BT_LOGV("Calling user function (compound, begin).");
			status = btr->user.cbs.types.compound_begin(
				next_field_type, btr->user.data);
			BT_LOGV("User function returned: status=%s",
				bt_btr_status_string(status));
			if (status != BT_BTR_STATUS_OK) {
				BT_LOGW("User function failed: btr-addr=%p, status=%s",
					btr, bt_btr_status_string(status));
				goto end;
			}
		}

		ret = stack_push_with_len(btr, next_field_type);
		if (ret) {
			/* stack_push_with_len() logs errors */
			status = BT_BTR_STATUS_ERROR;
			goto end;
		}

		/* Next state: align a compound type */
		btr->state = BTR_STATE_ALIGN_COMPOUND;
	} else {
		/* Replace current basic field type */
		BT_LOGV("Replacing current basic field type: "
			"btr-addr=%p, cur-basic-ft-addr=%p, "
			"next-basic-ft-addr=%p",
			btr, btr->cur_basic_field_type, next_field_type);
		BT_MOVE(btr->cur_basic_field_type, next_field_type);

		/* Next state: align a basic type */
		btr->state = BTR_STATE_ALIGN_BASIC;
	}

end:
	BT_PUT(next_field_type);

	return status;
}

static inline
enum bt_btr_status handle_state(struct bt_btr *btr)
{
	enum bt_btr_status status = BT_BTR_STATUS_OK;

	BT_LOGV("Handling state: btr-addr=%p, state=%s",
		btr, btr_state_string(btr->state));

	switch (btr->state) {
	case BTR_STATE_NEXT_FIELD:
		status = next_field_state(btr);
		break;
	case BTR_STATE_ALIGN_BASIC:
		status = align_type_state(btr, btr->cur_basic_field_type,
			BTR_STATE_READ_BASIC_BEGIN);
		break;
	case BTR_STATE_ALIGN_COMPOUND:
		status = align_type_state(btr, stack_top(btr->stack)->base_type,
			BTR_STATE_NEXT_FIELD);
		break;
	case BTR_STATE_READ_BASIC_BEGIN:
		status = read_basic_begin_state(btr);
		break;
	case BTR_STATE_READ_BASIC_CONTINUE:
		status = read_basic_continue_state(btr);
		break;
	case BTR_STATE_DONE:
		break;
	}

	BT_LOGV("Handled state: btr-addr=%p, status=%s",
		btr, bt_btr_status_string(status));
	return status;
}

struct bt_btr *bt_btr_create(struct bt_btr_cbs cbs, void *data)
{
	struct bt_btr *btr;

	BT_LOGD_STR("Creating binary type reader (BTR).");
	btr = g_new0(struct bt_btr, 1);
	if (!btr) {
		BT_LOGE_STR("Failed to allocate one binary type reader.");
		goto end;
	}

	btr->stack = stack_new();
	if (!btr->stack) {
		BT_LOGE_STR("Cannot create BTR's stack.");
		bt_btr_destroy(btr);
		btr = NULL;
		goto end;
	}

	btr->state = BTR_STATE_NEXT_FIELD;
	btr->user.cbs = cbs;
	btr->user.data = data;
	BT_LOGD("Created BTR: addr=%p", btr);

end:
	return btr;
}

void bt_btr_destroy(struct bt_btr *btr)
{
	if (btr->stack) {
		stack_destroy(btr->stack);
	}

	BT_LOGD("Destroying BTR: addr=%p", btr);
	BT_PUT(btr->cur_basic_field_type);
	g_free(btr);
}

static
void reset(struct bt_btr *btr)
{
	BT_LOGD("Resetting BTR: addr=%p", btr);
	stack_clear(btr->stack);
	BT_PUT(btr->cur_basic_field_type);
	stitch_reset(btr);
	btr->buf.addr = NULL;
	btr->last_bo = BT_BYTE_ORDER_UNKNOWN;
}

static
void update_packet_offset(struct bt_btr *btr)
{
	BT_LOGV("Updating packet offset for next call: "
		"btr-addr=%p, cur-packet-offset=%zu, next-packet-offset=%zu",
		btr, btr->buf.packet_offset,
		btr->buf.packet_offset + btr->buf.at);
	btr->buf.packet_offset += btr->buf.at;
}

size_t bt_btr_start(struct bt_btr *btr,
	struct bt_field_type *type, const uint8_t *buf,
	size_t offset, size_t packet_offset, size_t sz,
	enum bt_btr_status *status)
{
	BT_ASSERT(btr);
	BT_ASSERT(BYTES_TO_BITS(sz) >= offset);
	reset(btr);
	btr->buf.addr = buf;
	btr->buf.offset = offset;
	btr->buf.at = 0;
	btr->buf.packet_offset = packet_offset;
	btr->buf.buf_sz = sz;
	btr->buf.sz = BYTES_TO_BITS(sz) - offset;
	*status = BT_BTR_STATUS_OK;

	BT_LOGV("Starting decoding: btr-addr=%p, ft-addr=%p, "
		"buf-addr=%p, buf-size=%zu, offset=%zu, "
		"packet-offset=%zu",
		btr, type, buf, sz, offset, packet_offset);

	/* Set root type */
	if (is_compound_type(type)) {
		/* Compound type: push on visit stack */
		int stack_ret;

		if (btr->user.cbs.types.compound_begin) {
			BT_LOGV("Calling user function (compound, begin).");
			*status = btr->user.cbs.types.compound_begin(
				type, btr->user.data);
			BT_LOGV("User function returned: status=%s",
				bt_btr_status_string(*status));
			if (*status != BT_BTR_STATUS_OK) {
				BT_LOGW("User function failed: btr-addr=%p, status=%s",
					btr, bt_btr_status_string(*status));
				goto end;
			}
		}

		stack_ret = stack_push_with_len(btr, type);
		if (stack_ret) {
			/* stack_push_with_len() logs errors */
			*status = BT_BTR_STATUS_ERROR;
			goto end;
		}

		btr->state = BTR_STATE_ALIGN_COMPOUND;
	} else {
		/* Basic type: set as current basic type */
		btr->cur_basic_field_type = type;
		bt_get(btr->cur_basic_field_type);
		btr->state = BTR_STATE_ALIGN_BASIC;
	}

	/* Run the machine! */
	BT_LOGV_STR("Running the state machine.");

	while (true) {
		*status = handle_state(btr);
		if (*status != BT_BTR_STATUS_OK ||
				btr->state == BTR_STATE_DONE) {
			break;
		}
	}

	/* Update packet offset for next time */
	update_packet_offset(btr);

end:
	return btr->buf.at;
}

size_t bt_btr_continue(struct bt_btr *btr,
	const uint8_t *buf, size_t sz,
	enum bt_btr_status *status)
{
	BT_ASSERT(btr);
	BT_ASSERT(buf);
	BT_ASSERT(sz > 0);
	btr->buf.addr = buf;
	btr->buf.offset = 0;
	btr->buf.at = 0;
	btr->buf.buf_sz = sz;
	btr->buf.sz = BYTES_TO_BITS(sz);
	*status = BT_BTR_STATUS_OK;

	BT_LOGV("Continuing decoding: btr-addr=%p, buf-addr=%p, buf-size=%zu",
		btr, buf, sz);

	/* Continue running the machine */
	BT_LOGV_STR("Running the state machine.");

	while (true) {
		*status = handle_state(btr);
		if (*status != BT_BTR_STATUS_OK ||
				btr->state == BTR_STATE_DONE) {
			break;
		}
	}

	/* Update packet offset for next time */
	update_packet_offset(btr);
	return btr->buf.at;
}
