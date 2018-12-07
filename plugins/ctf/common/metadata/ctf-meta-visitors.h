#ifndef _CTF_META_VISITORS_H
#define _CTF_META_VISITORS_H

/*
 * Copyright 2018 - Philippe Proulx <pproulx@efficios.com>
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

#include <babeltrace/babeltrace.h>
#include <babeltrace/babeltrace-internal.h>

#include "ctf-meta.h"

BT_HIDDEN
int ctf_trace_class_resolve_field_classes(struct ctf_trace_class *tc);

BT_HIDDEN
int ctf_trace_class_translate(struct bt_trace_class *ir_tc,
		struct ctf_trace_class *tc);

BT_HIDDEN
int ctf_trace_class_update_default_clock_classes(
		struct ctf_trace_class *ctf_tc);

BT_HIDDEN
int ctf_trace_class_update_in_ir(struct ctf_trace_class *ctf_tc);

BT_HIDDEN
int ctf_trace_class_update_meanings(struct ctf_trace_class *ctf_tc);

BT_HIDDEN
int ctf_trace_class_update_text_array_sequence(struct ctf_trace_class *ctf_tc);

BT_HIDDEN
int ctf_trace_class_update_value_storing_indexes(struct ctf_trace_class *ctf_tc);

BT_HIDDEN
int ctf_trace_class_validate(struct ctf_trace_class *ctf_tc);

#endif /* _CTF_META_VISITORS_H */
