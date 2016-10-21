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

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <babeltrace/compat/uuid-internal.h>
#include <babeltrace/compat/memstream-internal.h>
#include <babeltrace/ctf-ir/trace.h>
#include <glib.h>
#include <string.h>

#include "ast.h"
#include "decoder.h"
#include "scanner.h"

#define PRINT_ERR_STREAM	(mdec ? mdec->err_stream : stderr)
#define PRINT_PREFIX		"metadata-decoder"
#include "../print.h"

#define TSDL_MAGIC	0x75d11d57

struct ctf_metadata_decoder {
	struct ctf_visitor_generate_ir *visitor;
	FILE *err_stream;
	uint8_t uuid[16];
	bool is_uuid_set;
	int bo;
};

struct packet_header {
	uint32_t magic;
	uint8_t  uuid[16];
	uint32_t checksum;
	uint32_t content_size;
	uint32_t packet_size;
	uint8_t  compression_scheme;
	uint8_t  encryption_scheme;
	uint8_t  checksum_scheme;
	uint8_t  major;
	uint8_t  minor;
} __attribute__((__packed__));

BT_HIDDEN
bool ctf_metadata_decoder_is_packetized(FILE *fp, int *byte_order)
{
	uint32_t magic;
	size_t len;
	int ret = 0;

	len = fread(&magic, sizeof(magic), 1, fp);
	if (len != 1) {
		goto end;
	}

	if (byte_order) {
		if (magic == TSDL_MAGIC) {
			ret = 1;
			*byte_order = BYTE_ORDER;
		} else if (magic == GUINT32_SWAP_LE_BE(TSDL_MAGIC)) {
			ret = 1;
			*byte_order = BYTE_ORDER == BIG_ENDIAN ?
				LITTLE_ENDIAN : BIG_ENDIAN;
		}
	}

end:
	rewind(fp);

	return ret;
}

static
bool is_version_valid(unsigned int major, unsigned int minor)
{
	return major == 1 && minor == 8;
}

static
int decode_packet(struct ctf_metadata_decoder *mdec, FILE *in_fp, FILE *out_fp,
		int byte_order)
{
	struct packet_header header;
	size_t readlen, writelen, toread;
	uint8_t buf[512 + 1];	/* + 1 for debug-mode \0 */
	int ret = 0;

	readlen = fread(&header, sizeof(header), 1, in_fp);
	if (feof(in_fp) != 0) {
		goto end;
	}
	if (readlen < 1) {
		goto error;
	}

	if (byte_order != BYTE_ORDER) {
		header.magic = GUINT32_SWAP_LE_BE(header.magic);
		header.checksum = GUINT32_SWAP_LE_BE(header.checksum);
		header.content_size = GUINT32_SWAP_LE_BE(header.content_size);
		header.packet_size = GUINT32_SWAP_LE_BE(header.packet_size);
	}

	if (header.compression_scheme) {
		PERR("Metadata packet compression not supported yet\n");
		goto error;
	}

	if (header.encryption_scheme) {
		PERR("Metadata packet encryption not supported yet\n");
		goto error;
	}

	if (header.checksum || header.checksum_scheme) {
		PERR("Metadata packet checksum verification not supported yet\n");
		goto error;
	}

	if (!is_version_valid(header.major, header.minor)) {
		PERR("Invalid metadata version: %u.%u\n", header.major,
			header.minor);
		goto error;
	}

	/* Set expected trace UUID if not set; otherwise validate it */
	if (mdec) {
		if (!mdec->is_uuid_set) {
			memcpy(mdec->uuid, header.uuid, sizeof(header.uuid));
			mdec->is_uuid_set = true;
		} else if (bt_uuid_compare(header.uuid, mdec->uuid)) {
			PERR("Metadata UUID mismatch between packets of the same stream\n");
			goto error;
		}
	}

	if ((header.content_size / CHAR_BIT) < sizeof(header)) {
		PERR("Bad metadata packet content size: %u\n",
			header.content_size);
		goto error;
	}

	toread = header.content_size / CHAR_BIT - sizeof(header);

	for (;;) {
		readlen = fread(buf, sizeof(uint8_t),
			MIN(sizeof(buf) - 1, toread), in_fp);
		if (ferror(in_fp)) {
			PERR("Cannot read metadata packet buffer (at position %ld)\n",
				ftell(in_fp));
			goto error;
		}

		writelen = fwrite(buf, sizeof(uint8_t), readlen, out_fp);
		if (writelen < readlen || ferror(out_fp)) {
			PERR("Cannot write decoded metadata text to buffer\n");
			goto error;
		}

		toread -= readlen;
		if (toread == 0) {
			int fseek_ret;

			/* Read leftover padding */
			toread = (header.packet_size - header.content_size) /
				CHAR_BIT;
			fseek_ret = fseek(in_fp, toread, SEEK_CUR);
			if (fseek_ret < 0) {
				PWARN("Missing padding at the end of the metadata file\n");
			}

			break;
		}
	}

	goto end;

error:
	ret = -1;

end:
	return ret;
}

