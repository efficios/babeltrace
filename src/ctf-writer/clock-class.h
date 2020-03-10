/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2013, 2014 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 */

#ifndef BABELTRACE_CTF_WRITER_CLOCK_CLASS_INTERNAL_H
#define BABELTRACE_CTF_WRITER_CLOCK_CLASS_INTERNAL_H

#include "common/macros.h"
#include "object-pool.h"
#include "common/uuid.h"
#include <babeltrace2-ctf-writer/types.h>
#include <stdint.h>
#include <glib.h>

#include "object.h"

struct bt_ctf_clock_class {
	struct bt_ctf_object base;
	GString *name;
	GString *description;
	uint64_t frequency;
	uint64_t precision;
	int64_t offset_s;	/* Offset in seconds */
	int64_t offset;		/* Offset in ticks */
	bt_uuid_t uuid;
	int uuid_set;
	int absolute;

	/*
	 * A clock's properties can't be modified once it is added to a stream
	 * class.
	 */
	int frozen;
};

BT_HIDDEN
void bt_ctf_clock_class_freeze(struct bt_ctf_clock_class *clock_class);

BT_HIDDEN
bt_ctf_bool bt_ctf_clock_class_is_valid(struct bt_ctf_clock_class *clock_class);

BT_HIDDEN
int bt_ctf_clock_class_compare(struct bt_ctf_clock_class *clock_class_a,
		struct bt_ctf_clock_class *clock_class_b);

BT_HIDDEN
struct bt_ctf_clock_class *bt_ctf_clock_class_create(const char *name,
		uint64_t freq);
BT_HIDDEN
const char *bt_ctf_clock_class_get_name(
		struct bt_ctf_clock_class *clock_class);
BT_HIDDEN
int bt_ctf_clock_class_set_name(struct bt_ctf_clock_class *clock_class,
		const char *name);
BT_HIDDEN
const char *bt_ctf_clock_class_get_description(
		struct bt_ctf_clock_class *clock_class);
BT_HIDDEN
int bt_ctf_clock_class_set_description(
		struct bt_ctf_clock_class *clock_class,
		const char *desc);
BT_HIDDEN
uint64_t bt_ctf_clock_class_get_frequency(
		struct bt_ctf_clock_class *clock_class);
BT_HIDDEN
int bt_ctf_clock_class_set_frequency(
		struct bt_ctf_clock_class *clock_class, uint64_t freq);
BT_HIDDEN
uint64_t bt_ctf_clock_class_get_precision(
		struct bt_ctf_clock_class *clock_class);
BT_HIDDEN
int bt_ctf_clock_class_set_precision(
		struct bt_ctf_clock_class *clock_class, uint64_t precision);
BT_HIDDEN
int bt_ctf_clock_class_get_offset_s(
		struct bt_ctf_clock_class *clock_class, int64_t *seconds);
BT_HIDDEN
int bt_ctf_clock_class_set_offset_s(
		struct bt_ctf_clock_class *clock_class, int64_t seconds);
BT_HIDDEN
int bt_ctf_clock_class_get_offset_cycles(
		struct bt_ctf_clock_class *clock_class, int64_t *cycles);
BT_HIDDEN
int bt_ctf_clock_class_set_offset_cycles(
		struct bt_ctf_clock_class *clock_class, int64_t cycles);
BT_HIDDEN
bt_ctf_bool bt_ctf_clock_class_is_absolute(
		struct bt_ctf_clock_class *clock_class);
BT_HIDDEN
int bt_ctf_clock_class_set_is_absolute(
		struct bt_ctf_clock_class *clock_class, bt_ctf_bool is_absolute);
BT_HIDDEN
const uint8_t *bt_ctf_clock_class_get_uuid(
		struct bt_ctf_clock_class *clock_class);
BT_HIDDEN
int bt_ctf_clock_class_set_uuid(struct bt_ctf_clock_class *clock_class,
		const uint8_t *uuid);

#endif /* BABELTRACE_CTF_WRITER_CLOCK_CLASS_INTERNAL_H */
