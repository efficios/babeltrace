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

#define BT_LOG_TAG "PLUGIN-CTF-METADATA-DECODER"
#include "logging.h"

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <inttypes.h>
#include <babeltrace/assert-internal.h>
#include <babeltrace/compat/uuid-internal.h>
#include <babeltrace/compat/memstream-internal.h>
#include <babeltrace/babeltrace.h>
#include <glib.h>
#include <string.h>

#include "ast.h"
#include "decoder.h"
#include "scanner.h"

#define TSDL_MAGIC	0x75d11d57

extern
int yydebug;

struct ctf_metadata_decoder {
	struct ctf_visitor_generate_ir *visitor;
	uint8_t uuid[16];
	bool is_uuid_set;
	int bo;
	struct ctf_metadata_decoder_config config;
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
		BT_LOGD_STR("Cannot reade first metadata packet header: assuming the stream is not packetized.");
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
	const long offset = ftell(in_fp);

	if (offset < 0) {
		BT_LOGE_ERRNO("Failed to get current metadata file position",
			".");
		goto error;
	}
	BT_LOGV("Decoding metadata packet: mdec-addr=%p, offset=%ld",
		mdec, offset);
	readlen = fread(&header, sizeof(header), 1, in_fp);
	if (feof(in_fp) != 0) {
		BT_LOGV("Reached end of file: offset=%ld", ftell(in_fp));
		goto end;
	}
	if (readlen < 1) {
		BT_LOGV("Cannot decode metadata packet: offset=%ld", offset);
		goto error;
	}

	if (byte_order != BYTE_ORDER) {
		header.magic = GUINT32_SWAP_LE_BE(header.magic);
		header.checksum = GUINT32_SWAP_LE_BE(header.checksum);
		header.content_size = GUINT32_SWAP_LE_BE(header.content_size);
		header.packet_size = GUINT32_SWAP_LE_BE(header.packet_size);
	}

	if (header.compression_scheme) {
		BT_LOGE("Metadata packet compression is not supported as of this version: "
			"compression-scheme=%u, offset=%ld",
			(unsigned int) header.compression_scheme, offset);
		goto error;
	}

	if (header.encryption_scheme) {
		BT_LOGE("Metadata packet encryption is not supported as of this version: "
			"encryption-scheme=%u, offset=%ld",
			(unsigned int) header.encryption_scheme, offset);
		goto error;
	}

	if (header.checksum || header.checksum_scheme) {
		BT_LOGE("Metadata packet checksum verification is not supported as of this version: "
			"checksum-scheme=%u, checksum=%x, offset=%ld",
			(unsigned int) header.checksum_scheme, header.checksum,
			offset);
		goto error;
	}

	if (!is_version_valid(header.major, header.minor)) {
		BT_LOGE("Invalid metadata packet version: "
			"version=%u.%u, offset=%ld",
			header.major, header.minor, offset);
		goto error;
	}

	/* Set expected trace UUID if not set; otherwise validate it */
	if (mdec) {
		if (!mdec->is_uuid_set) {
			memcpy(mdec->uuid, header.uuid, sizeof(header.uuid));
			mdec->is_uuid_set = true;
		} else if (bt_uuid_compare(header.uuid, mdec->uuid)) {
			BT_LOGE("Metadata UUID mismatch between packets of the same stream: "
				"packet-uuid=\"%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x\", "
				"expected-uuid=\"%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x\", "
				"offset=%ld",
				(unsigned int) header.uuid[0],
				(unsigned int) header.uuid[1],
				(unsigned int) header.uuid[2],
				(unsigned int) header.uuid[3],
				(unsigned int) header.uuid[4],
				(unsigned int) header.uuid[5],
				(unsigned int) header.uuid[6],
				(unsigned int) header.uuid[7],
				(unsigned int) header.uuid[8],
				(unsigned int) header.uuid[9],
				(unsigned int) header.uuid[10],
				(unsigned int) header.uuid[11],
				(unsigned int) header.uuid[12],
				(unsigned int) header.uuid[13],
				(unsigned int) header.uuid[14],
				(unsigned int) header.uuid[15],
				(unsigned int) mdec->uuid[0],
				(unsigned int) mdec->uuid[1],
				(unsigned int) mdec->uuid[2],
				(unsigned int) mdec->uuid[3],
				(unsigned int) mdec->uuid[4],
				(unsigned int) mdec->uuid[5],
				(unsigned int) mdec->uuid[6],
				(unsigned int) mdec->uuid[7],
				(unsigned int) mdec->uuid[8],
				(unsigned int) mdec->uuid[9],
				(unsigned int) mdec->uuid[10],
				(unsigned int) mdec->uuid[11],
				(unsigned int) mdec->uuid[12],
				(unsigned int) mdec->uuid[13],
				(unsigned int) mdec->uuid[14],
				(unsigned int) mdec->uuid[15],
				offset);
			goto error;
		}
	}

