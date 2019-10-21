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
#include "parser-wrap.h"

#define TSDL_MAGIC	0x75d11d57

struct ctf_metadata_decoder {
	struct ctf_scanner *scanner;
	GString *text;
	struct ctf_visitor_generate_ir *visitor;
	bt_uuid_t uuid;
	bool is_uuid_set;
	int bo;
	struct ctf_metadata_decoder_config config;
	struct meta_log_config log_cfg;
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
int ctf_metadata_decoder_packetized_file_stream_to_buf(FILE *fp,
		char **buf, int byte_order, bool *is_uuid_set,
		uint8_t *uuid, bt_logging_level log_level,
		bt_self_component *self_comp);

BT_HIDDEN
int ctf_metadata_decoder_is_packetized(FILE *fp, bool *is_packetized,
		int *byte_order, bt_logging_level log_level,
		bt_self_component *self_comp)
{
	uint32_t magic;
	size_t len;
	int ret = 0;

	*is_packetized = false;
	len = fread(&magic, sizeof(magic), 1, fp);
	if (len != 1) {
		BT_COMP_LOG_CUR_LVL(BT_LOG_INFO, log_level, self_comp,
			"Cannot read first metadata packet header: assuming the stream is not packetized.");
		ret = -1;
		goto end;
	}

	if (byte_order) {
		if (magic == TSDL_MAGIC) {
			*is_packetized = true;
			*byte_order = BYTE_ORDER;
		} else if (magic == GUINT32_SWAP_LE_BE(TSDL_MAGIC)) {
			*is_packetized = true;
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

	mdec->log_cfg.log_level = config->log_level;
	mdec->log_cfg.self_comp = config->self_comp;
	mdec->scanner = ctf_scanner_alloc();
	if (!mdec->scanner) {
		BT_COMP_LOGE("Cannot allocate a metadata lexical scanner: "
			"mdec-addr=%p", mdec);
		goto error;
	}

	mdec->text = g_string_new(NULL);
	if (!mdec->text) {
		BT_COMP_LOGE("Failed to allocate one GString: "
			"mdec-addr=%p", mdec);
		goto error;
	}

	mdec->bo = -1;
	mdec->config = *config;
	mdec->visitor = ctf_visitor_generate_ir_create(config);
	if (!mdec->visitor) {
		BT_COMP_LOGE("Failed to create a CTF IR metadata AST visitor: "
			"mdec-addr=%p", mdec);
		goto error;
	}

	BT_COMP_LOGD("Creating CTF metadata decoder: "
		"clock-class-offset-s=%" PRId64 ", "
		"clock-class-offset-ns=%" PRId64 ", addr=%p",
		config->clock_class_offset_s, config->clock_class_offset_ns,
		mdec);
	goto end;

error:
	ctf_metadata_decoder_destroy(mdec);
	mdec = NULL;

end:
	return mdec;
}

BT_HIDDEN
void ctf_metadata_decoder_destroy(struct ctf_metadata_decoder *mdec)
{
	if (!mdec) {
		return;
	}

	if (mdec->scanner) {
		ctf_scanner_free(mdec->scanner);
	}

	if (mdec->text) {
		g_string_free(mdec->text, TRUE);
	}

	BT_COMP_LOGD("Destroying CTF metadata decoder: addr=%p", mdec);
	ctf_visitor_generate_ir_destroy(mdec->visitor);
	g_free(mdec);
}

BT_HIDDEN
enum ctf_metadata_decoder_status ctf_metadata_decoder_append_content(
		struct ctf_metadata_decoder *mdec, FILE *fp)
{
	enum ctf_metadata_decoder_status status =
		CTF_METADATA_DECODER_STATUS_OK;
	int ret;
	char *buf = NULL;
	bool close_fp = false;
	long start_pos = -1;
	bool is_packetized;

	BT_ASSERT(mdec);
	ret = ctf_metadata_decoder_is_packetized(fp, &is_packetized, &mdec->bo,
			mdec->config.log_level, mdec->config.self_comp);
	if (ret) {
		status = CTF_METADATA_DECODER_STATUS_ERROR;
		goto end;
	}

	if (is_packetized) {
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

#if YYDEBUG
	if (BT_LOG_ON_TRACE) {
		yydebug = 1;
	}
#endif

	/* Save the file's position: we'll seek back to append the plain text */
	BT_ASSERT(fp);

	if (mdec->config.keep_plain_text) {
		start_pos = ftell(fp);
	}

	/* Append the metadata text content */
	ret = ctf_scanner_append_ast(mdec->scanner, fp);
	if (ret) {
		BT_COMP_LOGE("Cannot create the metadata AST out of the metadata text: "
			"mdec-addr=%p", mdec);
		status = CTF_METADATA_DECODER_STATUS_INCOMPLETE;
		goto end;
	}

	/* We know it's complete: append plain text */
	if (mdec->config.keep_plain_text) {
		BT_ASSERT(start_pos != -1);
		ret = fseek(fp, start_pos, SEEK_SET);
		if (ret) {
			BT_COMP_LOGE("Failed to seek file: ret=%d, mdec-addr=%p",
				ret, mdec);
			status = CTF_METADATA_DECODER_STATUS_ERROR;
			goto end;
		}

		ret = bt_common_append_file_content_to_g_string(mdec->text, fp);
		if (ret) {
			BT_COMP_LOGE("Failed to append to current plain text: "
				"ret=%d, mdec-addr=%p", ret, mdec);
			status = CTF_METADATA_DECODER_STATUS_ERROR;
			goto end;
		}
	}

	ret = ctf_visitor_semantic_check(0, &mdec->scanner->ast->root,
		&mdec->log_cfg);
	if (ret) {
		BT_COMP_LOGE("Validation of the metadata semantics failed: "
			"mdec-addr=%p", mdec);
		status = CTF_METADATA_DECODER_STATUS_ERROR;
		goto end;
	}

	if (mdec->config.create_trace_class) {
		ret = ctf_visitor_generate_ir_visit_node(mdec->visitor,
			&mdec->scanner->ast->root);
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
	}

end:
#if YYDEBUG
	yydebug = 0;
#endif

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
	BT_ASSERT_DBG(mdec);
	BT_ASSERT_DBG(mdec->config.create_trace_class);
	return ctf_visitor_generate_ir_get_ir_trace_class(mdec->visitor);
}

BT_HIDDEN
struct ctf_trace_class *ctf_metadata_decoder_borrow_ctf_trace_class(
		struct ctf_metadata_decoder *mdec)
{
	BT_ASSERT_DBG(mdec);
	BT_ASSERT_DBG(mdec->config.create_trace_class);
	return ctf_visitor_generate_ir_borrow_ctf_trace_class(mdec->visitor);
}

BT_HIDDEN
const char *ctf_metadata_decoder_get_text(struct ctf_metadata_decoder *mdec)
{
	BT_ASSERT_DBG(mdec);
	BT_ASSERT_DBG(mdec->config.keep_plain_text);
	return mdec->text->str;
}

BT_HIDDEN
int ctf_metadata_decoder_get_byte_order(struct ctf_metadata_decoder *mdec)
{
	BT_ASSERT_DBG(mdec);
	return mdec->bo;
}

BT_HIDDEN
int ctf_metadata_decoder_get_uuid(struct ctf_metadata_decoder *mdec,
		bt_uuid_t uuid)
{
	int ret = 0;

	BT_ASSERT_DBG(mdec);

	if (!mdec->is_uuid_set) {
		ret = -1;
		goto end;
	}

	bt_uuid_copy(uuid, mdec->uuid);

end:
	return ret;
}

static
enum ctf_metadata_decoder_status find_uuid_in_trace_decl(
		struct ctf_metadata_decoder *mdec, struct ctf_node *trace_node,
		bt_uuid_t uuid)
{
	enum ctf_metadata_decoder_status status =
		CTF_METADATA_DECODER_STATUS_OK;
	struct ctf_node *entry_node;
	struct bt_list_head *decl_list = &trace_node->u.trace.declaration_list;
	char *left = NULL;

	bt_list_for_each_entry(entry_node, decl_list, siblings) {
		if (entry_node->type == NODE_CTF_EXPRESSION) {
			int ret;

			left = ctf_ast_concatenate_unary_strings(
				&entry_node->u.ctf_expression.left);
			if (!left) {
				BT_COMP_LOGE("Cannot concatenate unary strings.");
				status = CTF_METADATA_DECODER_STATUS_ERROR;
				goto end;
			}

			if (strcmp(left, "uuid") == 0) {
				ret = ctf_ast_get_unary_uuid(
					&entry_node->u.ctf_expression.right,
					uuid, mdec->config.log_level,
					mdec->config.self_comp);
				if (ret) {
					BT_COMP_LOGE("Invalid trace's `uuid` attribute.");
					status = CTF_METADATA_DECODER_STATUS_ERROR;
					goto end;
				}

				goto end;
			}

			g_free(left);
			left = NULL;
		}
	}

	status = CTF_METADATA_DECODER_STATUS_NONE;

end:
	g_free(left);
	return status;
}

BT_HIDDEN
enum ctf_metadata_decoder_status ctf_metadata_decoder_get_trace_class_uuid(
		struct ctf_metadata_decoder *mdec, bt_uuid_t uuid)
{
	enum ctf_metadata_decoder_status status =
		CTF_METADATA_DECODER_STATUS_INCOMPLETE;
	struct ctf_node *root_node = &mdec->scanner->ast->root;
	struct ctf_node *trace_node;

	if (!root_node) {
		status = CTF_METADATA_DECODER_STATUS_INCOMPLETE;
		goto end;
	}

	trace_node =
		bt_list_entry(root_node->u.root.trace.next,
			struct ctf_node, siblings);
	if (!trace_node) {
		status = CTF_METADATA_DECODER_STATUS_INCOMPLETE;
		goto end;
	}

	status = find_uuid_in_trace_decl(mdec, trace_node, uuid);

end:
	return status;
}