static
int ctf_metadata_decoder_packetized_file_stream_to_buf_with_mdec(
		struct ctf_metadata_decoder *mdec, FILE *fp,
		char **buf, int byte_order)
{
	FILE *out_fp;
	size_t size;
	int ret = 0;
	int tret;
	size_t packet_index = 0;

	out_fp = bt_open_memstream(buf, &size);
	if (out_fp == NULL) {
		PERR("Cannot open memory stream: %s\n", strerror(errno));
		goto error;
	}

	for (;;) {
		if (feof(fp) != 0) {
			break;
		}

		tret = decode_packet(mdec, fp, out_fp, byte_order);
		if (tret) {
			PERR("Cannot decode packet #%zu\n", packet_index);
			goto error;
		}

		packet_index++;
	}

	/* Make sure the whole string ends with a null character */
	tret = fputc('\0', out_fp);
	if (tret == EOF) {
		PERR("Cannot append '\\0' to the decoded metadata buffer\n");
		goto error;
	}

	/* Close stream, which also flushes the buffer */
	ret = bt_close_memstream(buf, &size, out_fp);
	if (ret < 0) {
		PERR("Cannot close memory stream: %s\n", strerror(errno));
		goto error;
	}

	goto end;

error:
	ret = -1;

	if (out_fp) {
		bt_close_memstream(buf, &size, out_fp);
	}

	if (*buf) {
		free(*buf);
		*buf = NULL;
	}

end:
	return ret;
}

BT_HIDDEN
int ctf_metadata_decoder_packetized_file_stream_to_buf(
		FILE *fp, char **buf, int byte_order)
{
	return ctf_metadata_decoder_packetized_file_stream_to_buf_with_mdec(
		NULL, fp, buf, byte_order);
}

BT_HIDDEN
struct ctf_metadata_decoder *ctf_metadata_decoder_create(FILE *err,
		uint64_t clock_class_offset_ns, const char *name)
{
	struct ctf_metadata_decoder *mdec =
		g_new0(struct ctf_metadata_decoder, 1);

	if (!mdec) {
		goto end;
	}

	mdec->err_stream = err;
	mdec->visitor = ctf_visitor_generate_ir_create(err,
		clock_class_offset_ns, name);
	if (!mdec->visitor) {
		ctf_metadata_decoder_destroy(mdec);
		mdec = NULL;
		goto end;
	}

end:
	return mdec;
}

BT_HIDDEN
void ctf_metadata_decoder_destroy(struct ctf_metadata_decoder *mdec)
{
	if (!mdec) {
		return;
	}

	ctf_visitor_generate_ir_destroy(mdec->visitor);
	g_free(mdec);
}

