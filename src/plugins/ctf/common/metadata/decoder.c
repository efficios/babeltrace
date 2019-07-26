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

#define BT_COMP_LOG_SELF_COMP (mdec->config.self_comp)
#define BT_LOG_OUTPUT_LEVEL (mdec->config.log_level)
#define BT_LOG_TAG "PLUGIN/CTF/META/DECODER"
#include "logging/comp-logging.h"

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

extern
int yydebug;

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

BT_HIDDEN
bool ctf_metadata_decoder_is_packetized(FILE *fp, int *byte_order,
		bt_logging_level log_level, bt_self_component *self_comp)
{
	uint32_t magic;
	size_t len;
	int ret = 0;

	len = fread(&magic, sizeof(magic), 1, fp);
	if (len != 1) {
		BT_COMP_LOG_CUR_LVL(BT_LOG_INFO, log_level, self_comp,
			"Cannot read first metadata packet header: assuming the stream is not packetized.");
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

BT_HIDDEN
struct ctf_metadata_decoder *ctf_metadata_decoder_create(
		const struct ctf_metadata_decoder_config *config)
{
	struct ctf_metadata_decoder *mdec =
		g_new0(struct ctf_metadata_decoder, 1);

	BT_ASSERT(config);
	BT_COMP_LOG_CUR_LVL(BT_LOG_DEBUG, config->log_level, config->self_comp,
		"Creating CTF metadata decoder: "
		"clock-class-offset-s=%" PRId64 ", "
		"clock-class-offset-ns=%" PRId64,
		config->clock_class_offset_s, config->clock_class_offset_ns);

	if (!mdec) {
		BT_COMP_LOG_CUR_LVL(BT_LOG_ERROR, config->log_level,
			config->self_comp,
			"Failed to allocate one CTF metadata decoder.");
		goto end;
	}

	mdec->config = *config;
	mdec->visitor = ctf_visitor_generate_ir_create(config);
	if (!mdec->visitor) {
		BT_COMP_LOGE("Failed to create a CTF IR metadata AST visitor: "
			"mdec-addr=%p", mdec);
		ctf_metadata_decoder_destroy(mdec);
		mdec = NULL;
		goto end;
	}

	BT_COMP_LOGD("Creating CTF metadata decoder: "
		"clock-class-offset-s=%" PRId64 ", "
		"clock-class-offset-ns=%" PRId64 ", addr=%p",
		config->clock_class_offset_s, config->clock_class_offset_ns,
		mdec);

end:
	return mdec;
}

BT_HIDDEN
void ctf_metadata_decoder_destroy(struct ctf_metadata_decoder *mdec)
{
	if (!mdec) {
		return;
	}

	BT_COMP_LOGD("Destroying CTF metadata decoder: addr=%p", mdec);
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
	struct meta_log_config log_cfg;

	BT_ASSERT(mdec);
	log_cfg.log_level = mdec->config.log_level;
	log_cfg.self_comp = mdec->config.self_comp;

	if (ctf_metadata_decoder_is_packetized(fp, &mdec->bo,
			mdec->config.log_level, mdec->config.self_comp)) {
		BT_COMP_LOGI("Metadata stream is packetized: mdec-addr=%p", mdec);
		ret = ctf_metadata_decoder_packetized_file_stream_to_buf(fp,
			&buf, mdec->bo, &mdec->is_uuid_set,
			mdec->uuid, mdec->config.log_level,
			mdec->config.self_comp);
		if (ret) {
			BT_COMP_LOGE("Cannot decode packetized metadata packets to metadata text: "
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
			BT_COMP_LOGE("Cannot memory-open metadata buffer: %s: "
				"mdec-addr=%p", strerror(errno), mdec);
			status = CTF_METADATA_DECODER_STATUS_ERROR;
			goto end;
		}
	} else {
		unsigned int major, minor;
		ssize_t nr_items;
		const long init_pos = ftell(fp);

		BT_COMP_LOGI("Metadata stream is plain text: mdec-addr=%p", mdec);

		if (init_pos < 0) {
			BT_COMP_LOGE_ERRNO("Failed to get current file position", ".");
			status = CTF_METADATA_DECODER_STATUS_ERROR;
			goto end;
		}

		/* Check text-only metadata header and version */
		nr_items = fscanf(fp, "/* CTF %10u.%10u", &major, &minor);
		if (nr_items < 2) {
			BT_COMP_LOGW("Missing \"/* CTF major.minor\" signature in plain text metadata file stream: "
				"mdec-addr=%p", mdec);
		}

		BT_COMP_LOGI("Found metadata stream version in signature: version=%u.%u", major, minor);

		if (!ctf_metadata_decoder_is_packet_version_valid(major,
				minor)) {
			BT_COMP_LOGE("Invalid metadata version found in plain text signature: "
				"version=%u.%u, mdec-addr=%p", major, minor,
				mdec);
			status = CTF_METADATA_DECODER_STATUS_INVAL_VERSION;
			goto end;
		}

		if (fseek(fp, init_pos, SEEK_SET)) {
			BT_COMP_LOGE("Cannot seek metadata file stream to initial position: %s: "
				"mdec-addr=%p", strerror(errno), mdec);
			status = CTF_METADATA_DECODER_STATUS_ERROR;
			goto end;
		}
	}

	if (BT_LOG_ON_TRACE) {
		yydebug = 1;
	}

	/* Allocate a scanner and append the metadata text content */
	scanner = ctf_scanner_alloc();
	if (!scanner) {
		BT_COMP_LOGE("Cannot allocate a metadata lexical scanner: "
			"mdec-addr=%p", mdec);
		status = CTF_METADATA_DECODER_STATUS_ERROR;
		goto end;
	}

	BT_ASSERT(fp);
	ret = ctf_scanner_append_ast(scanner, fp);
	if (ret) {
		BT_COMP_LOGE("Cannot create the metadata AST out of the metadata text: "
			"mdec-addr=%p", mdec);
		status = CTF_METADATA_DECODER_STATUS_INCOMPLETE;
		goto end;
	}

	ret = ctf_visitor_semantic_check(0, &scanner->ast->root, &log_cfg);
	if (ret) {
		BT_COMP_LOGE("Validation of the metadata semantics failed: "
			"mdec-addr=%p", mdec);
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
		BT_COMP_LOGD("While visiting metadata AST: incomplete data: "
			"mdec-addr=%p", mdec);
		status = CTF_METADATA_DECODER_STATUS_INCOMPLETE;
		goto end;
	default:
		BT_COMP_LOGE("Failed to visit AST node to create CTF IR objects: "
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
			BT_COMP_LOGE("Cannot close metadata file stream: "
				"mdec-addr=%p", mdec);
		}
	}

	free(buf);

	return status;
}

BT_HIDDEN
bt_trace_class *ctf_metadata_decoder_get_ir_trace_class(
		struct ctf_metadata_decoder *mdec)
{
	return ctf_visitor_generate_ir_get_ir_trace_class(mdec->visitor);
}

BT_HIDDEN
struct ctf_trace_class *ctf_metadata_decoder_borrow_ctf_trace_class(
		struct ctf_metadata_decoder *mdec)
{
	return ctf_visitor_generate_ir_borrow_ctf_trace_class(mdec->visitor);
}
