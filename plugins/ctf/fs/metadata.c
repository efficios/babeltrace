/*
 * Copyright 2016 - Philippe Proulx <pproulx@efficios.com>
 * Copyright 2010-2011 - EfficiOS Inc. and Linux Foundation
 *
 * Some functions are based on older functions written by Mathieu Desnoyers.
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
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <glib.h>
#include <babeltrace/compat/uuid.h>
#include <babeltrace/compat/memstream.h>

#define PRINT_ERR_STREAM	ctf_fs->error_fp
#define PRINT_PREFIX		"ctf-fs-metadata"
#include "print.h"

#include "fs.h"
#include "file.h"
#include "metadata.h"
#include "../common/metadata/ast.h"
#include "../common/metadata/scanner.h"

#define TSDL_MAGIC	0x75d11d57

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
FILE *ctf_fs_metadata_open_file(const char *trace_path)
{
	GString *metadata_path = g_string_new(trace_path);
	FILE *fp = NULL;

	if (!metadata_path) {
		goto error;
	}

	g_string_append(metadata_path, "/" CTF_FS_METADATA_FILENAME);
	fp = fopen(metadata_path->str, "rb");
	if (!fp) {
		goto error;
	}

	goto end;

error:
	if (fp) {
		fclose(fp);
		fp = NULL;
	}

end:
	g_string_free(metadata_path, TRUE);
	return fp;
}

static struct ctf_fs_file *get_file(struct ctf_fs_component *ctf_fs,
		const char *trace_path)
{
	struct ctf_fs_file *file = ctf_fs_file_create(ctf_fs);

	if (!file) {
		goto error;
	}

	g_string_append(file->path, trace_path);
	g_string_append(file->path, "/" CTF_FS_METADATA_FILENAME);

	if (ctf_fs_file_open(ctf_fs, file, "rb")) {
		goto error;
	}

	goto end;

error:
	if (file) {
		ctf_fs_file_destroy(file);
		file = NULL;
	}

end:
	return file;
}

BT_HIDDEN
bool ctf_metadata_is_packetized(FILE *fp, int *byte_order)
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
int decode_packet(struct ctf_fs_component *ctf_fs, FILE *in_fp, FILE *out_fp,
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
	if (ctf_fs) {
		if (!ctf_fs->metadata->is_uuid_set) {
			memcpy(ctf_fs->metadata->uuid, header.uuid, sizeof(header.uuid));
			ctf_fs->metadata->is_uuid_set = true;
		} else if (bt_uuid_compare(header.uuid, ctf_fs->metadata->uuid)) {
			PERR("Metadata UUID mismatch between packets of the same file\n");
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

BT_HIDDEN
int ctf_metadata_packetized_file_to_buf(struct ctf_fs_component *ctf_fs,
		FILE *fp, uint8_t **buf, int byte_order)
{
	FILE *out_fp;
	size_t size;
	int ret = 0;
	int tret;
	size_t packet_index = 0;

	out_fp = babeltrace_open_memstream((char **) buf, &size);
	if (out_fp == NULL) {
		PERR("Cannot open memory stream: %s\n", strerror(errno));
		goto error;
	}

	for (;;) {
		if (feof(fp) != 0) {
			break;
		}

		tret = decode_packet(ctf_fs, fp, out_fp, byte_order);
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
	ret = babeltrace_close_memstream((char **) buf, &size, out_fp);
	if (ret < 0) {
		PERR("Cannot close memory stream: %s\n", strerror(errno));
		goto error;
	}

	goto end;

error:
	ret = -1;

	if (out_fp) {
		babeltrace_close_memstream((char **) buf, &size, out_fp);
	}

	if (*buf) {
		free(*buf);
	}

end:
	return ret;
}

int ctf_fs_metadata_set_trace(struct ctf_fs_component *ctf_fs)
{
	int ret = 0;
	struct ctf_fs_file *file = get_file(ctf_fs, ctf_fs->trace_path->str);
	struct ctf_scanner *scanner = NULL;
	uint8_t *buf = NULL;
	char *cbuf;

	if (!file) {
		PERR("Cannot create metadata file object\n");
		goto error;
	}

	if (babeltrace_debug) {
		// yydebug = 1;
	}

	if (ctf_metadata_is_packetized(file->fp, &ctf_fs->metadata->bo)) {
		PDBG("Metadata file \"%s\" is packetized\n", file->path->str);
		ret = ctf_metadata_packetized_file_to_buf(ctf_fs, file->fp,
			&buf, ctf_fs->metadata->bo);
		if (ret) {
			// log: details
			goto error;
		}

		cbuf = (char *) buf;
		ctf_fs->metadata->text = (char *) cbuf;

		/* Convert the real file pointer to a memory file pointer */
		ret = fclose(file->fp);
		file->fp = NULL;
		if (ret) {
			PERR("Cannot close file \"%s\": %s\n", file->path->str,
				strerror(errno));
			goto error;
		}

		file->fp = babeltrace_fmemopen(cbuf, strlen(cbuf), "rb");
		if (!file->fp) {
			PERR("Cannot memory-open metadata buffer: %s\n",
				strerror(errno));
			goto error;
		}
	} else {
		unsigned int major, minor;
		ssize_t nr_items;

		PDBG("Metadata file \"%s\" is plain text\n", file->path->str);

		/* Check text-only metadata header and version */
		nr_items = fscanf(file->fp, "/* CTF %10u.%10u", &major, &minor);
		if (nr_items < 2) {
			PWARN("Ill-shapen or missing \"/* CTF major.minor\" header in plain text metadata file\n");
		}
		PDBG("Metadata version: %u.%u\n", major, minor);

		if (!is_version_valid(major, minor)) {
			PERR("Invalid metadata version: %u.%u\n", major, minor);
			goto error;
		}

		rewind(file->fp);
	}

	/* Allocate a scanner and append the metadata text content */
	scanner = ctf_scanner_alloc();
	if (!scanner) {
		PERR("Cannot allocate a metadata lexical scanner\n");
		goto error;
	}

	ret = ctf_scanner_append_ast(scanner, file->fp);
	if (ret) {
		PERR("Cannot create the metadata AST\n");
		goto error;
	}

	ret = ctf_visitor_semantic_check(stderr, 0, &scanner->ast->root);
	if (ret) {
		PERR("Metadata semantic validation failed\n");
		goto error;
	}

	ret = ctf_visitor_generate_ir(ctf_fs->error_fp, &scanner->ast->root,
		&ctf_fs->metadata->trace);
	if (ret) {
		PERR("Cannot create trace object from metadata AST\n");
		goto error;
	}

	goto end;

error:
	if (ctf_fs->metadata->text) {
		free(ctf_fs->metadata->text);
		ctf_fs->metadata->text = NULL;
	}

	ret = -1;

end:
	if (file) {
		ctf_fs_file_destroy(file);
	}

	if (scanner) {
		ctf_scanner_free(scanner);
	}

	return ret;
}

int ctf_fs_metadata_init(struct ctf_fs_metadata *metadata)
{
	/* Nothing to initialize for the moment. */
	return 0;
}

void ctf_fs_metadata_fini(struct ctf_fs_metadata *metadata)
{
	if (metadata->text) {
		free(metadata->text);
	}

	if (metadata->trace) {
		BT_PUT(metadata->trace);
	}
}