BT_HIDDEN
enum ctf_metadata_decoder_status ctf_metadata_decoder_decode(
		struct ctf_metadata_decoder *mdec, FILE *fp)
{
	enum ctf_metadata_decoder_status status =
		CTF_METADATA_DECODER_STATUS_OK;
	int ret;
	struct ctf_scanner *scanner = NULL;
	char *buf = NULL;
	bool close_fp = false;

	assert(mdec);

	if (ctf_metadata_decoder_is_packetized(fp, &mdec->bo)) {
		PDBG("Metadata stream is packetized\n");
		ret = ctf_metadata_decoder_packetized_file_stream_to_buf_with_mdec(
			mdec, fp, &buf, mdec->bo);
		if (ret) {
			// log: details
			status = CTF_METADATA_DECODER_STATUS_ERROR;
			goto end;
		}

		if (strlen(buf) == 0) {
			/* An empty metadata packet is OK. */
			goto end;
		}

		/* Convert the real file pointer to a memory file pointer */
		fp = bt_fmemopen(buf, strlen(buf), "rb");
		close_fp = true;
		if (!fp) {
			PERR("Cannot memory-open metadata buffer: %s\n",
				strerror(errno));
			status = CTF_METADATA_DECODER_STATUS_ERROR;
			goto end;
		}
	} else {
		unsigned int major, minor;
		ssize_t nr_items;
		const long init_pos = ftell(fp);

		PDBG("Metadata stream is plain text\n");

		/* Check text-only metadata header and version */
		nr_items = fscanf(fp, "/* CTF %10u.%10u", &major, &minor);
		if (nr_items < 2) {
			PWARN("Ill-shapen or missing \"/* CTF major.minor\" header in plain text metadata file stream\n");
		}

		PDBG("Metadata version: %u.%u\n", major, minor);

		if (!is_version_valid(major, minor)) {
			PERR("Invalid metadata version: %u.%u\n", major, minor);
			status = CTF_METADATA_DECODER_STATUS_INVAL_VERSION;
			goto end;
		}

		if (fseek(fp, init_pos, SEEK_SET)) {
			PERR("Cannot seek metadata file stream to initial position: %s\n",
				strerror(errno));
			status = CTF_METADATA_DECODER_STATUS_ERROR;
			goto end;
		}
	}

	/* Allocate a scanner and append the metadata text content */
	scanner = ctf_scanner_alloc();
	if (!scanner) {
		PERR("Cannot allocate a metadata lexical scanner\n");
		status = CTF_METADATA_DECODER_STATUS_ERROR;
		goto end;
	}

	assert(fp);
	ret = ctf_scanner_append_ast(scanner, fp);
	if (ret) {
		PERR("Cannot create the metadata AST\n");
		status = CTF_METADATA_DECODER_STATUS_INCOMPLETE;
		goto end;
	}

	ret = ctf_visitor_semantic_check(stderr, 0, &scanner->ast->root);
	if (ret) {
		PERR("Metadata semantic validation failed\n");
		status = CTF_METADATA_DECODER_STATUS_ERROR;
		goto end;
	}

	ret = ctf_visitor_generate_ir_visit_node(mdec->visitor,
		&scanner->ast->root);
	switch (ret) {
	case 0:
		/* Success */
		break;
	case -EINCOMPLETE:
		PDBG("While visiting AST: incomplete data\n");
		status = CTF_METADATA_DECODER_STATUS_INCOMPLETE;
		goto end;
	default:
		PERR("Cannot visit AST node to create CTF IR objects\n");
		status = CTF_METADATA_DECODER_STATUS_IR_VISITOR_ERROR;
		goto end;
	}

end:
	if (scanner) {
		ctf_scanner_free(scanner);
	}

	if (fp && close_fp) {
		if (fclose(fp)) {
			PERR("Cannot close metadata file stream\n");
		}
	}

	if (buf) {
		free(buf);
	}

	return status;
}

BT_HIDDEN
struct bt_ctf_trace *ctf_metadata_decoder_get_trace(
		struct ctf_metadata_decoder *mdec)
{
	return ctf_visitor_generate_ir_get_trace(mdec->visitor);
}
