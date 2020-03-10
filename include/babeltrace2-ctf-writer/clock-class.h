/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (C) 2010-2019 EfficiOS Inc. and Linux Foundation
 */

#ifndef BABELTRACE2_CTF_WRITER_CLOCK_CLASS_H
#define BABELTRACE2_CTF_WRITER_CLOCK_CLASS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct bt_ctf_clock_class;

extern struct bt_ctf_clock_class *bt_ctf_clock_class_create(const char *name,
                uint64_t freq);

extern const char *bt_ctf_clock_class_get_name(
                struct bt_ctf_clock_class *clock_class);

extern int bt_ctf_clock_class_set_name(struct bt_ctf_clock_class *clock_class,
                const char *name);

extern const char *bt_ctf_clock_class_get_description(
                struct bt_ctf_clock_class *clock_class);

extern int bt_ctf_clock_class_set_description(
                struct bt_ctf_clock_class *clock_class,
                const char *desc);

extern uint64_t bt_ctf_clock_class_get_frequency(
                struct bt_ctf_clock_class *clock_class);

extern int bt_ctf_clock_class_set_frequency(
                struct bt_ctf_clock_class *clock_class, uint64_t freq);

extern uint64_t bt_ctf_clock_class_get_precision(
                struct bt_ctf_clock_class *clock_class);

extern int bt_ctf_clock_class_set_precision(
                struct bt_ctf_clock_class *clock_class, uint64_t precision);

extern int bt_ctf_clock_class_get_offset_s(
                struct bt_ctf_clock_class *clock_class, int64_t *seconds);

extern int bt_ctf_clock_class_set_offset_s(
                struct bt_ctf_clock_class *clock_class, int64_t seconds);

extern int bt_ctf_clock_class_get_offset_cycles(
                struct bt_ctf_clock_class *clock_class, int64_t *cycles);

extern int bt_ctf_clock_class_set_offset_cycles(
                struct bt_ctf_clock_class *clock_class, int64_t cycles);

extern bt_ctf_bool bt_ctf_clock_class_is_absolute(
                struct bt_ctf_clock_class *clock_class);

extern int bt_ctf_clock_class_set_is_absolute(
                struct bt_ctf_clock_class *clock_class, bt_ctf_bool is_absolute);

extern const uint8_t *bt_ctf_clock_class_get_uuid(
                struct bt_ctf_clock_class *clock_class);

extern int bt_ctf_clock_class_set_uuid(struct bt_ctf_clock_class *clock_class,
                const uint8_t *uuid);
#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE2_CTF_WRITER_CLOCK_CLASS_H */
