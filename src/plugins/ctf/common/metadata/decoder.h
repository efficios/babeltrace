#ifndef _METADATA_DECODER_H
#define _METADATA_DECODER_H

/*
 * Copyright 2016-2017 - Philippe Proulx <pproulx@efficios.com>
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

#include <stdint.h>
#include <stdbool.h>

#include <babeltrace2/babeltrace.h>

#include "common/macros.h"
#include "common/uuid.h"

struct ctf_trace_class;

/* A CTF metadata decoder object */
struct ctf_metadata_decoder;

/* CTF metadata decoder status */
enum ctf_metadata_decoder_status {
	CTF_METADATA_DECODER_STATUS_OK			= 0,
	CTF_METADATA_DECODER_STATUS_NONE		= 1,
	CTF_METADATA_DECODER_STATUS_ERROR		= -1,
	CTF_METADATA_DECODER_STATUS_INCOMPLETE		= -2,
	CTF_METADATA_DECODER_STATUS_INVAL_VERSION	= -3,
	CTF_METADATA_DECODER_STATUS_IR_VISITOR_ERROR	= -4,
};

/* Decoding configuration */
struct ctf_metadata_decoder_config {
	/* Active log level to use */
	bt_logging_level log_level;

	/* Component to use for logging (can be `NULL`); weak */
	bt_self_component *self_comp;

	/* Additional clock class offset to apply */
	int64_t clock_class_offset_s;
	int64_t clock_class_offset_ns;
	bool force_clock_class_origin_unix_epoch;

	/* True to create trace class objects */
	bool create_trace_class;

	/*
	 * True to keep the plain text when content is appended with
	 * ctf_metadata_decoder_append_content().
	 */
	bool keep_plain_text;
};

/*
 * Creates a CTF metadata decoder.
 *
 * Returns `NULL` on error.
 */
BT_HIDDEN
struct ctf_metadata_decoder *ctf_metadata_decoder_create(
		const struct ctf_metadata_decoder_config *config);

/*
 * Destroys a CTF metadata decoder that you created with
 * ctf_metadata_decoder_create().
 */
BT_HIDDEN
void ctf_metadata_decoder_destroy(
		struct ctf_metadata_decoder *metadata_decoder);

/*
 * Appends content to the metadata decoder.
 *
 * This function reads the metadata from the current position of `fp`
 * until the end of this file stream.
 *
 * The metadata can be packetized or not.
 *
 * The metadata chunk needs to be complete and lexically scannable, that
 * is, zero or more complete top-level blocks. If it's incomplete, this
 * function returns `CTF_METADATA_DECODER_STATUS_INCOMPLETE`. If this
 * function returns `CTF_METADATA_DECODER_STATUS_INCOMPLETE`, then you
 * need to call it again with the _same_ metadata and more to make it
 * complete. For example:
 *
 *     First call:  event { name = hell
 *     Second call: event { name = hello_world; ... };
 *
 * If everything goes as expected, this function returns
 * `CTF_METADATA_DECODER_STATUS_OK`.
 */
BT_HIDDEN
enum ctf_metadata_decoder_status ctf_metadata_decoder_append_content(
		struct ctf_metadata_decoder *metadata_decoder, FILE *fp);

/*
 * Returns the trace IR trace class of this metadata decoder (new
 * reference).
 *
 * Returns `NULL` if there's none yet or if the metadata decoder is not
 * configured to create trace classes.
 */
BT_HIDDEN
bt_trace_class *ctf_metadata_decoder_get_ir_trace_class(
		struct ctf_metadata_decoder *mdec);

/*
 * Returns the CTF IR trace class of this metadata decoder.
 *
 * Returns `NULL` if there's none yet or if the metadata decoder is not
 * configured to create trace classes.
 */
BT_HIDDEN
struct ctf_trace_class *ctf_metadata_decoder_borrow_ctf_trace_class(
		struct ctf_metadata_decoder *mdec);

/*
 * Checks whether or not a given metadata file stream `fp` is
 * packetized, setting `is_packetized` accordingly on success. On
 * success, also sets `*byte_order` to the byte order of the first
 * packet.
 *
 * This function uses `log_level` and `self_comp` for logging purposes.
 * `self_comp` can be `NULL` if not available.
 */
BT_HIDDEN
int ctf_metadata_decoder_is_packetized(FILE *fp, bool *is_packetized,
		int *byte_order, bt_logging_level log_level,
		bt_self_component *self_comp);

/*
 * Returns the byte order of the decoder's metadata stream as set by the
 * last call to ctf_metadata_decoder_append_content().
 *
 * Returns -1 if unknown (plain text content).
 */
BT_HIDDEN
int ctf_metadata_decoder_get_byte_order(struct ctf_metadata_decoder *mdec);

/*
 * Returns the UUID of the decoder's metadata stream as set by the last
 * call to ctf_metadata_decoder_append_content().
 */
BT_HIDDEN
int ctf_metadata_decoder_get_uuid(
		struct ctf_metadata_decoder *mdec, bt_uuid_t uuid);

/*
 * Returns the UUID of the decoder's trace class, if available.
 *
 * Returns:
 *
 * * `CTF_METADATA_DECODER_STATUS_OK`: success.
 * * `CTF_METADATA_DECODER_STATUS_NONE`: no UUID.
 * * `CTF_METADATA_DECODER_STATUS_INCOMPLETE`: missing metadata content.
 */
BT_HIDDEN
enum ctf_metadata_decoder_status ctf_metadata_decoder_get_trace_class_uuid(
		struct ctf_metadata_decoder *mdec, bt_uuid_t uuid);

/*
 * Returns the metadata decoder's current metadata text.
 */
BT_HIDDEN
const char *ctf_metadata_decoder_get_text(struct ctf_metadata_decoder *mdec);

static inline
bool ctf_metadata_decoder_is_packet_version_valid(unsigned int major,
		unsigned int minor)
{
	return major == 1 && minor == 8;
}

#endif /* _METADATA_DECODER_H */
