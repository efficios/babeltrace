/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2024 EfficiOS, Inc.
 */

#ifndef CLOCK_CORRELATION_VALIDATOR_CLOCK_CORRELATION_VALIDATOR_H
#define CLOCK_CORRELATION_VALIDATOR_CLOCK_CORRELATION_VALIDATOR_H

#include <glib.h>
#include <stdbool.h>
#include <babeltrace2/babeltrace.h>

#include "common/macros.h"

#ifdef __cplusplus
extern "C" {
#endif

struct bt_clock_class;
struct bt_message;

enum bt_clock_correlation_validator_error_type
{
	BT_CLOCK_CORRELATION_VALIDATOR_ERROR_TYPE_EXPECTING_NO_CLOCK_CLASS_GOT_ONE,
	BT_CLOCK_CORRELATION_VALIDATOR_ERROR_TYPE_EXPECTING_ORIGIN_UNIX_GOT_NONE,
	BT_CLOCK_CORRELATION_VALIDATOR_ERROR_TYPE_EXPECTING_ORIGIN_UNIX_GOT_OTHER,
	BT_CLOCK_CORRELATION_VALIDATOR_ERROR_TYPE_EXPECTING_ORIGIN_UUID_GOT_NONE,
	BT_CLOCK_CORRELATION_VALIDATOR_ERROR_TYPE_EXPECTING_ORIGIN_UUID_GOT_UNIX,
	BT_CLOCK_CORRELATION_VALIDATOR_ERROR_TYPE_EXPECTING_ORIGIN_UUID_GOT_NO_UUID,
	BT_CLOCK_CORRELATION_VALIDATOR_ERROR_TYPE_EXPECTING_ORIGIN_UUID_GOT_OTHER_UUID,
	BT_CLOCK_CORRELATION_VALIDATOR_ERROR_TYPE_EXPECTING_ORIGIN_NO_UUID_GOT_NONE,
	BT_CLOCK_CORRELATION_VALIDATOR_ERROR_TYPE_EXPECTING_ORIGIN_NO_UUID_GOT_OTHER,
};

struct bt_clock_correlation_validator *bt_clock_correlation_validator_create(
	void) BT_NOEXCEPT;

bool bt_clock_correlation_validator_validate_message(
	struct bt_clock_correlation_validator *validator,
	const struct bt_message *msg,
	enum bt_clock_correlation_validator_error_type *type,
	bt_uuid *expected_uuid,
	const struct bt_clock_class ** const actual_clock_cls,
	const struct bt_clock_class ** const expected_clock_cls) BT_NOEXCEPT;

void bt_clock_correlation_validator_destroy(
	struct bt_clock_correlation_validator *validator) BT_NOEXCEPT;

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* CLOCK_CORRELATION_VALIDATOR_CLOCK_CORRELATION_VALIDATOR_H */