	if ((header.content_size / CHAR_BIT) < sizeof(header)) {
		BT_LOGE("Bad metadata packet content size: content-size=%u, "
			"offset=%ld", header.content_size, offset);
		goto error;
	}

	toread = header.content_size / CHAR_BIT - sizeof(header);

	for (;;) {
		size_t loop_read;

		loop_read = MIN(sizeof(buf) - 1, toread);
		readlen = fread(buf, sizeof(uint8_t), loop_read, in_fp);
		if (ferror(in_fp)) {
			BT_LOGE("Cannot read metadata packet buffer: "
				"offset=%ld, read-size=%zu",
				ftell(in_fp), loop_read);
			goto error;
		}
		if (readlen > loop_read) {
			BT_LOGE("fread returned more byte than expected: "
				"read-size-asked=%zu, read-size-returned=%zu",
				loop_read, readlen);
			goto error;
		}

		writelen = fwrite(buf, sizeof(uint8_t), readlen, out_fp);
		if (writelen < readlen || ferror(out_fp)) {
			BT_LOGE("Cannot write decoded metadata text to buffer: "
				"read-offset=%ld, write-size=%zu",
				ftell(in_fp), readlen);
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
				BT_LOGW_STR("Missing padding at the end of the metadata stream.");
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
		BT_LOGE("Cannot open memory stream: %s: mdec-addr=%p",
			strerror(errno), mdec);
		goto error;
	}

	for (;;) {
		if (feof(fp) != 0) {
			break;
		}

		tret = decode_packet(mdec, fp, out_fp, byte_order);
		if (tret) {
			BT_LOGE("Cannot decode packet: index=%zu, mdec-addr=%p",
				packet_index, mdec);
			goto error;
		}

		packet_index++;
	}

	/* Make sure the whole string ends with a null character */
	tret = fputc('\0', out_fp);
	if (tret == EOF) {
		BT_LOGE("Cannot append '\\0' to the decoded metadata buffer: "
			"mdec-addr=%p", mdec);
		goto error;
	}

	/* Close stream, which also flushes the buffer */
	ret = bt_close_memstream(buf, &size, out_fp);
	/*
	 * See fclose(3). Further access to out_fp after both success
	 * and error, even through another bt_close_memstream(), results
	 * in undefined behavior. Nullify out_fp to ensure we don't
	 * fclose it twice on error.
	 */
	out_fp = NULL;
	if (ret < 0) {
		BT_LOGE("Cannot close memory stream: %s: mdec-addr=%p",
			strerror(errno), mdec);
		goto error;
	}

	goto end;

error:
	ret = -1;

	if (out_fp) {
		if (bt_close_memstream(buf, &size, out_fp)) {
			BT_LOGE("Cannot close memory stream: %s: mdec-addr=%p",
				strerror(errno), mdec);
		}
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
struct ctf_metadata_decoder *ctf_metadata_decoder_create(
		const struct ctf_metadata_decoder_config *config,
		const char *name)
{
	struct ctf_metadata_decoder *mdec =
		g_new0(struct ctf_metadata_decoder, 1);
	struct ctf_metadata_decoder_config default_config = {
		.clock_class_offset_s = 0,
		.clock_class_offset_ns = 0,
	};

	if (!config) {
		config = &default_config;
	}

	BT_LOGD("Creating CTF metadata decoder: "
		"clock-class-offset-s=%" PRId64 ", "
		"clock-class-offset-ns=%" PRId64 ", name=\"%s\"",
		config->clock_class_offset_s, config->clock_class_offset_ns,
		name);

	if (!mdec) {
		BT_LOGE_STR("Failed to allocate one CTF metadata decoder.");
		goto end;
	}

	mdec->config = *config;
	mdec->visitor = ctf_visitor_generate_ir_create(config, name);
	if (!mdec->visitor) {
		BT_LOGE("Failed to create a CTF IR metadata AST visitor: "
			"mdec-addr=%p", mdec);
		ctf_metadata_decoder_destroy(mdec);
		mdec = NULL;
		goto end;
	}

	BT_LOGD("Creating CTF metadata decoder: "
		"clock-class-offset-s=%" PRId64 ", "
		"clock-class-offset-ns=%" PRId64 ", "
		"name=\"%s\", addr=%p",
		config->clock_class_offset_s, config->clock_class_offset_ns,
		name, mdec);

end:
	return mdec;
}

BT_HIDDEN
void ctf_metadata_decoder_destroy(struct ctf_metadata_decoder *mdec)
{
	if (!mdec) {
		return;
	}

	BT_LOGD("Destroying CTF metadata decoder: addr=%p", mdec);
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

	BT_ASSERT(mdec);

	if (ctf_metadata_decoder_is_packetized(fp, &mdec->bo)) {
		BT_LOGD("Metadata stream is packetized: mdec-addr=%p", mdec);
		ret = ctf_metadata_decoder_packetized_file_stream_to_buf_with_mdec(
			mdec, fp, &buf, mdec->bo);
		if (ret) {
			BT_LOGE("Cannot decode packetized metadata packets to metadata text: "
				"mdec-addr=%p, ret=%d", mdec, ret);
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
			BT_LOGE("Cannot memory-open metadata buffer: %s: "
				"mdec-addr=%p", strerror(errno), mdec);
			status = CTF_METADATA_DECODER_STATUS_ERROR;
			goto end;
		}
	} else {
		unsigned int major, minor;
		ssize_t nr_items;
		const long init_pos = ftell(fp);

		BT_LOGD("Metadata stream is plain text: mdec-addr=%p", mdec);

		if (init_pos < 0) {
			BT_LOGE_ERRNO("Failed to get current file position", ".");
			status = CTF_METADATA_DECODER_STATUS_ERROR;
			goto end;
		}

		/* Check text-only metadata header and version */
		nr_items = fscanf(fp, "/* CTF %10u.%10u", &major, &minor);
		if (nr_items < 2) {
			BT_LOGW("Missing \"/* CTF major.minor\" signature in plain text metadata file stream: "
				"mdec-addr=%p", mdec);
		}

		BT_LOGD("Found metadata stream version in signature: version=%u.%u", major, minor);

		if (!is_version_valid(major, minor)) {
			BT_LOGE("Invalid metadata version found in plain text signature: "
				"version=%u.%u, mdec-addr=%p", major, minor,
				mdec);
			status = CTF_METADATA_DECODER_STATUS_INVAL_VERSION;
			goto end;
		}

		if (fseek(fp, init_pos, SEEK_SET)) {
			BT_LOGE("Cannot seek metadata file stream to initial position: %s: "
				"mdec-addr=%p", strerror(errno), mdec);
			status = CTF_METADATA_DECODER_STATUS_ERROR;
			goto end;
		}
	}

	if (BT_LOG_ON_VERBOSE) {
		yydebug = 1;
	}

	/* Allocate a scanner and append the metadata text content */
	scanner = ctf_scanner_alloc();
	if (!scanner) {
		BT_LOGE("Cannot allocate a metadata lexical scanner: "
			"mdec-addr=%p", mdec);
		status = CTF_METADATA_DECODER_STATUS_ERROR;
		goto end;
	}

	BT_ASSERT(fp);
	ret = ctf_scanner_append_ast(scanner, fp);
	if (ret) {
		BT_LOGE("Cannot create the metadata AST out of the metadata text: "
			"mdec-addr=%p", mdec);
		status = CTF_METADATA_DECODER_STATUS_INCOMPLETE;
		goto end;
	}

	ret = ctf_visitor_semantic_check(0, &scanner->ast->root);
	if (ret) {
		BT_LOGE("Validation of the metadata semantics failed: "
			"mdec-addr=%p", mdec);
		status = CTF_METADATA_DECODER_STATUS_ERROR;
		goto end;
	}

	ret = ctf_visitor_generate_ir_visit_node(mdec->visitor,
		&scanner->ast->root);
	// TODO
	ret = -1;
	goto end;

	switch (ret) {
	case 0:
		/* Success */
		break;
	case -EINCOMPLETE:
		BT_LOGD("While visiting metadata AST: incomplete data: "
			"mdec-addr=%p", mdec);
		status = CTF_METADATA_DECODER_STATUS_INCOMPLETE;
		goto end;
	default:
		BT_LOGE("Failed to visit AST node to create CTF IR objects: "
			"mdec-addr=%p, ret=%d", mdec, ret);
		status = CTF_METADATA_DECODER_STATUS_IR_VISITOR_ERROR;
		goto end;
	}

end:
	if (scanner) {
		ctf_scanner_free(scanner);
	}

	yydebug = 0;

	if (fp && close_fp) {
		if (fclose(fp)) {
			BT_LOGE("Cannot close metadata file stream: "
				"mdec-addr=%p", mdec);
		}
	}

	if (buf) {
		free(buf);
	}

	return status;
}

BT_HIDDEN
struct bt_trace *ctf_metadata_decoder_get_ir_trace(
		struct ctf_metadata_decoder *mdec)
{
	return ctf_visitor_generate_ir_get_ir_trace(mdec->visitor);
}

BT_HIDDEN
struct ctf_trace_class *ctf_metadata_decoder_borrow_ctf_trace_class(
		struct ctf_metadata_decoder *mdec)
{
	return ctf_visitor_generate_ir_borrow_ctf_trace_class(mdec->visitor);
}
