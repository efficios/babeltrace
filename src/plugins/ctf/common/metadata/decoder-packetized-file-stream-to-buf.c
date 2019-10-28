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

#define BT_COMP_LOG_SELF_COMP self_comp
#define BT_LOG_OUTPUT_LEVEL log_level
#define BT_LOG_TAG "PLUGIN/CTF/META/DECODER-DECODE-PACKET"
#include "logging/comp-logging.h"

#include "decoder-packetized-file-stream-to-buf.h"

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <inttypes.h>
#include "common/assert.h"
#include "common/uuid.h"
#include "compat/memstream.h"
#include <babeltrace2/babeltrace.h>
#include <glib.h>
#include <string.h>

#include "ast.h"
#include "decoder.h"
#include "scanner.h"
#include "logging.h"

#define TSDL_MAGIC	0x75d11d57

struct ctf_metadata_decoder {
	struct ctf_visitor_generate_ir *visitor;
	bt_uuid_t uuid;
	bool is_uuid_set;
	int bo;
	struct ctf_metadata_decoder_config config;
};

struct packet_header {
	uint32_t magic;
	bt_uuid_t  uuid;
	uint32_t checksum;
	uint32_t content_size;
	uint32_t packet_size;
	uint8_t  compression_scheme;
	uint8_t  encryption_scheme;
	uint8_t  checksum_scheme;
	uint8_t  major;
	uint8_t  minor;
} __attribute__((__packed__));

static
int decode_packet(FILE *in_fp, FILE *out_fp,
		int byte_order, bool *is_uuid_set, uint8_t *uuid,
		bt_logging_level log_level, bt_self_component *self_comp)
{
	struct packet_header header;
	size_t readlen, writelen, toread;
	uint8_t buf[512 + 1];	/* + 1 for debug-mode \0 */
	int ret = 0;
	const long offset = ftell(in_fp);

	if (offset < 0) {
		BT_COMP_LOGE_ERRNO("Failed to get current metadata file position",
			".");
		goto error;
	}
	BT_COMP_LOGD("Decoding metadata packet: offset=%ld", offset);
	readlen = fread(&header, sizeof(header), 1, in_fp);
	if (feof(in_fp) != 0) {
		BT_COMP_LOGI("Reached end of file: offset=%ld", ftell(in_fp));
		goto end;
	}
	if (readlen < 1) {
		BT_COMP_LOGE("Cannot decode metadata packet: offset=%ld", offset);
		goto error;
	}

	if (byte_order != BYTE_ORDER) {
		header.magic = GUINT32_SWAP_LE_BE(header.magic);
		header.checksum = GUINT32_SWAP_LE_BE(header.checksum);
		header.content_size = GUINT32_SWAP_LE_BE(header.content_size);
		header.packet_size = GUINT32_SWAP_LE_BE(header.packet_size);
	}

	if (header.compression_scheme) {
		BT_COMP_LOGE("Metadata packet compression is not supported as of this version: "
			"compression-scheme=%u, offset=%ld",
			(unsigned int) header.compression_scheme, offset);
		goto error;
	}

	if (header.encryption_scheme) {
		BT_COMP_LOGE("Metadata packet encryption is not supported as of this version: "
			"encryption-scheme=%u, offset=%ld",
			(unsigned int) header.encryption_scheme, offset);
		goto error;
	}

	if (header.checksum || header.checksum_scheme) {
		BT_COMP_LOGE("Metadata packet checksum verification is not supported as of this version: "
			"checksum-scheme=%u, checksum=%x, offset=%ld",
			(unsigned int) header.checksum_scheme, header.checksum,
			offset);
		goto error;
	}

	if (!ctf_metadata_decoder_is_packet_version_valid(header.major,
			header.minor)) {
		BT_COMP_LOGE("Invalid metadata packet version: "
			"version=%u.%u, offset=%ld",
			header.major, header.minor, offset);
		goto error;
	}

	/* Set expected trace UUID if not set; otherwise validate it */
	if (is_uuid_set) {
		if (!*is_uuid_set) {
			bt_uuid_copy(uuid, header.uuid);
			*is_uuid_set = true;
		} else if (bt_uuid_compare(header.uuid, uuid)) {
			BT_COMP_LOGE("Metadata UUID mismatch between packets of the same stream: "
				"packet-uuid=\"" BT_UUID_FMT "\", "
				"expected-uuid=\"" BT_UUID_FMT "\", "
				"offset=%ld",
				BT_UUID_FMT_VALUES(header.uuid),
				BT_UUID_FMT_VALUES(uuid),
				offset);
			goto error;
		}
	}

	if ((header.content_size / CHAR_BIT) < sizeof(header)) {
		BT_COMP_LOGE("Bad metadata packet content size: content-size=%u, "
			"offset=%ld", header.content_size, offset);
		goto error;
	}

	toread = header.content_size / CHAR_BIT - sizeof(header);

	for (;;) {
		size_t loop_read;

		loop_read = MIN(sizeof(buf) - 1, toread);
		readlen = fread(buf, sizeof(uint8_t), loop_read, in_fp);
		if (ferror(in_fp)) {
			BT_COMP_LOGE("Cannot read metadata packet buffer: "
				"offset=%ld, read-size=%zu",
				ftell(in_fp), loop_read);
			goto error;
		}
		if (readlen > loop_read) {
			BT_COMP_LOGE("fread returned more byte than expected: "
				"read-size-asked=%zu, read-size-returned=%zu",
				loop_read, readlen);
			goto error;
		}

		writelen = fwrite(buf, sizeof(uint8_t), readlen, out_fp);
		if (writelen < readlen || ferror(out_fp)) {
			BT_COMP_LOGE("Cannot write decoded metadata text to buffer: "
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
				BT_COMP_LOGW_STR("Missing padding at the end of the metadata stream.");
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

BT_HIDDEN
int ctf_metadata_decoder_packetized_file_stream_to_buf(FILE *fp,
		char **buf, int byte_order, bool *is_uuid_set,
		uint8_t *uuid, bt_logging_level log_level,
		bt_self_component *self_comp)
{
	FILE *out_fp;
	size_t size;
	int ret = 0;
	int tret;
	size_t packet_index = 0;

	out_fp = bt_open_memstream(buf, &size);
	if (!out_fp) {
		BT_COMP_LOGE("Cannot open memory stream: %s.",
			strerror(errno));
		goto error;
	}

	for (;;) {
		if (feof(fp) != 0) {
			break;
		}

		tret = decode_packet(fp, out_fp, byte_order, is_uuid_set,
			uuid, log_level, self_comp);
		if (tret) {
			BT_COMP_LOGE("Cannot decode packet: index=%zu",
				packet_index);
			goto error;
		}

		packet_index++;
	}

	/* Make sure the whole string ends with a null character */
	tret = fputc('\0', out_fp);
	if (tret == EOF) {
		BT_COMP_LOGE_STR(
			"Cannot append '\\0' to the decoded metadata buffer.");
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
		BT_COMP_LOGE_ERRNO("Cannot close memory stream", ".");
		goto error;
	}

	goto end;

error:
	ret = -1;

	if (out_fp) {
		if (bt_close_memstream(buf, &size, out_fp)) {
			BT_COMP_LOGE_ERRNO("Cannot close memory stream", ".");
		}
	}

	if (*buf) {
		free(*buf);
		*buf = NULL;
	}

end:
	return ret;
}
