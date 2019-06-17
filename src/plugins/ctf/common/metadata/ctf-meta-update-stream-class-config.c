/*
 * Copyright 2019 - Philippe Proulx <pproulx@efficios.com>
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

#include <babeltrace2/babeltrace.h>
#include "common/macros.h"
#include "common/assert.h"
#include <glib.h>
#include <stdint.h>
#include <string.h>
#include <inttypes.h>

#include "ctf-meta-visitors.h"

BT_HIDDEN
int ctf_trace_class_update_stream_class_config(struct ctf_trace_class *ctf_tc)
{
	struct ctf_field_class_int *int_fc;
	uint64_t i;

	for (i = 0; i < ctf_tc->stream_classes->len; i++) {
		struct ctf_stream_class *sc =
			ctf_tc->stream_classes->pdata[i];

		if (sc->is_translated) {
			continue;
		}

		if (!sc->packet_context_fc) {
			continue;
		}

		int_fc = ctf_field_class_struct_borrow_member_int_field_class_by_name(
			(void *) sc->packet_context_fc, "timestamp_begin");
		if (int_fc && int_fc->meaning ==
				CTF_FIELD_CLASS_MEANING_PACKET_BEGINNING_TIME) {
			sc->packets_have_ts_begin = true;
		}

		int_fc = ctf_field_class_struct_borrow_member_int_field_class_by_name(
			(void *) sc->packet_context_fc, "timestamp_end");
		if (int_fc && int_fc->meaning ==
				CTF_FIELD_CLASS_MEANING_PACKET_END_TIME) {
			sc->packets_have_ts_end = true;
		}

		int_fc = ctf_field_class_struct_borrow_member_int_field_class_by_name(
			(void *) sc->packet_context_fc, "events_discarded");
		if (int_fc && int_fc->meaning ==
				CTF_FIELD_CLASS_MEANING_DISC_EV_REC_COUNTER_SNAPSHOT) {
			sc->has_discarded_events = true;
		}

		sc->discarded_events_have_default_cs =
			sc->has_discarded_events && sc->packets_have_ts_begin &&
			sc->packets_have_ts_end;
		int_fc = ctf_field_class_struct_borrow_member_int_field_class_by_name(
			(void *) sc->packet_context_fc, "packet_seq_num");
		if (int_fc && int_fc->meaning ==
				CTF_FIELD_CLASS_MEANING_PACKET_COUNTER_SNAPSHOT) {
			sc->has_discarded_packets = true;
		}

		sc->discarded_packets_have_default_cs =
			sc->has_discarded_packets &&
			sc->packets_have_ts_begin && sc->packets_have_ts_end;
	}

	return 0;
}
