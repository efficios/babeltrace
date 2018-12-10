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

#include <babeltrace/babeltrace.h>

/* A CTF metadata decoder object */
struct ctf_metadata_decoder;

/* CTF metadata decoder status */
enum ctf_metadata_decoder_status {
	CTF_METADATA_DECODER_STATUS_OK			= 0,
	CTF_METADATA_DECODER_STATUS_ERROR		= -1,
	CTF_METADATA_DECODER_STATUS_INCOMPLETE		= -2,
	CTF_METADATA_DECODER_STATUS_INVAL_VERSION	= -3,
	CTF_METADATA_DECODER_STATUS_IR_VISITOR_ERROR	= -4,
};

/* Decoding configuration */
struct ctf_metadata_decoder_config {
	int64_t clock_class_offset_s;
	int64_t clock_class_offset_ns;
};

/*
 * Creates a CTF metadata decoder.
 *
 * Returns `NULL` on error.
 */
BT_HIDDEN
struct ctf_metadata_decoder *ctf_metadata_decoder_create(
		bt_self_component_source *self_comp,
		const struct ctf_metadata_decoder_config *config);

/*
 * Destroys a CTF metadata decoder that you created with
 * ctf_metadata_decoder_create().
 */
BT_HIDDEN
void ctf_metadata_decoder_destroy(
		struct ctf_metadata_decoder *metadata_decoder);

/*
 * Decodes a new chunk of CTF metadata.
 *
 * This function reads the metadata from the current position of `fp`
 * until the end of this file stream. If it finds new information (new
 * event class, new stream class, or new clock class), it appends this
 * information to the decoder's trace object (as returned by
 * ctf_metadata_decoder_get_ir_trace_class()), or it creates this trace.
 *
 * The metadata can be packetized or not.
 *
 * The metadata chunk needs to be complete and scannable, that is,
 * zero or more complete top-level blocks. If it's incomplete, this
 * function returns `CTF_METADATA_DECODER_STATUS_INCOMPLETE`. If this
 * function returns `CTF_METADATA_DECODER_STATUS_INCOMPLETE`, then you
 * need to call it again with the same metadata and more to make it
 * complete. For example:
 *
 *     First call:  event { name = hell
 *     Second call: event { name = hello_world; ... };
 *
 * If the conversion from the metadata text to CTF IR objects fails,
 * this function returns `CTF_METADATA_DECODER_STATUS_IR_VISITOR_ERROR`.
 *
 * If everything goes as expected, this function returns
 * `CTF_METADATA_DECODER_STATUS_OK`.
 */
BT_HIDDEN
enum ctf_metadata_decoder_status ctf_metadata_decoder_decode(
		struct ctf_metadata_decoder *metadata_decoder, FILE *fp);

BT_HIDDEN
bt_trace_class *ctf_metadata_decoder_get_ir_trace_class(
		struct ctf_metadata_decoder *mdec);

BT_HIDDEN
struct ctf_trace_class *ctf_metadata_decoder_borrow_ctf_trace_class(
		struct ctf_metadata_decoder *mdec);

/*
 * Checks whether or not a given metadata file stream is packetized, and
 * if so, sets `*byte_order` to the byte order of the first packet.
 */
BT_HIDDEN
bool ctf_metadata_decoder_is_packetized(FILE *fp, int *byte_order);

/*
 * Decodes a packetized metadata file stream to a NULL-terminated
 * text buffer using the given byte order.
 */
BT_HIDDEN
int ctf_metadata_decoder_packetized_file_stream_to_buf(
		FILE *fp, char **buf, int byte_order);

#endif /* _METADATA_DECODER_H */
