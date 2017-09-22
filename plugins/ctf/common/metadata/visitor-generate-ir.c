/*
 * ctf-visitor-generate-ir.c
 *
 * Common Trace Format metadata visitor (generates CTF IR objects).
 *
 * Based on older ctf-visitor-generate-io-struct.c.
 *
 * Copyright 2010 - Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
 * Copyright 2015-2016 - Philippe Proulx <philippe.proulx@efficios.com>
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

#define BT_LOG_TAG "PLUGIN-CTF-METADATA-IR-VISITOR"
#include "logging.h"

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <ctype.h>
#include <assert.h>
#include <glib.h>
#include <inttypes.h>
#include <errno.h>
#include <babeltrace/compat/uuid-internal.h>
#include <babeltrace/endian-internal.h>
#include <babeltrace/babeltrace.h>
#include <babeltrace/ctf-ir/field-types-internal.h>

#include "scanner.h"
#include "parser.h"
#include "ast.h"
#include "decoder.h"

/* Bit value (left shift) */
#define _BV(_val)		(1 << (_val))

/* Bit is set in a set of bits */
#define _IS_SET(_set, _mask)	(*(_set) & (_mask))

/* Set bit in a set of bits */
#define _SET(_set, _mask)	(*(_set) |= (_mask))

/* Bits for verifying existing attributes in various declarations */
enum {
	_CLOCK_NAME_SET =		_BV(0),
	_CLOCK_UUID_SET =		_BV(1),
	_CLOCK_FREQ_SET =		_BV(2),
	_CLOCK_PRECISION_SET =		_BV(3),
	_CLOCK_OFFSET_S_SET =		_BV(4),
	_CLOCK_OFFSET_SET =		_BV(5),
	_CLOCK_ABSOLUTE_SET =		_BV(6),
	_CLOCK_DESCRIPTION_SET =	_BV(7),
};

enum {
	_INTEGER_ALIGN_SET =		_BV(0),
	_INTEGER_SIZE_SET =		_BV(1),
	_INTEGER_BASE_SET =		_BV(2),
	_INTEGER_ENCODING_SET =		_BV(3),
	_INTEGER_BYTE_ORDER_SET =	_BV(4),
	_INTEGER_SIGNED_SET =		_BV(5),
	_INTEGER_MAP_SET =		_BV(6),
};

enum {
	_FLOAT_ALIGN_SET =		_BV(0),
	_FLOAT_MANT_DIG_SET =		_BV(1),
	_FLOAT_EXP_DIG_SET =		_BV(2),
	_FLOAT_BYTE_ORDER_SET =		_BV(3),
};

enum {
	_STRING_ENCODING_SET =		_BV(0),
};

enum {
	_TRACE_MINOR_SET =		_BV(0),
	_TRACE_MAJOR_SET =		_BV(1),
	_TRACE_BYTE_ORDER_SET =		_BV(2),
	_TRACE_UUID_SET =		_BV(3),
	_TRACE_PACKET_HEADER_SET =	_BV(4),
};

enum {
	_STREAM_ID_SET =		_BV(0),
	_STREAM_PACKET_CONTEXT_SET =	_BV(1),
	_STREAM_EVENT_HEADER_SET =	_BV(2),
	_STREAM_EVENT_CONTEXT_SET =	_BV(3),
};

enum {
	_EVENT_NAME_SET =		_BV(0),
	_EVENT_ID_SET =			_BV(1),
	_EVENT_MODEL_EMF_URI_SET =	_BV(2),
	_EVENT_STREAM_ID_SET =		_BV(3),
	_EVENT_LOGLEVEL_SET =		_BV(4),
	_EVENT_CONTEXT_SET =		_BV(5),
	_EVENT_FIELDS_SET =		_BV(6),
};

enum loglevel {
        LOGLEVEL_EMERG                  = 0,
        LOGLEVEL_ALERT                  = 1,
        LOGLEVEL_CRIT                   = 2,
        LOGLEVEL_ERR                    = 3,
        LOGLEVEL_WARNING                = 4,
        LOGLEVEL_NOTICE                 = 5,
        LOGLEVEL_INFO                   = 6,
        LOGLEVEL_DEBUG_SYSTEM           = 7,
        LOGLEVEL_DEBUG_PROGRAM          = 8,
        LOGLEVEL_DEBUG_PROCESS          = 9,
        LOGLEVEL_DEBUG_MODULE           = 10,
        LOGLEVEL_DEBUG_UNIT             = 11,
        LOGLEVEL_DEBUG_FUNCTION         = 12,
        LOGLEVEL_DEBUG_LINE             = 13,
        LOGLEVEL_DEBUG                  = 14,
	_NR_LOGLEVELS			= 15,
};

/* Prefixes of type aliases */
#define _PREFIX_ALIAS			'a'
#define _PREFIX_ENUM			'e'
#define _PREFIX_STRUCT			's'
#define _PREFIX_VARIANT			'v'

/* First entry in a BT list */
#define _BT_LIST_FIRST_ENTRY(_ptr, _type, _member)	\
	bt_list_entry((_ptr)->next, _type, _member)

#define _BT_FIELD_TYPE_INIT(_name)	struct bt_field_type *_name = NULL;

#define _BT_LOGE_DUP_ATTR(_node, _attr, _entity)			\
	_BT_LOGE_LINENO((_node)->lineno,				\
		"Duplicate attribute in %s: attr-name=\"%s\"",		\
		_entity, _attr)

#define _BT_LOGE_NODE(_node, _msg, args...)				\
	_BT_LOGE_LINENO((_node)->lineno, _msg, ## args)

#define _BT_LOGW_NODE(_node, _msg, args...)				\
	_BT_LOGW_LINENO((_node)->lineno, _msg, ## args)

#define _BT_LOGV_NODE(_node, _msg, args...)				\
	_BT_LOGV_LINENO((_node)->lineno, _msg, ## args)

/*
 * Declaration scope of a visitor context. This represents a TSDL
 * lexical scope, so that aliases and named structures, variants,
 * and enumerations may be registered and looked up hierarchically.
 */
struct ctx_decl_scope {
	/*
	 * Alias name to field type.
	 *
	 * GQuark -> struct bt_field_type *
	 */
	GHashTable *decl_map;

	/* Parent scope; NULL if this is the root declaration scope */
	struct ctx_decl_scope *parent_scope;
};

/*
 * Visitor context (private).
 */
struct ctx {
	/* Trace being filled (owned by this) */
	struct bt_trace *trace;

	/* Current declaration scope (top of the stack) */
	struct ctx_decl_scope *current_scope;

	/* 1 if trace declaration is visited */
	int is_trace_visited;

	/* 1 if this is an LTTng trace */
	bool is_lttng;

	/* Eventual name suffix of the trace to set */
	char *trace_name_suffix;

	/* Trace attributes */
	enum bt_byte_order trace_bo;
	uint64_t trace_major;
	uint64_t trace_minor;
	unsigned char trace_uuid[BABELTRACE_UUID_LEN];

	/*
	 * Stream IDs to stream classes.
	 *
	 * int64_t -> struct bt_stream_class *
	 */
	GHashTable *stream_classes;

	/* Config passed by the user */
	struct ctf_metadata_decoder_config decoder_config;
};

/*
 * Visitor (public).
 */
struct ctf_visitor_generate_ir { };

/**
 * Creates a new declaration scope.
 *
 * @param par_scope	Parent scope (NULL if creating a root scope)
 * @returns		New declaration scope, or NULL on error
 */
static
struct ctx_decl_scope *ctx_decl_scope_create(struct ctx_decl_scope *par_scope)
{
	struct ctx_decl_scope *scope;

	scope = g_new(struct ctx_decl_scope, 1);
	if (!scope) {
		BT_LOGE_STR("Failed to allocate one declaration scope.");
		goto end;
	}

	scope->decl_map = g_hash_table_new_full(g_direct_hash, g_direct_equal,
		NULL, (GDestroyNotify) bt_put);
	scope->parent_scope = par_scope;

end:
	return scope;
}

/**
 * Destroys a declaration scope.
 *
 * This function does not destroy the parent scope.
 *
 * @param scope	Scope to destroy
 */
static
void ctx_decl_scope_destroy(struct ctx_decl_scope *scope)
{
	if (!scope) {
		goto end;
	}

	g_hash_table_destroy(scope->decl_map);
	g_free(scope);

end:
	return;
}

/**
 * Returns the GQuark of a prefixed alias.
 *
 * @param prefix	Prefix character
 * @param name		Name
 * @returns		Associated GQuark, or 0 on error
 */
static
GQuark get_prefixed_named_quark(char prefix, const char *name)
{
	GQuark qname = 0;

	assert(name);

	/* Prefix character + original string + '\0' */
	char *prname = g_new(char, strlen(name) + 2);
	if (!prname) {
		BT_LOGE_STR("Failed to allocate a string.");
		goto end;
	}

	sprintf(prname, "%c%s", prefix, name);
	qname = g_quark_from_string(prname);
	g_free(prname);

end:
	return qname;
}

/**
 * Looks up a prefixed type alias within a declaration scope.
 *
 * @param scope		Declaration scope
 * @param prefix	Prefix character
 * @param name		Alias name
 * @param level		Number of levels to dig (-1 means infinite)
 * @returns		Declaration, or NULL if not found
 */
static
struct bt_field_type *ctx_decl_scope_lookup_prefix_alias(
	struct ctx_decl_scope *scope, char prefix,
	const char *name, int levels)
{
	GQuark qname = 0;
	int cur_levels = 0;
	_BT_FIELD_TYPE_INIT(decl);
	struct ctx_decl_scope *cur_scope = scope;

	assert(scope);
	assert(name);
	qname = get_prefixed_named_quark(prefix, name);
	if (!qname) {
		goto error;
	}

	if (levels < 0) {
		levels = INT_MAX;
	}

	while (cur_scope && cur_levels < levels) {
		decl = g_hash_table_lookup(cur_scope->decl_map,
			(gconstpointer) GUINT_TO_POINTER(qname));
		if (decl) {
			/* Caller's reference */
			bt_get(decl);
			break;
		}

		cur_scope = cur_scope->parent_scope;
		cur_levels++;
	}

	return decl;

error:
	return NULL;
}

/**
 * Looks up a type alias within a declaration scope.
 *
 * @param scope		Declaration scope
 * @param name		Alias name
 * @param level		Number of levels to dig (-1 means infinite)
 * @returns		Declaration, or NULL if not found
 */
static
struct bt_field_type *ctx_decl_scope_lookup_alias(
	struct ctx_decl_scope *scope, const char *name, int levels)
{
	return ctx_decl_scope_lookup_prefix_alias(scope, _PREFIX_ALIAS,
		name, levels);
}

/**
 * Looks up an enumeration within a declaration scope.
 *
 * @param scope		Declaration scope
 * @param name		Enumeration name
 * @param level		Number of levels to dig (-1 means infinite)
 * @returns		Declaration, or NULL if not found
 */
static
struct bt_field_type *ctx_decl_scope_lookup_enum(
	struct ctx_decl_scope *scope, const char *name, int levels)
{
	return ctx_decl_scope_lookup_prefix_alias(scope, _PREFIX_ENUM,
		name, levels);
}

/**
 * Looks up a structure within a declaration scope.
 *
 * @param scope		Declaration scope
 * @param name		Structure name
 * @param level		Number of levels to dig (-1 means infinite)
 * @returns		Declaration, or NULL if not found
 */
static
struct bt_field_type *ctx_decl_scope_lookup_struct(
	struct ctx_decl_scope *scope, const char *name, int levels)
{
	return ctx_decl_scope_lookup_prefix_alias(scope, _PREFIX_STRUCT,
		name, levels);
}

/**
 * Looks up a variant within a declaration scope.
 *
 * @param scope		Declaration scope
 * @param name		Variant name
 * @param level		Number of levels to dig (-1 means infinite)
 * @returns		Declaration, or NULL if not found
 */
static
struct bt_field_type *ctx_decl_scope_lookup_variant(
	struct ctx_decl_scope *scope, const char *name, int levels)
{
	return ctx_decl_scope_lookup_prefix_alias(scope, _PREFIX_VARIANT,
		name, levels);
}

/**
 * Registers a prefixed type alias within a declaration scope.
 *
 * @param scope		Declaration scope
 * @param prefix	Prefix character
 * @param name		Alias name (non-NULL)
 * @param decl		Declaration to register
 * @returns		0 if registration went okay, negative value otherwise
 */
static
int ctx_decl_scope_register_prefix_alias(struct ctx_decl_scope *scope,
	char prefix, const char *name, struct bt_field_type *decl)
{
	int ret = 0;
	GQuark qname = 0;
	_BT_FIELD_TYPE_INIT(edecl);

	assert(scope);
	assert(name);
	assert(decl);
	qname = get_prefixed_named_quark(prefix, name);
	if (!qname) {
		ret = -ENOMEM;
		goto error;
	}

	/* Make sure alias does not exist in local scope */
	edecl = ctx_decl_scope_lookup_prefix_alias(scope, prefix, name, 1);
	if (edecl) {
		BT_PUT(edecl);
		ret = -EEXIST;
		goto error;
	}

	g_hash_table_insert(scope->decl_map,
		GUINT_TO_POINTER(qname), decl);

	/* Hash table's reference */
	bt_get(decl);

	return 0;

error:
	return ret;
}

/**
 * Registers a type alias within a declaration scope.
 *
 * @param scope	Declaration scope
 * @param name	Alias name (non-NULL)
 * @param decl	Declaration to register
 * @returns	0 if registration went okay, negative value otherwise
 */
static
int ctx_decl_scope_register_alias(struct ctx_decl_scope *scope,
	const char *name, struct bt_field_type *decl)
{
	return ctx_decl_scope_register_prefix_alias(scope, _PREFIX_ALIAS,
		name, decl);
}

/**
 * Registers an enumeration declaration within a declaration scope.
 *
 * @param scope	Declaration scope
 * @param name	Enumeration name (non-NULL)
 * @param decl	Enumeration declaration to register
 * @returns	0 if registration went okay, negative value otherwise
 */
static
int ctx_decl_scope_register_enum(struct ctx_decl_scope *scope,
	const char *name, struct bt_field_type *decl)
{
	return ctx_decl_scope_register_prefix_alias(scope, _PREFIX_ENUM,
		name, decl);
}

/**
 * Registers a structure declaration within a declaration scope.
 *
 * @param scope	Declaration scope
 * @param name	Structure name (non-NULL)
 * @param decl	Structure declaration to register
 * @returns	0 if registration went okay, negative value otherwise
 */
static
int ctx_decl_scope_register_struct(struct ctx_decl_scope *scope,
	const char *name, struct bt_field_type *decl)
{
	return ctx_decl_scope_register_prefix_alias(scope, _PREFIX_STRUCT,
		name, decl);
}

/**
 * Registers a variant declaration within a declaration scope.
 *
 * @param scope	Declaration scope
 * @param name	Variant name (non-NULL)
 * @param decl	Variant declaration to register
 * @returns	0 if registration went okay, negative value otherwise
 */
static
int ctx_decl_scope_register_variant(struct ctx_decl_scope *scope,
	const char *name, struct bt_field_type *decl)
{
	return ctx_decl_scope_register_prefix_alias(scope, _PREFIX_VARIANT,
		name, decl);
}

/**
 * Destroys a visitor context.
 *
 * @param ctx	Visitor context to destroy
 */
static
void ctx_destroy(struct ctx *ctx)
{
	struct ctx_decl_scope *scope;
	/*
	 * Destroy all scopes, from current one to the root scope.
	 */

	if (!ctx) {
		goto end;
	}

	scope = ctx->current_scope;

	while (scope) {
		struct ctx_decl_scope *parent_scope = scope->parent_scope;

		ctx_decl_scope_destroy(scope);
		scope = parent_scope;
	}

	bt_put(ctx->trace);

	if (ctx->stream_classes) {
		g_hash_table_destroy(ctx->stream_classes);
	}

	free(ctx->trace_name_suffix);
	g_free(ctx);

end:
	return;
}

/**
 * Creates a new visitor context.
 *
 * @param trace	Associated trace
 * @returns	New visitor context, or NULL on error
 */
static
struct ctx *ctx_create(struct bt_trace *trace,
		const struct ctf_metadata_decoder_config *decoder_config,
		const char *trace_name_suffix)
{
	struct ctx *ctx = NULL;
	struct ctx_decl_scope *scope = NULL;

	assert(decoder_config);

	ctx = g_new0(struct ctx, 1);
	if (!ctx) {
		BT_LOGE_STR("Failed to allocate one visitor context.");
		goto error;
	}

	/* Root declaration scope */
	scope = ctx_decl_scope_create(NULL);
	if (!scope) {
		BT_LOGE_STR("Cannot create declaration scope.");
		goto error;
	}

	ctx->stream_classes = g_hash_table_new_full(g_int64_hash,
		g_int64_equal, g_free, (GDestroyNotify) bt_put);
	if (!ctx->stream_classes) {
		BT_LOGE_STR("Failed to allocate a GHashTable.");
		goto error;
	}

	if (trace_name_suffix) {
		ctx->trace_name_suffix = strdup(trace_name_suffix);
		if (!ctx->trace_name_suffix) {
			BT_LOGE_STR("Failed to copy string.");
			goto error;
		}
	}

	ctx->trace = trace;
	ctx->current_scope = scope;
	scope = NULL;
	ctx->trace_bo = BT_BYTE_ORDER_NATIVE;
	ctx->decoder_config = *decoder_config;
	return ctx;

error:
	ctx_destroy(ctx);
	ctx_decl_scope_destroy(scope);
	return NULL;
}

/**
 * Pushes a new declaration scope on top of a visitor context's
 * declaration scope stack.
 *
 * @param ctx	Visitor context
 * @returns	0 on success, or a negative value on error
 */
static
int ctx_push_scope(struct ctx *ctx)
{
	int ret = 0;
	struct ctx_decl_scope *new_scope;

	assert(ctx);
	new_scope = ctx_decl_scope_create(ctx->current_scope);
	if (!new_scope) {
		BT_LOGE_STR("Cannot create declaration scope.");
		ret = -ENOMEM;
		goto end;
	}

	ctx->current_scope = new_scope;

end:
	return ret;
}

static
void ctx_pop_scope(struct ctx *ctx)
{
	struct ctx_decl_scope *parent_scope = NULL;

	assert(ctx);

	if (!ctx->current_scope) {
		goto end;
	}

	parent_scope = ctx->current_scope->parent_scope;
	ctx_decl_scope_destroy(ctx->current_scope);
	ctx->current_scope = parent_scope;

end:
	return;
}

static
int visit_type_specifier_list(struct ctx *ctx, struct ctf_node *ts_list,
	struct bt_field_type **decl);

static
char *remove_underscores_from_field_ref(const char *field_ref)
{
	const char *in_ch;
	char *out_ch;
	char *ret;
	enum {
		UNDERSCORE_REMOVE_STATE_REMOVE_NEXT_UNDERSCORE,
		UNDERSCORE_REMOVE_STATE_DO_NOT_REMOVE_NEXT_UNDERSCORE,
	} state = UNDERSCORE_REMOVE_STATE_REMOVE_NEXT_UNDERSCORE;

	assert(field_ref);
	ret = calloc(strlen(field_ref) + 1, 1);
	if (!ret) {
		BT_LOGE("Failed to allocate a string: size=%zu",
			strlen(field_ref) + 1);
		goto end;
	}

	in_ch = field_ref;
	out_ch = ret;

	while (*in_ch != '\0') {
		switch (*in_ch) {
		case ' ':
		case '\t':
			/* Remove whitespace */
			in_ch++;
			continue;
		case '_':
			if (state == UNDERSCORE_REMOVE_STATE_REMOVE_NEXT_UNDERSCORE) {
				in_ch++;
				state = UNDERSCORE_REMOVE_STATE_DO_NOT_REMOVE_NEXT_UNDERSCORE;
				continue;
			}

			goto copy;
		case '.':
			state = UNDERSCORE_REMOVE_STATE_REMOVE_NEXT_UNDERSCORE;
			goto copy;
		default:
			state = UNDERSCORE_REMOVE_STATE_DO_NOT_REMOVE_NEXT_UNDERSCORE;
			goto copy;
		}

copy:
		*out_ch = *in_ch;
		in_ch++;
		out_ch++;
	}

end:
	return ret;
}

static
int is_unary_string(struct bt_list_head *head)
{
	int ret = TRUE;
	struct ctf_node *node;

	bt_list_for_each_entry(node, head, siblings) {
		if (node->type != NODE_UNARY_EXPRESSION) {
			ret = FALSE;
		}

		if (node->u.unary_expression.type != UNARY_STRING) {
			ret = FALSE;
		}
	}

	return ret;
}

static
char *concatenate_unary_strings(struct bt_list_head *head)
{
	int i = 0;
	GString *str;
	struct ctf_node *node;

	str = g_string_new(NULL);
	assert(str);

	bt_list_for_each_entry(node, head, siblings) {
		char *src_string;

		if (
			node->type != NODE_UNARY_EXPRESSION ||
			node->u.unary_expression.type != UNARY_STRING ||
			!(
				(
					node->u.unary_expression.link !=
					UNARY_LINK_UNKNOWN
				) ^ (i == 0)
			)
		) {
			goto error;
		}

		switch (node->u.unary_expression.link) {
		case UNARY_DOTLINK:
			g_string_append(str, ".");
			break;
		case UNARY_ARROWLINK:
			g_string_append(str, "->");
			break;
		case UNARY_DOTDOTDOT:
			g_string_append(str, "...");
			break;
		default:
			break;
		}

		src_string = node->u.unary_expression.u.string;
		g_string_append(str, src_string);
		i++;
	}

	/* Destroys the container, returns the underlying string */
	return g_string_free(str, FALSE);

error:
	/* This always returns NULL */
	return g_string_free(str, TRUE);
}

static
const char *get_map_clock_name_value(struct bt_list_head *head)
{
	int i = 0;
	struct ctf_node *node;
	const char *name = NULL;

	bt_list_for_each_entry(node, head, siblings) {
		char *src_string;
		int uexpr_type = node->u.unary_expression.type;
		int uexpr_link = node->u.unary_expression.link;
		int cond = node->type != NODE_UNARY_EXPRESSION ||
			uexpr_type != UNARY_STRING ||
			!((uexpr_link != UNARY_LINK_UNKNOWN) ^ (i == 0));
		if (cond) {
			goto error;
		}

		/* Needs to be chained with . */
		switch (node->u.unary_expression.link) {
		case UNARY_DOTLINK:
			break;
		case UNARY_ARROWLINK:
		case UNARY_DOTDOTDOT:
			goto error;
		default:
			break;
		}

		src_string = node->u.unary_expression.u.string;

		switch (i) {
		case 0:
			if (strcmp("clock", src_string)) {
				goto error;
			}
			break;
		case 1:
			name = src_string;
			break;
		case 2:
			if (strcmp("value", src_string)) {
				goto error;
			}
			break;
		default:
			/* Extra identifier, unknown */
			goto error;
		}

		i++;
	}

	return name;

error:
	return NULL;
}

static
int is_unary_unsigned(struct bt_list_head *head)
{
	int ret = TRUE;
	struct ctf_node *node;

	bt_list_for_each_entry(node, head, siblings) {
		if (node->type != NODE_UNARY_EXPRESSION) {
			ret = FALSE;
		}

		if (node->u.unary_expression.type != UNARY_UNSIGNED_CONSTANT) {
			ret = FALSE;
		}
	}

	return ret;
}

static
int get_unary_unsigned(struct bt_list_head *head, uint64_t *value)
{
	int i = 0;
	int ret = 0;
	struct ctf_node *node;

	if (bt_list_empty(head)) {
		ret = -1;
		goto end;
	}

	bt_list_for_each_entry(node, head, siblings) {
		int uexpr_type = node->u.unary_expression.type;
		int uexpr_link = node->u.unary_expression.link;
		int cond = node->type != NODE_UNARY_EXPRESSION ||
			uexpr_type != UNARY_UNSIGNED_CONSTANT ||
			uexpr_link != UNARY_LINK_UNKNOWN || i != 0;
		if (cond) {
			_BT_LOGE_NODE(node, "Invalid constant unsigned integer.");
			ret = -EINVAL;
			goto end;
		}

		*value = node->u.unary_expression.u.unsigned_constant;
		i++;
	}

end:
	return ret;
}

static
int is_unary_signed(struct bt_list_head *head)
{
	int ret = TRUE;
	struct ctf_node *node;

	bt_list_for_each_entry(node, head, siblings) {
		if (node->type != NODE_UNARY_EXPRESSION) {
			ret = FALSE;
		}

		if (node->u.unary_expression.type != UNARY_SIGNED_CONSTANT) {
			ret = FALSE;
		}
	}

	return ret;
}

static
int get_unary_signed(struct bt_list_head *head, int64_t *value)
{
	int i = 0;
	int ret = 0;
	struct ctf_node *node;

	bt_list_for_each_entry(node, head, siblings) {
		int uexpr_type = node->u.unary_expression.type;
		int uexpr_link = node->u.unary_expression.link;
		int cond = node->type != NODE_UNARY_EXPRESSION ||
			(uexpr_type != UNARY_UNSIGNED_CONSTANT &&
				uexpr_type != UNARY_SIGNED_CONSTANT) ||
			uexpr_link != UNARY_LINK_UNKNOWN || i != 0;
		if (cond) {
			ret = -EINVAL;
			goto end;
		}

		switch (uexpr_type) {
		case UNARY_UNSIGNED_CONSTANT:
			*value = (int64_t)
				node->u.unary_expression.u.unsigned_constant;
			break;
		case UNARY_SIGNED_CONSTANT:
			*value = node->u.unary_expression.u.signed_constant;
			break;
		default:
			ret = -EINVAL;
			goto end;
		}

		i++;
	}

end:
	return ret;
}

static
int get_unary_uuid(struct bt_list_head *head, unsigned char *uuid)
{
	int i = 0;
	int ret = 0;
	struct ctf_node *node;

	bt_list_for_each_entry(node, head, siblings) {
		int uexpr_type = node->u.unary_expression.type;
		int uexpr_link = node->u.unary_expression.link;
		const char *src_string;

		if (node->type != NODE_UNARY_EXPRESSION ||
				uexpr_type != UNARY_STRING ||
				uexpr_link != UNARY_LINK_UNKNOWN ||
				i != 0) {
			ret = -EINVAL;
			goto end;
		}

		src_string = node->u.unary_expression.u.string;
		ret = bt_uuid_parse(src_string, uuid);
		if (ret) {
			_BT_LOGE_NODE(node,
				"Cannot parse UUID: uuid=\"%s\"", src_string);
			goto end;
		}
	}

end:
	return ret;
}

static
int get_boolean(struct ctf_node *unary_expr)
{
	int ret = 0;

	if (unary_expr->type != NODE_UNARY_EXPRESSION) {
		_BT_LOGE_NODE(unary_expr,
			"Expecting unary expression: node-type=%d",
			unary_expr->type);
		ret = -EINVAL;
		goto end;
	}

	switch (unary_expr->u.unary_expression.type) {
	case UNARY_UNSIGNED_CONSTANT:
		ret = (unary_expr->u.unary_expression.u.unsigned_constant != 0);
		break;
	case UNARY_SIGNED_CONSTANT:
		ret = (unary_expr->u.unary_expression.u.signed_constant != 0);
		break;
	case UNARY_STRING:
	{
		const char *str = unary_expr->u.unary_expression.u.string;

		if (!strcmp(str, "true") || !strcmp(str, "TRUE")) {
			ret = TRUE;
		} else if (!strcmp(str, "false") || !strcmp(str, "FALSE")) {
			ret = FALSE;
		} else {
			_BT_LOGE_NODE(unary_expr,
				"Unexpected boolean value: value=\"%s\"", str);
			ret = -EINVAL;
			goto end;
		}
		break;
	}
	default:
		_BT_LOGE_NODE(unary_expr,
			"Unexpected unary expression type: node-type=%d",
			unary_expr->u.unary_expression.type);
		ret = -EINVAL;
		goto end;
	}

end:
	return ret;
}

static
enum bt_byte_order byte_order_from_unary_expr(struct ctf_node *unary_expr)
{
	const char *str;
	enum bt_byte_order bo = BT_BYTE_ORDER_UNKNOWN;

	if (unary_expr->u.unary_expression.type != UNARY_STRING) {
		_BT_LOGE_NODE(unary_expr,
			"\"byte_order\" attribute: expecting `be`, `le`, `network`, or `native`.");
		goto end;
	}

	str = unary_expr->u.unary_expression.u.string;

	if (!strcmp(str, "be") || !strcmp(str, "network")) {
		bo = BT_BYTE_ORDER_BIG_ENDIAN;
	} else if (!strcmp(str, "le")) {
		bo = BT_BYTE_ORDER_LITTLE_ENDIAN;
	} else if (!strcmp(str, "native")) {
		bo = BT_BYTE_ORDER_NATIVE;
	} else {
		_BT_LOGE_NODE(unary_expr,
			"Unexpected \"byte_order\" attribute value: "
			"expecting `be`, `le`, `network`, or `native`: value=\"%s\"",
			str);
		goto end;
	}

end:
	return bo;
}

static
enum bt_byte_order get_real_byte_order(struct ctx *ctx,
	struct ctf_node *uexpr)
{
	enum bt_byte_order bo = byte_order_from_unary_expr(uexpr);

	if (bo == BT_BYTE_ORDER_NATIVE) {
		bo = bt_trace_get_native_byte_order(ctx->trace);
	}

	return bo;
}

static
int is_align_valid(uint64_t align)
{
	return (align != 0) && !(align & (align - 1ULL));
}

static
int get_type_specifier_name(struct ctx *ctx, struct ctf_node *type_specifier,
	GString *str)
{
	int ret = 0;

	if (type_specifier->type != NODE_TYPE_SPECIFIER) {
		_BT_LOGE_NODE(type_specifier,
			"Unexpected node type: node-type=%d",
			type_specifier->type);
		ret = -EINVAL;
		goto end;
	}

	switch (type_specifier->u.type_specifier.type) {
	case TYPESPEC_VOID:
		g_string_append(str, "void");
		break;
	case TYPESPEC_CHAR:
		g_string_append(str, "char");
		break;
	case TYPESPEC_SHORT:
		g_string_append(str, "short");
		break;
	case TYPESPEC_INT:
		g_string_append(str, "int");
		break;
	case TYPESPEC_LONG:
		g_string_append(str, "long");
		break;
	case TYPESPEC_FLOAT:
		g_string_append(str, "float");
		break;
	case TYPESPEC_DOUBLE:
		g_string_append(str, "double");
		break;
	case TYPESPEC_SIGNED:
		g_string_append(str, "signed");
		break;
	case TYPESPEC_UNSIGNED:
		g_string_append(str, "unsigned");
		break;
	case TYPESPEC_BOOL:
		g_string_append(str, "bool");
		break;
	case TYPESPEC_COMPLEX:
		g_string_append(str, "_Complex");
		break;
	case TYPESPEC_IMAGINARY:
		g_string_append(str, "_Imaginary");
		break;
	case TYPESPEC_CONST:
		g_string_append(str, "const");
		break;
	case TYPESPEC_ID_TYPE:
		if (type_specifier->u.type_specifier.id_type) {
			g_string_append(str,
				type_specifier->u.type_specifier.id_type);
		}
		break;
	case TYPESPEC_STRUCT:
	{
		struct ctf_node *node = type_specifier->u.type_specifier.node;

		if (!node->u._struct.name) {
			_BT_LOGE_NODE(node, "Unexpected empty structure field type name.");
			ret = -EINVAL;
			goto end;
		}

		g_string_append(str, "struct ");
		g_string_append(str, node->u._struct.name);
		break;
	}
	case TYPESPEC_VARIANT:
	{
		struct ctf_node *node = type_specifier->u.type_specifier.node;

		if (!node->u.variant.name) {
			_BT_LOGE_NODE(node, "Unexpected empty variant field type name.");
			ret = -EINVAL;
			goto end;
		}

		g_string_append(str, "variant ");
		g_string_append(str, node->u.variant.name);
		break;
	}
	case TYPESPEC_ENUM:
	{
		struct ctf_node *node = type_specifier->u.type_specifier.node;

		if (!node->u._enum.enum_id) {
			_BT_LOGE_NODE(node,
				"Unexpected empty enumeration field type (`enum`) name.");
			ret = -EINVAL;
			goto end;
		}

		g_string_append(str, "enum ");
		g_string_append(str, node->u._enum.enum_id);
		break;
	}
	case TYPESPEC_FLOATING_POINT:
	case TYPESPEC_INTEGER:
	case TYPESPEC_STRING:
	default:
		_BT_LOGE_NODE(type_specifier->u.type_specifier.node,
			"Unexpected type specifier type: %d",
			type_specifier->u.type_specifier.type);
		ret = -EINVAL;
		goto end;
	}

end:
	return ret;
}

static
int get_type_specifier_list_name(struct ctx *ctx,
	struct ctf_node *type_specifier_list, GString *str)
{
	int ret = 0;
	struct ctf_node *iter;
	int alias_item_nr = 0;
	struct bt_list_head *head =
		&type_specifier_list->u.type_specifier_list.head;

	bt_list_for_each_entry(iter, head, siblings) {
		if (alias_item_nr != 0) {
			g_string_append(str, " ");
		}

		alias_item_nr++;
		ret = get_type_specifier_name(ctx, iter, str);
		if (ret) {
			goto end;
		}
	}

end:
	return ret;
}

static
GQuark create_typealias_identifier(struct ctx *ctx,
	struct ctf_node *type_specifier_list,
	struct ctf_node *node_type_declarator)
{
	int ret;
	char *str_c;
	GString *str;
	GQuark qalias = 0;
	struct ctf_node *iter;
	struct bt_list_head *pointers =
		&node_type_declarator->u.type_declarator.pointers;

	str = g_string_new("");
	ret = get_type_specifier_list_name(ctx, type_specifier_list, str);
	if (ret) {
		g_string_free(str, TRUE);
		goto end;
	}

	bt_list_for_each_entry(iter, pointers, siblings) {
		g_string_append(str, " *");

		if (iter->u.pointer.const_qualifier) {
			g_string_append(str, " const");
		}
	}

	str_c = g_string_free(str, FALSE);
	qalias = g_quark_from_string(str_c);
	g_free(str_c);

end:
	return qalias;
}

static
int visit_type_declarator(struct ctx *ctx, struct ctf_node *type_specifier_list,
	GQuark *field_name, struct ctf_node *node_type_declarator,
	struct bt_field_type **field_decl,
	struct bt_field_type *nested_decl)
{
	/*
	 * During this whole function, nested_decl is always OURS,
	 * whereas field_decl is an output which we create, but
	 * belongs to the caller (it is moved).
	 */

	int ret = 0;
	*field_decl = NULL;

	/* Validate type declarator node */
	if (node_type_declarator) {
		if (node_type_declarator->u.type_declarator.type ==
				TYPEDEC_UNKNOWN) {
			_BT_LOGE_NODE(node_type_declarator,
				"Unexpected type declarator type: type=%d",
				node_type_declarator->u.type_declarator.type);
			ret = -EINVAL;
			goto error;
		}

		/* TODO: GCC bitfields not supported yet */
		if (node_type_declarator->u.type_declarator.bitfield_len !=
				NULL) {
			_BT_LOGE_NODE(node_type_declarator,
				"GCC bitfields are not supported as of this version.");
			ret = -EPERM;
			goto error;
		}
	}

	/* Find the right nested declaration if not provided */
	if (!nested_decl) {
		struct bt_list_head *pointers =
			&node_type_declarator->u.type_declarator.pointers;

		if (node_type_declarator && !bt_list_empty(pointers)) {
			GQuark qalias;
			_BT_FIELD_TYPE_INIT(nested_decl_copy);

			/*
			 * If we have a pointer declarator, it HAS to
			 * be present in the typealiases (else fail).
			 */
			qalias = create_typealias_identifier(ctx,
				type_specifier_list, node_type_declarator);
			nested_decl =
				ctx_decl_scope_lookup_alias(ctx->current_scope,
					g_quark_to_string(qalias), -1);
			if (!nested_decl) {
				_BT_LOGE_NODE(node_type_declarator,
					"Cannot find type alias: name=\"%s\"",
					g_quark_to_string(qalias));
				ret = -EINVAL;
				goto error;
			}

			/* Make a copy of it */
			nested_decl_copy = bt_field_type_copy(nested_decl);
			BT_PUT(nested_decl);
			if (!nested_decl_copy) {
				_BT_LOGE_NODE(node_type_declarator,
					"Cannot copy nested field type.");
				ret = -EINVAL;
				goto error;
			}

			BT_MOVE(nested_decl, nested_decl_copy);

			/* Force integer's base to 16 since it's a pointer */
			if (bt_field_type_is_integer(nested_decl)) {
				ret = bt_field_type_integer_set_base(
					nested_decl,
					BT_INTEGER_BASE_HEXADECIMAL);
				assert(ret == 0);
			}
		} else {
			ret = visit_type_specifier_list(ctx,
				type_specifier_list, &nested_decl);
			if (ret) {
				assert(!nested_decl);
				goto error;
			}
		}
	}

	assert(nested_decl);

	if (!node_type_declarator) {
		BT_MOVE(*field_decl, nested_decl);
		goto end;
	}

	if (node_type_declarator->u.type_declarator.type == TYPEDEC_ID) {
		if (node_type_declarator->u.type_declarator.u.id) {
			const char *id =
				node_type_declarator->u.type_declarator.u.id;

			if (id[0] == '_') {
				id++;
			}

			*field_name = g_quark_from_string(id);
		} else {
			*field_name = 0;
		}

		BT_MOVE(*field_decl, nested_decl);
		goto end;
	} else {
		struct ctf_node *first;
		_BT_FIELD_TYPE_INIT(decl);
		_BT_FIELD_TYPE_INIT(outer_field_decl);
		struct bt_list_head *length =
			&node_type_declarator->
				u.type_declarator.u.nested.length;

		/* Create array/sequence, pass nested_decl as child */
		if (bt_list_empty(length)) {
			_BT_LOGE_NODE(node_type_declarator,
				"Expecting length field reference or value.");
			ret = -EINVAL;
			goto error;
		}

		first = _BT_LIST_FIRST_ENTRY(length, struct ctf_node, siblings);
		if (first->type != NODE_UNARY_EXPRESSION) {
			_BT_LOGE_NODE(first,
				"Unexpected node type: node-type=%d",
				first->type);
			ret = -EINVAL;
			goto error;
		}

		switch (first->u.unary_expression.type) {
		case UNARY_UNSIGNED_CONSTANT:
		{
			size_t len;
			_BT_FIELD_TYPE_INIT(array_decl);

			len = first->u.unary_expression.u.unsigned_constant;
			array_decl = bt_field_type_array_create(nested_decl,
				len);
			BT_PUT(nested_decl);
			if (!array_decl) {
				_BT_LOGE_NODE(first,
					"Cannot create array field type.");
				ret = -ENOMEM;
				goto error;
			}

			BT_MOVE(decl, array_decl);
			break;
		}
		case UNARY_STRING:
		{
			/* Lookup unsigned integer definition, create seq. */
			_BT_FIELD_TYPE_INIT(seq_decl);
			char *length_name = concatenate_unary_strings(length);
			char *length_name_no_underscore;

			if (!length_name) {
				_BT_LOGE_NODE(node_type_declarator,
					"Cannot concatenate unary strings.");
				ret = -EINVAL;
				goto error;
			}

			length_name_no_underscore =
				remove_underscores_from_field_ref(length_name);
			if (!length_name_no_underscore) {
				/* remove_underscores_from_field_ref() logs errors */
				ret = -EINVAL;
				goto error;
			}
			seq_decl = bt_field_type_sequence_create(
				nested_decl, length_name_no_underscore);
			free(length_name_no_underscore);
			g_free(length_name);
			BT_PUT(nested_decl);
			if (!seq_decl) {
				_BT_LOGE_NODE(node_type_declarator,
					"Cannot create sequence field type.");
				ret = -ENOMEM;
				goto error;
			}

			BT_MOVE(decl, seq_decl);
			break;
		}
		default:
			ret = -EINVAL;
			goto error;
		}

		assert(!nested_decl);
		assert(decl);
		assert(!*field_decl);

		/*
		 * At this point, we found the next nested declaration.
		 * We currently own this (and lost the ownership of
		 * nested_decl in the meantime). Pass this next
		 * nested declaration as the content of the outer
		 * container, MOVING its ownership.
		 */
		ret = visit_type_declarator(ctx, type_specifier_list,
			field_name,
			node_type_declarator->
				u.type_declarator.u.nested.type_declarator,
			&outer_field_decl, decl);
		decl = NULL;
		if (ret) {
			assert(!outer_field_decl);
			ret = -EINVAL;
			goto error;
		}

		assert(outer_field_decl);
		BT_MOVE(*field_decl, outer_field_decl);
	}

end:
	BT_PUT(nested_decl);
	assert(*field_decl);

	return 0;

error:
	BT_PUT(nested_decl);
	BT_PUT(*field_decl);

	return ret;
}

static
int visit_struct_decl_field(struct ctx *ctx,
	struct bt_field_type *struct_decl,
	struct ctf_node *type_specifier_list,
	struct bt_list_head *type_declarators)
{
	int ret = 0;
	struct ctf_node *iter;
	_BT_FIELD_TYPE_INIT(field_decl);

	bt_list_for_each_entry(iter, type_declarators, siblings) {
		field_decl = NULL;
		GQuark qfield_name;
		const char *field_name;
		_BT_FIELD_TYPE_INIT(efield_decl);

		ret = visit_type_declarator(ctx, type_specifier_list,
			&qfield_name, iter, &field_decl, NULL);
		if (ret) {
			assert(!field_decl);
			_BT_LOGE_NODE(type_specifier_list,
				"Cannot visit type declarator: ret=%d", ret);
			goto error;
		}

		assert(field_decl);
		field_name = g_quark_to_string(qfield_name);

		/* Check if field with same name already exists */
		efield_decl =
			bt_field_type_structure_get_field_type_by_name(
				struct_decl, field_name);
		if (efield_decl) {
			BT_PUT(efield_decl);
			_BT_LOGE_NODE(type_specifier_list,
				"Duplicate field in structure field type: "
				"field-name=\"%s\"", field_name);
			ret = -EINVAL;
			goto error;
		}

		/* Add field to structure */
		ret = bt_field_type_structure_add_field(struct_decl,
			field_decl, field_name);
		BT_PUT(field_decl);
		if (ret) {
			_BT_LOGE_NODE(type_specifier_list,
				"Cannot add field to structure field type: "
				"field-name=\"%s\", ret=%d",
				g_quark_to_string(qfield_name), ret);
			goto error;
		}
	}

	return 0;

error:
	BT_PUT(field_decl);

	return ret;
}

static
int visit_variant_decl_field(struct ctx *ctx,
	struct bt_field_type *variant_decl,
	struct ctf_node *type_specifier_list,
	struct bt_list_head *type_declarators)
{
	int ret = 0;
	struct ctf_node *iter;
	_BT_FIELD_TYPE_INIT(field_decl);

	bt_list_for_each_entry(iter, type_declarators, siblings) {
		field_decl = NULL;
		GQuark qfield_name;
		const char *field_name;
		_BT_FIELD_TYPE_INIT(efield_decl);

		ret = visit_type_declarator(ctx, type_specifier_list,
			&qfield_name, iter, &field_decl, NULL);
		if (ret) {
			assert(!field_decl);
			_BT_LOGE_NODE(type_specifier_list,
				"Cannot visit type declarator: ret=%d", ret);
			goto error;
		}

		assert(field_decl);
		field_name = g_quark_to_string(qfield_name);

		/* Check if field with same name already exists */
		efield_decl =
			bt_field_type_variant_get_field_type_by_name(
				variant_decl, field_name);
		if (efield_decl) {
			BT_PUT(efield_decl);
			_BT_LOGE_NODE(type_specifier_list,
				"Duplicate field in variant field type: "
				"field-name=\"%s\"", field_name);
			ret = -EINVAL;
			goto error;
		}

		/* Add field to structure */
		ret = bt_field_type_variant_add_field(variant_decl,
			field_decl, field_name);
		BT_PUT(field_decl);
		if (ret) {
			_BT_LOGE_NODE(type_specifier_list,
				"Cannot add field to variant field type: "
				"field-name=\"%s\", ret=%d",
				g_quark_to_string(qfield_name), ret);
			goto error;
		}
	}

	return 0;

error:
	BT_PUT(field_decl);

	return ret;
}

static
int visit_typedef(struct ctx *ctx, struct ctf_node *type_specifier_list,
	struct bt_list_head *type_declarators)
{
	int ret = 0;
	GQuark qidentifier;
	struct ctf_node *iter;
	_BT_FIELD_TYPE_INIT(type_decl);

	bt_list_for_each_entry(iter, type_declarators, siblings) {
		ret = visit_type_declarator(ctx, type_specifier_list,
			&qidentifier, iter, &type_decl, NULL);
		if (ret) {
			_BT_LOGE_NODE(iter,
				"Cannot visit type declarator: ret=%d", ret);
			ret = -EINVAL;
			goto end;
		}

		/* Do not allow typedef and typealias of untagged variants */
		if (bt_field_type_is_variant(type_decl)) {
			if (bt_field_type_variant_get_tag_name(type_decl)) {
				_BT_LOGE_NODE(iter,
					"Type definition of untagged variant field type is not allowed.");
				ret = -EPERM;
				goto end;
			}
		}

		ret = ctx_decl_scope_register_alias(ctx->current_scope,
			g_quark_to_string(qidentifier), type_decl);
		if (ret) {
			_BT_LOGE_NODE(iter,
				"Cannot register type definition: name=\"%s\"",
				g_quark_to_string(qidentifier));
			goto end;
		}
	}

end:
	BT_PUT(type_decl);

	return ret;
}

static
int visit_typealias(struct ctx *ctx, struct ctf_node *target,
	struct ctf_node *alias)
{
	int ret = 0;
	GQuark qalias;
	struct ctf_node *node;
	GQuark qdummy_field_name;
	_BT_FIELD_TYPE_INIT(type_decl);

	/* Create target type declaration */
	if (bt_list_empty(&target->u.typealias_target.type_declarators)) {
		node = NULL;
	} else {
		node = _BT_LIST_FIRST_ENTRY(
			&target->u.typealias_target.type_declarators,
			struct ctf_node, siblings);
	}

	ret = visit_type_declarator(ctx,
		target->u.typealias_target.type_specifier_list,
		&qdummy_field_name, node, &type_decl, NULL);
	if (ret) {
		assert(!type_decl);
		_BT_LOGE_NODE(node,
			"Cannot visit type declarator: ret=%d", ret);
		goto end;
	}

	/* Do not allow typedef and typealias of untagged variants */
	if (bt_field_type_is_variant(type_decl)) {
		if (bt_field_type_variant_get_tag_name(type_decl)) {
			_BT_LOGE_NODE(target,
				"Type definition of untagged variant field type is not allowed.");
			ret = -EPERM;
			goto end;
		}
	}

	/*
	 * The semantic validator does not check whether the target is
	 * abstract or not (if it has an identifier). Check it here.
	 */
	if (qdummy_field_name != 0) {
		_BT_LOGE_NODE(target,
			"Expecting empty identifier: id=\"%s\"",
			g_quark_to_string(qdummy_field_name));
		ret = -EINVAL;
		goto end;
	}

	/* Create alias identifier */
	node = _BT_LIST_FIRST_ENTRY(&alias->u.typealias_alias.type_declarators,
		struct ctf_node, siblings);
	qalias = create_typealias_identifier(ctx,
		alias->u.typealias_alias.type_specifier_list, node);
	ret = ctx_decl_scope_register_alias(ctx->current_scope,
		g_quark_to_string(qalias), type_decl);
	if (ret) {
		_BT_LOGE_NODE(node,
			"Cannot register type alias: name=\"%s\"",
			g_quark_to_string(qalias));
		goto end;
	}

end:
	BT_PUT(type_decl);

	return ret;
}

static
int visit_struct_decl_entry(struct ctx *ctx, struct ctf_node *entry_node,
	struct bt_field_type *struct_decl)
{
	int ret = 0;

	switch (entry_node->type) {
	case NODE_TYPEDEF:
		ret = visit_typedef(ctx,
			entry_node->u._typedef.type_specifier_list,
			&entry_node->u._typedef.type_declarators);
		if (ret) {
			_BT_LOGE_NODE(entry_node,
				"Cannot add type definition found in structure field type: ret=%d",
				ret);
			goto end;
		}
		break;
	case NODE_TYPEALIAS:
		ret = visit_typealias(ctx, entry_node->u.typealias.target,
			entry_node->u.typealias.alias);
		if (ret) {
			_BT_LOGE_NODE(entry_node,
				"Cannot add type alias found in structure field type: ret=%d",
				ret);
			goto end;
		}
		break;
	case NODE_STRUCT_OR_VARIANT_DECLARATION:
		/* Field */
		ret = visit_struct_decl_field(ctx, struct_decl,
			entry_node->u.struct_or_variant_declaration.
				type_specifier_list,
			&entry_node->u.struct_or_variant_declaration.
				type_declarators);
		if (ret) {
			goto end;
		}
		break;
	default:
		_BT_LOGE_NODE(entry_node,
			"Unexpected node type: node-type=%d", entry_node->type);
		ret = -EINVAL;
		goto end;
	}

end:
	return ret;
}

static
int visit_variant_decl_entry(struct ctx *ctx, struct ctf_node *entry_node,
	struct bt_field_type *variant_decl)
{
	int ret = 0;

	switch (entry_node->type) {
	case NODE_TYPEDEF:
		ret = visit_typedef(ctx,
			entry_node->u._typedef.type_specifier_list,
			&entry_node->u._typedef.type_declarators);
		if (ret) {
			_BT_LOGE_NODE(entry_node,
				"Cannot add type definition found in variant field type: ret=%d",
				ret);
			goto end;
		}
		break;
	case NODE_TYPEALIAS:
		ret = visit_typealias(ctx, entry_node->u.typealias.target,
			entry_node->u.typealias.alias);
		if (ret) {
			_BT_LOGE_NODE(entry_node,
				"Cannot add type alias found in variant field type: ret=%d",
				ret);
			goto end;
		}
		break;
	case NODE_STRUCT_OR_VARIANT_DECLARATION:
		/* Field */
		ret = visit_variant_decl_field(ctx, variant_decl,
			entry_node->u.struct_or_variant_declaration.
				type_specifier_list,
			&entry_node->u.struct_or_variant_declaration.
				type_declarators);
		if (ret) {
			goto end;
		}
		break;
	default:
		_BT_LOGE_NODE(entry_node,
			"Unexpected node type: node-type=%d",
			entry_node->type);
		ret = -EINVAL;
		goto end;
	}

end:
	return ret;
}

static
int visit_struct_decl(struct ctx *ctx, const char *name,
	struct bt_list_head *decl_list, int has_body,
	struct bt_list_head *min_align,
	struct bt_field_type **struct_decl)
{
	int ret = 0;

	*struct_decl = NULL;

	/* For named struct (without body), lookup in declaration scope */
	if (!has_body) {
		_BT_FIELD_TYPE_INIT(struct_decl_copy);

		if (!name) {
			BT_LOGE_STR("Bodyless structure field type: missing name.");
			ret = -EPERM;
			goto error;
		}

		*struct_decl = ctx_decl_scope_lookup_struct(ctx->current_scope,
			name, -1);
		if (!*struct_decl) {
			BT_LOGE("Cannot find structure field type: name=\"struct %s\"",
				name);
			ret = -EINVAL;
			goto error;
		}

		/* Make a copy of it */
		struct_decl_copy = bt_field_type_copy(*struct_decl);
		if (!struct_decl_copy) {
			BT_LOGE_STR("Cannot create copy of structure field type.");
			ret = -EINVAL;
			goto error;
		}

		BT_MOVE(*struct_decl, struct_decl_copy);
	} else {
		struct ctf_node *entry_node;
		uint64_t min_align_value = 0;

		if (name) {
			_BT_FIELD_TYPE_INIT(estruct_decl);

			estruct_decl = ctx_decl_scope_lookup_struct(
				ctx->current_scope, name, 1);
			if (estruct_decl) {
				BT_PUT(estruct_decl);
				BT_LOGE("Structure field type already declared in local scope: "
					"name=\"struct %s\"", name);
				ret = -EINVAL;
				goto error;
			}
		}

		if (!bt_list_empty(min_align)) {
			ret = get_unary_unsigned(min_align, &min_align_value);
			if (ret) {
				BT_LOGE("Unexpected unary expression for structure field type's `align` attribute: "
					"ret=%d", ret);
				goto error;
			}
		}

		*struct_decl = bt_field_type_structure_create();
		if (!*struct_decl) {
			BT_LOGE_STR("Cannot create empty structure field type.");
			ret = -ENOMEM;
			goto error;
		}

		if (min_align_value != 0) {
			ret = bt_field_type_set_alignment(*struct_decl,
					min_align_value);
			if (ret) {
				BT_LOGE("Cannot set structure field type's alignment: "
					"ret=%d", ret);
				goto error;
			}
		}

		ret = ctx_push_scope(ctx);
		if (ret) {
			BT_LOGE_STR("Cannot push scope.");
			goto error;
		}

		bt_list_for_each_entry(entry_node, decl_list, siblings) {
			ret = visit_struct_decl_entry(ctx, entry_node,
				*struct_decl);
			if (ret) {
				_BT_LOGE_NODE(entry_node,
					"Cannot visit structure field type entry: "
					"ret=%d", ret);
				ctx_pop_scope(ctx);
				goto error;
			}
		}

		ctx_pop_scope(ctx);

		if (name) {
			ret = ctx_decl_scope_register_struct(ctx->current_scope,
				name, *struct_decl);
			if (ret) {
				BT_LOGE("Cannot register structure field type in declaration scope: "
					"name=\"struct %s\", ret=%d", name, ret);
				goto error;
			}
		}
	}

	return 0;

error:
	BT_PUT(*struct_decl);

	return ret;
}

static
int visit_variant_decl(struct ctx *ctx, const char *name,
	const char *tag, struct bt_list_head *decl_list,
	int has_body, struct bt_field_type **variant_decl)
{
	int ret = 0;
	_BT_FIELD_TYPE_INIT(untagged_variant_decl);

	*variant_decl = NULL;

	/* For named variant (without body), lookup in declaration scope */
	if (!has_body) {
		_BT_FIELD_TYPE_INIT(variant_decl_copy);

		if (!name) {
			BT_LOGE_STR("Bodyless variant field type: missing name.");
			ret = -EPERM;
			goto error;
		}

		untagged_variant_decl =
			ctx_decl_scope_lookup_variant(ctx->current_scope,
				name, -1);
		if (!untagged_variant_decl) {
			BT_LOGE("Cannot find variant field type: name=\"variant %s\"",
				name);
			ret = -EINVAL;
			goto error;
		}

		/* Make a copy of it */
		variant_decl_copy = bt_field_type_copy(
			untagged_variant_decl);
		if (!variant_decl_copy) {
			BT_LOGE_STR("Cannot create copy of variant field type.");
			ret = -EINVAL;
			goto error;
		}

		BT_MOVE(untagged_variant_decl, variant_decl_copy);
	} else {
		struct ctf_node *entry_node;

		if (name) {
			struct bt_field_type *evariant_decl =
				ctx_decl_scope_lookup_struct(ctx->current_scope,
					name, 1);

			if (evariant_decl) {
				BT_PUT(evariant_decl);
				BT_LOGE("Variant field type already declared in local scope: "
					"name=\"variant %s\"", name);
				ret = -EINVAL;
				goto error;
			}
		}

		untagged_variant_decl = bt_field_type_variant_create(NULL,
			NULL);
		if (!untagged_variant_decl) {
			BT_LOGE_STR("Cannot create empty variant field type.");
			ret = -ENOMEM;
			goto error;
		}

		ret = ctx_push_scope(ctx);
		if (ret) {
			BT_LOGE_STR("Cannot push scope.");
			goto error;
		}

		bt_list_for_each_entry(entry_node, decl_list, siblings) {
			ret = visit_variant_decl_entry(ctx, entry_node,
				untagged_variant_decl);
			if (ret) {
				_BT_LOGE_NODE(entry_node,
					"Cannot visit variant field type entry: "
					"ret=%d", ret);
				ctx_pop_scope(ctx);
				goto error;
			}
		}

		ctx_pop_scope(ctx);

		if (name) {
			ret = ctx_decl_scope_register_variant(
				ctx->current_scope, name,
				untagged_variant_decl);
			if (ret) {
				BT_LOGE("Cannot register variant field type in declaration scope: "
					"name=\"variant %s\", ret=%d", name, ret);
				goto error;
			}
		}
	}

	/*
	 * If tagged, create tagged variant and return; otherwise
	 * return untagged variant.
	 */
	if (!tag) {
		BT_MOVE(*variant_decl, untagged_variant_decl);
	} else {
		/*
		 * At this point, we have a fresh untagged variant; nobody
		 * else owns it. Set its tag now.
		 */
		char *tag_no_underscore =
			remove_underscores_from_field_ref(tag);

		if (!tag_no_underscore) {
			/* remove_underscores_from_field_ref() logs errors */
			goto error;
		}

		ret = bt_field_type_variant_set_tag_name(
			untagged_variant_decl, tag_no_underscore);
		free(tag_no_underscore);
		if (ret) {
			BT_LOGE("Cannot set variant field type's tag name: "
				"tag-name=\"%s\"", tag);
			goto error;
		}

		BT_MOVE(*variant_decl, untagged_variant_decl);
	}

	assert(!untagged_variant_decl);
	assert(*variant_decl);

	return 0;

error:
	BT_PUT(untagged_variant_decl);
	BT_PUT(*variant_decl);

	return ret;
}

static
int visit_enum_decl_entry(struct ctx *ctx, struct ctf_node *enumerator,
	struct bt_field_type *enum_decl, int64_t *last, bt_bool is_signed)
{
	int ret = 0;
	int nr_vals = 0;
	struct ctf_node *iter;
	int64_t start = 0, end = 0;
	const char *label = enumerator->u.enumerator.id;
	struct bt_list_head *values = &enumerator->u.enumerator.values;

	bt_list_for_each_entry(iter, values, siblings) {
		int64_t *target;

		if (iter->type != NODE_UNARY_EXPRESSION) {
			_BT_LOGE_NODE(iter,
				"Wrong expression for enumeration field type label: "
				"node-type=%d, label=\"%s\"", iter->type,
				label);
			ret = -EINVAL;
			goto error;
		}

		if (nr_vals == 0) {
			target = &start;
		} else {
			target = &end;
		}

		switch (iter->u.unary_expression.type) {
		case UNARY_SIGNED_CONSTANT:
			*target = iter->u.unary_expression.u.signed_constant;
			break;
		case UNARY_UNSIGNED_CONSTANT:
			*target = (int64_t)
				iter->u.unary_expression.u.unsigned_constant;
			break;
		default:
			_BT_LOGE_NODE(iter,
				"Invalid enumeration field type entry: "
				"expecting constant signed or unsigned integer: "
				"node-type=%d, label=\"%s\"",
				iter->u.unary_expression.type, label);
			ret = -EINVAL;
			goto error;
		}

		if (nr_vals > 1) {
			_BT_LOGE_NODE(iter,
				"Invalid enumeration field type entry: label=\"%s\"",
				label);
			ret = -EINVAL;
			goto error;
		}

		nr_vals++;
	}

	if (nr_vals == 0) {
		start = *last;
	}

	if (nr_vals <= 1) {
		end = start;
	}

	*last = end + 1;

	if (is_signed) {
		ret = bt_ctf_field_type_enumeration_add_mapping(enum_decl, label,
			start, end);
	} else {
		ret = bt_field_type_enumeration_add_mapping_unsigned(enum_decl,
			label, (uint64_t) start, (uint64_t) end);
	}
	if (ret) {
		_BT_LOGE_NODE(enumerator,
			"Cannot add mapping to enumeration field type: "
			"label=\"%s\", ret=%d, "
			"start-value-unsigned=%" PRIu64 ", "
			"end-value-unsigned=%" PRIu64, label, ret,
			(uint64_t) start, (uint64_t) end);
		goto error;
	}

	return 0;

error:
	return ret;
}

static
int visit_enum_decl(struct ctx *ctx, const char *name,
	struct ctf_node *container_type,
	struct bt_list_head *enumerator_list,
	int has_body,
	struct bt_field_type **enum_decl)
{
	int ret = 0;
	GQuark qdummy_id;
	_BT_FIELD_TYPE_INIT(integer_decl);

	*enum_decl = NULL;

	/* For named enum (without body), lookup in declaration scope */
	if (!has_body) {
		_BT_FIELD_TYPE_INIT(enum_decl_copy);

		if (!name) {
			BT_LOGE_STR("Bodyless enumeration field type: missing name.");
			ret = -EPERM;
			goto error;
		}

		*enum_decl = ctx_decl_scope_lookup_enum(ctx->current_scope,
			name, -1);
		if (!*enum_decl) {
			BT_LOGE("Cannot find enumeration field type: "
				"name=\"enum %s\"", name);
			ret = -EINVAL;
			goto error;
		}

		/* Make a copy of it */
		enum_decl_copy = bt_field_type_copy(*enum_decl);
		if (!enum_decl_copy) {
			BT_LOGE_STR("Cannot create copy of enumeration field type.");
			ret = -EINVAL;
			goto error;
		}

		BT_PUT(*enum_decl);
		BT_MOVE(*enum_decl, enum_decl_copy);
	} else {
		struct ctf_node *iter;
		int64_t last_value = 0;

		if (name) {
			_BT_FIELD_TYPE_INIT(eenum_decl);

			eenum_decl = ctx_decl_scope_lookup_enum(
				ctx->current_scope, name, 1);
			if (eenum_decl) {
				BT_PUT(eenum_decl);
				BT_LOGE("Enumeration field type already declared in local scope: "
					"name=\"enum %s\"", name);
				ret = -EINVAL;
				goto error;
			}
		}

		if (!container_type) {
			integer_decl = ctx_decl_scope_lookup_alias(
				ctx->current_scope, "int", -1);
			if (!integer_decl) {
				BT_LOGE_STR("Cannot find implicit `int` field type alias for enumeration field type.");
				ret = -EINVAL;
				goto error;
			}
		} else {
			ret = visit_type_declarator(ctx, container_type,
				&qdummy_id, NULL, &integer_decl, NULL);
			if (ret) {
				assert(!integer_decl);
				ret = -EINVAL;
				goto error;
			}
		}

		assert(integer_decl);

		if (!bt_field_type_is_integer(integer_decl)) {
			BT_LOGE("Container field type for enumeration field type is not an integer field type: "
				"ft-id=%s",
				bt_field_type_id_string(
					bt_field_type_get_type_id(integer_decl)));
			ret = -EINVAL;
			goto error;
		}

		*enum_decl = bt_field_type_enumeration_create(integer_decl);
		if (!*enum_decl) {
			BT_LOGE_STR("Cannot create enumeration field type.");
			ret = -ENOMEM;
			goto error;
		}

		bt_list_for_each_entry(iter, enumerator_list, siblings) {
			ret = visit_enum_decl_entry(ctx, iter, *enum_decl,
				&last_value,
				bt_field_type_integer_is_signed(integer_decl));
			if (ret) {
				_BT_LOGE_NODE(iter,
					"Cannot visit enumeration field type entry: "
					"ret=%d", ret);
				goto error;
			}
		}

		if (name) {
			ret = ctx_decl_scope_register_enum(ctx->current_scope,
				name, *enum_decl);
			if (ret) {
				BT_LOGE("Cannot register enumeration field type in declaration scope: "
					"ret=%d", ret);
				goto error;
			}
		}
	}

	BT_PUT(integer_decl);

	return 0;

error:
	BT_PUT(integer_decl);
	BT_PUT(*enum_decl);

	return ret;
}

static
int visit_type_specifier(struct ctx *ctx,
	struct ctf_node *type_specifier_list,
	struct bt_field_type **decl)
{
	int ret = 0;
	GString *str = NULL;
	_BT_FIELD_TYPE_INIT(decl_copy);

	*decl = NULL;
	str = g_string_new("");
	ret = get_type_specifier_list_name(ctx, type_specifier_list, str);
	if (ret) {
		_BT_LOGE_NODE(type_specifier_list,
			"Cannot get type specifier list's name: ret=%d", ret);
		goto error;
	}

	*decl = ctx_decl_scope_lookup_alias(ctx->current_scope, str->str, -1);
	if (!*decl) {
		_BT_LOGE_NODE(type_specifier_list,
			"Cannot find type alias: name=\"%s\"", str->str);
		ret = -EINVAL;
		goto error;
	}

	/* Make a copy of the type declaration */
	decl_copy = bt_field_type_copy(*decl);
	if (!decl_copy) {
		_BT_LOGE_NODE(type_specifier_list,
			"Cannot create field type copy.");
		ret = -EINVAL;
		goto error;
	}

	BT_MOVE(*decl, decl_copy);
	(void) g_string_free(str, TRUE);
	str = NULL;

	return 0;

error:
	if (str) {
		(void) g_string_free(str, TRUE);
	}

	BT_PUT(*decl);

	return ret;
}

static
int visit_integer_decl(struct ctx *ctx,
	struct bt_list_head *expressions,
	struct bt_field_type **integer_decl)
{
	int set = 0;
	int ret = 0;
	bt_bool signedness = 0;
	struct ctf_node *expression;
	uint64_t alignment = 0, size = 0;
	struct bt_clock_class *mapped_clock = NULL;
	enum bt_string_encoding encoding = BT_STRING_ENCODING_NONE;
	enum bt_integer_base base = BT_INTEGER_BASE_DECIMAL;
	enum bt_byte_order byte_order =
		bt_trace_get_native_byte_order(ctx->trace);

	*integer_decl = NULL;

	bt_list_for_each_entry(expression, expressions, siblings) {
		struct ctf_node *left, *right;

		left = _BT_LIST_FIRST_ENTRY(&expression->u.ctf_expression.left,
			struct ctf_node, siblings);
		right = _BT_LIST_FIRST_ENTRY(
			&expression->u.ctf_expression.right, struct ctf_node,
			siblings);

		if (left->u.unary_expression.type != UNARY_STRING) {
			_BT_LOGE_NODE(left,
				"Unexpected unary expression type: type=%d",
				left->u.unary_expression.type);
			ret = -EINVAL;
			goto error;
		}

		if (!strcmp(left->u.unary_expression.u.string, "signed")) {
			if (_IS_SET(&set, _INTEGER_SIGNED_SET)) {
				_BT_LOGE_DUP_ATTR(left, "signed",
					"integer field type");
				ret = -EPERM;
				goto error;
			}

			signedness = get_boolean(right);
			if (signedness < 0) {
				_BT_LOGE_NODE(right,
					"Invalid boolean value for integer field type's `signed` attribute: "
					"ret=%d", ret);
				ret = -EINVAL;
				goto error;
			}

			_SET(&set, _INTEGER_SIGNED_SET);
		} else if (!strcmp(left->u.unary_expression.u.string,
				"byte_order")) {
			if (_IS_SET(&set, _INTEGER_BYTE_ORDER_SET)) {
				_BT_LOGE_DUP_ATTR(left, "byte_order",
					"integer field type");
				ret = -EPERM;
				goto error;
			}

			byte_order = get_real_byte_order(ctx, right);
			if (byte_order == BT_BYTE_ORDER_UNKNOWN) {
				_BT_LOGE_NODE(right,
					"Invalid `byte_order` attribute in integer field type: "
					"ret=%d", ret);
				ret = -EINVAL;
				goto error;
			}

			_SET(&set, _INTEGER_BYTE_ORDER_SET);
		} else if (!strcmp(left->u.unary_expression.u.string, "size")) {
			if (_IS_SET(&set, _INTEGER_SIZE_SET)) {
				_BT_LOGE_DUP_ATTR(left, "size",
					"integer field type");
				ret = -EPERM;
				goto error;
			}

			if (right->u.unary_expression.type !=
					UNARY_UNSIGNED_CONSTANT) {
				_BT_LOGE_NODE(right,
					"Invalid `size` attribute in integer field type: "
					"expecting unsigned constant integer: "
					"node-type=%d",
					right->u.unary_expression.type);
				ret = -EINVAL;
				goto error;
			}

			size = right->u.unary_expression.u.unsigned_constant;
			if (size == 0) {
				_BT_LOGE_NODE(right,
					"Invalid `size` attribute in integer field type: "
					"expecting positive constant integer: "
					"size=%" PRIu64, size);
				ret = -EINVAL;
				goto error;
			} else if (size > 64) {
				_BT_LOGE_NODE(right,
					"Invalid `size` attribute in integer field type: "
					"integer fields over 64 bits are not supported as of this version: "
					"size=%" PRIu64, size);
				ret = -EINVAL;
				goto error;
			}

			_SET(&set, _INTEGER_SIZE_SET);
		} else if (!strcmp(left->u.unary_expression.u.string,
				"align")) {
			if (_IS_SET(&set, _INTEGER_ALIGN_SET)) {
				_BT_LOGE_DUP_ATTR(left, "align",
					"integer field type");
				ret = -EPERM;
				goto error;
			}

			if (right->u.unary_expression.type !=
					UNARY_UNSIGNED_CONSTANT) {
				_BT_LOGE_NODE(right,
					"Invalid `align` attribute in integer field type: "
					"expecting unsigned constant integer: "
					"node-type=%d",
					right->u.unary_expression.type);
				ret = -EINVAL;
				goto error;
			}

			alignment =
				right->u.unary_expression.u.unsigned_constant;
			if (!is_align_valid(alignment)) {
				_BT_LOGE_NODE(right,
					"Invalid `align` attribute in integer field type: "
					"expecting power of two: "
					"align=%" PRIu64, alignment);
				ret = -EINVAL;
				goto error;
			}

			_SET(&set, _INTEGER_ALIGN_SET);
		} else if (!strcmp(left->u.unary_expression.u.string, "base")) {
			if (_IS_SET(&set, _INTEGER_BASE_SET)) {
				_BT_LOGE_DUP_ATTR(left, "base",
					"integer field type");
				ret = -EPERM;
				goto error;
			}

			switch (right->u.unary_expression.type) {
			case UNARY_UNSIGNED_CONSTANT:
			{
				uint64_t constant = right->u.unary_expression.
					u.unsigned_constant;

				switch (constant) {
				case 2:
					base = BT_INTEGER_BASE_BINARY;
					break;
				case 8:
					base = BT_INTEGER_BASE_OCTAL;
					break;
				case 10:
					base = BT_INTEGER_BASE_DECIMAL;
					break;
				case 16:
					base = BT_INTEGER_BASE_HEXADECIMAL;
					break;
				default:
					_BT_LOGE_NODE(right,
						"Invalid `base` attribute in integer field type: "
						"base=%" PRIu64,
						right->u.unary_expression.u.unsigned_constant);
					ret = -EINVAL;
					goto error;
				}
				break;
			}
			case UNARY_STRING:
			{
				char *s_right = concatenate_unary_strings(
					&expression->u.ctf_expression.right);
				if (!s_right) {
					_BT_LOGE_NODE(right,
						"Unexpected unary expression for integer field type's `base` attribute.");
					ret = -EINVAL;
					goto error;
				}

				if (!strcmp(s_right, "decimal") ||
						!strcmp(s_right, "dec") ||
						!strcmp(s_right, "d") ||
						!strcmp(s_right, "i") ||
						!strcmp(s_right, "u")) {
					base = BT_INTEGER_BASE_DECIMAL;
				} else if (!strcmp(s_right, "hexadecimal") ||
						!strcmp(s_right, "hex") ||
						!strcmp(s_right, "x") ||
						!strcmp(s_right, "X") ||
						!strcmp(s_right, "p")) {
					base = BT_INTEGER_BASE_HEXADECIMAL;
				} else if (!strcmp(s_right, "octal") ||
						!strcmp(s_right, "oct") ||
						!strcmp(s_right, "o")) {
					base = BT_INTEGER_BASE_OCTAL;
				} else if (!strcmp(s_right, "binary") ||
						!strcmp(s_right, "b")) {
					base = BT_INTEGER_BASE_BINARY;
				} else {
					_BT_LOGE_NODE(right,
						"Unexpected unary expression for integer field type's `base` attribute: "
						"base=\"%s\"", s_right);
					g_free(s_right);
					ret = -EINVAL;
					goto error;
				}

				g_free(s_right);
				break;
			}
			default:
				_BT_LOGE_NODE(right,
					"Invalid `base` attribute in integer field type: "
					"expecting unsigned constant integer or unary string.");
				ret = -EINVAL;
				goto error;
			}

			_SET(&set, _INTEGER_BASE_SET);
		} else if (!strcmp(left->u.unary_expression.u.string,
				"encoding")) {
			char *s_right;

			if (_IS_SET(&set, _INTEGER_ENCODING_SET)) {
				_BT_LOGE_DUP_ATTR(left, "encoding",
					"integer field type");
				ret = -EPERM;
				goto error;
			}

			if (right->u.unary_expression.type != UNARY_STRING) {
				_BT_LOGE_NODE(right,
					"Invalid `encoding` attribute in integer field type: "
					"expecting unary string.");
				ret = -EINVAL;
				goto error;
			}

			s_right = concatenate_unary_strings(
				&expression->u.ctf_expression.right);
			if (!s_right) {
				_BT_LOGE_NODE(right,
					"Unexpected unary expression for integer field type's `encoding` attribute.");
				ret = -EINVAL;
				goto error;
			}

			if (!strcmp(s_right, "UTF8") ||
					!strcmp(s_right, "utf8") ||
					!strcmp(s_right, "utf-8") ||
					!strcmp(s_right, "UTF-8")) {
				encoding = BT_STRING_ENCODING_UTF8;
			} else if (!strcmp(s_right, "ASCII") ||
					!strcmp(s_right, "ascii")) {
				encoding = BT_STRING_ENCODING_ASCII;
			} else if (!strcmp(s_right, "none")) {
				encoding = BT_STRING_ENCODING_NONE;
			} else {
				_BT_LOGE_NODE(right,
					"Invalid `encoding` attribute in integer field type: "
					"unknown encoding: encoding=\"%s\"",
					s_right);
				g_free(s_right);
				ret = -EINVAL;
				goto error;
			}

			g_free(s_right);
			_SET(&set, _INTEGER_ENCODING_SET);
		} else if (!strcmp(left->u.unary_expression.u.string, "map")) {
			const char *clock_name;

			if (_IS_SET(&set, _INTEGER_MAP_SET)) {
				_BT_LOGE_DUP_ATTR(left, "map",
					"integer field type");
				ret = -EPERM;
				goto error;
			}

			if (right->u.unary_expression.type != UNARY_STRING) {
				_BT_LOGE_NODE(right,
					"Invalid `map` attribute in integer field type: "
					"expecting unary string.");
				ret = -EINVAL;
				goto error;
			}

			clock_name =
				get_map_clock_name_value(
					&expression->u.ctf_expression.right);
			if (!clock_name) {
				char *s_right = concatenate_unary_strings(
					&expression->u.ctf_expression.right);

				if (!s_right) {
					_BT_LOGE_NODE(right,
						"Unexpected unary expression for integer field type's `map` attribute.");
					ret = -EINVAL;
					goto error;
				}

				_BT_LOGE_NODE(right,
					"Invalid `map` attribute in integer field type: "
					"cannot find clock class at this point: name=\"%s\"",
					s_right);
				_SET(&set, _INTEGER_MAP_SET);
				g_free(s_right);
				continue;
			}

			mapped_clock = bt_trace_get_clock_class_by_name(
				ctx->trace, clock_name);
			if (!mapped_clock) {
				_BT_LOGE_NODE(right,
					"Invalid `map` attribute in integer field type: "
					"cannot find clock class at this point: name=\"%s\"",
					clock_name);
				ret = -EINVAL;
				goto error;
			}

			_SET(&set, _INTEGER_MAP_SET);
		} else {
			_BT_LOGW_NODE(left,
				"Unknown attribute in integer field type: "
				"attr-name=\"%s\"",
				left->u.unary_expression.u.string);
		}
	}

	if (!_IS_SET(&set, _INTEGER_SIZE_SET)) {
		BT_LOGE_STR("Missing `size` attribute in integer field type.");
		ret = -EPERM;
		goto error;
	}

	if (!_IS_SET(&set, _INTEGER_ALIGN_SET)) {
		if (size % CHAR_BIT) {
			/* Bit-packed alignment */
			alignment = 1;
		} else {
			/* Byte-packed alignment */
			alignment = CHAR_BIT;
		}
	}

	*integer_decl = bt_field_type_integer_create((unsigned int) size);
	if (!*integer_decl) {
		BT_LOGE_STR("Cannot create integer field type.");
		ret = -ENOMEM;
		goto error;
	}

	ret = bt_field_type_integer_set_is_signed(*integer_decl, signedness);
	ret |= bt_field_type_integer_set_base(*integer_decl, base);
	ret |= bt_field_type_integer_set_encoding(*integer_decl, encoding);
	ret |= bt_field_type_set_alignment(*integer_decl,
		(unsigned int) alignment);
	ret |= bt_field_type_set_byte_order(*integer_decl, byte_order);

	if (mapped_clock) {
		/* Move clock */
		ret |= bt_field_type_integer_set_mapped_clock_class(
			*integer_decl, mapped_clock);
		bt_put(mapped_clock);
		mapped_clock = NULL;
	}

	if (ret) {
		BT_LOGE_STR("Cannot configure integer field type.");
		ret = -EINVAL;
		goto error;
	}

	return 0;

error:
	if (mapped_clock) {
		bt_put(mapped_clock);
	}

	BT_PUT(*integer_decl);

	return ret;
}

static
int visit_floating_point_number_decl(struct ctx *ctx,
	struct bt_list_head *expressions,
	struct bt_field_type **float_decl)
{
	int set = 0;
	int ret = 0;
	struct ctf_node *expression;
	uint64_t alignment = 1, exp_dig = 0, mant_dig = 0;
	enum bt_byte_order byte_order =
		bt_trace_get_native_byte_order(ctx->trace);

	*float_decl = NULL;

	bt_list_for_each_entry(expression, expressions, siblings) {
		struct ctf_node *left, *right;

		left = _BT_LIST_FIRST_ENTRY(&expression->u.ctf_expression.left,
			struct ctf_node, siblings);
		right = _BT_LIST_FIRST_ENTRY(
			&expression->u.ctf_expression.right, struct ctf_node,
			siblings);

		if (left->u.unary_expression.type != UNARY_STRING) {
			_BT_LOGE_NODE(left,
				"Unexpected unary expression type: type=%d",
				left->u.unary_expression.type);
			ret = -EINVAL;
			goto error;
		}

		if (!strcmp(left->u.unary_expression.u.string, "byte_order")) {
			if (_IS_SET(&set, _FLOAT_BYTE_ORDER_SET)) {
				_BT_LOGE_DUP_ATTR(left, "byte_order",
					"floating point number field type");
				ret = -EPERM;
				goto error;
			}

			byte_order = get_real_byte_order(ctx, right);
			if (byte_order == BT_BYTE_ORDER_UNKNOWN) {
				_BT_LOGE_NODE(right,
					"Invalid `byte_order` attribute in floating point number field type: "
					"ret=%d", ret);
				ret = -EINVAL;
				goto error;
			}

			_SET(&set, _FLOAT_BYTE_ORDER_SET);
		} else if (!strcmp(left->u.unary_expression.u.string,
				"exp_dig")) {
			if (_IS_SET(&set, _FLOAT_EXP_DIG_SET)) {
				_BT_LOGE_DUP_ATTR(left, "exp_dig",
					"floating point number field type");
				ret = -EPERM;
				goto error;
			}

			if (right->u.unary_expression.type !=
					UNARY_UNSIGNED_CONSTANT) {
				_BT_LOGE_NODE(right,
					"Invalid `exp_dig` attribute in floating point number field type: "
					"expecting unsigned constant integer: "
					"node-type=%d",
					right->u.unary_expression.type);
				ret = -EINVAL;
				goto error;
			}

			exp_dig = right->u.unary_expression.u.unsigned_constant;
			_SET(&set, _FLOAT_EXP_DIG_SET);
		} else if (!strcmp(left->u.unary_expression.u.string,
				"mant_dig")) {
			if (_IS_SET(&set, _FLOAT_MANT_DIG_SET)) {
				_BT_LOGE_DUP_ATTR(left, "mant_dig",
					"floating point number field type");
				ret = -EPERM;
				goto error;
			}

			if (right->u.unary_expression.type !=
					UNARY_UNSIGNED_CONSTANT) {
				_BT_LOGE_NODE(right,
					"Invalid `mant_dig` attribute in floating point number field type: "
					"expecting unsigned constant integer: "
					"node-type=%d",
					right->u.unary_expression.type);
				ret = -EINVAL;
				goto error;
			}

			mant_dig = right->u.unary_expression.u.
				unsigned_constant;
			_SET(&set, _FLOAT_MANT_DIG_SET);
		} else if (!strcmp(left->u.unary_expression.u.string,
				"align")) {
			if (_IS_SET(&set, _FLOAT_ALIGN_SET)) {
				_BT_LOGE_DUP_ATTR(left, "align",
					"floating point number field type");
				ret = -EPERM;
				goto error;
			}

			if (right->u.unary_expression.type !=
					UNARY_UNSIGNED_CONSTANT) {
				_BT_LOGE_NODE(right,
					"Invalid `align` attribute in floating point number field type: "
					"expecting unsigned constant integer: "
					"node-type=%d",
					right->u.unary_expression.type);
				ret = -EINVAL;
				goto error;
			}

			alignment = right->u.unary_expression.u.
				unsigned_constant;

			if (!is_align_valid(alignment)) {
				_BT_LOGE_NODE(right,
					"Invalid `align` attribute in floating point number field type: "
					"expecting power of two: "
					"align=%" PRIu64, alignment);
				ret = -EINVAL;
				goto error;
			}

			_SET(&set, _FLOAT_ALIGN_SET);
		} else {
			_BT_LOGW_NODE(left,
				"Unknown attribute in floating point number field type: "
				"attr-name=\"%s\"",
				left->u.unary_expression.u.string);
		}
	}

	if (!_IS_SET(&set, _FLOAT_MANT_DIG_SET)) {
		BT_LOGE_STR("Missing `mant_dig` attribute in floating point number field type.");
		ret = -EPERM;
		goto error;
	}

	if (!_IS_SET(&set, _FLOAT_EXP_DIG_SET)) {
		BT_LOGE_STR("Missing `exp_dig` attribute in floating point number field type.");
		ret = -EPERM;
		goto error;
	}

	if (!_IS_SET(&set, _INTEGER_ALIGN_SET)) {
		if ((mant_dig + exp_dig) % CHAR_BIT) {
			/* Bit-packed alignment */
			alignment = 1;
		} else {
			/* Byte-packed alignment */
			alignment = CHAR_BIT;
		}
	}

	*float_decl = bt_field_type_floating_point_create();
	if (!*float_decl) {
		BT_LOGE_STR("Cannot create floating point number field type.");
		ret = -ENOMEM;
		goto error;
	}

	ret = bt_field_type_floating_point_set_exponent_digits(
		*float_decl, exp_dig);
	ret |= bt_field_type_floating_point_set_mantissa_digits(
		*float_decl, mant_dig);
	ret |= bt_field_type_set_byte_order(*float_decl, byte_order);
	ret |= bt_field_type_set_alignment(*float_decl, alignment);
	if (ret) {
		BT_LOGE_STR("Cannot configure floating point number field type.");
		ret = -EINVAL;
		goto error;
	}

	return 0;

error:
	BT_PUT(*float_decl);

	return ret;
}

static
int visit_string_decl(struct ctx *ctx,
	struct bt_list_head *expressions,
	struct bt_field_type **string_decl)
{
	int set = 0;
	int ret = 0;
	struct ctf_node *expression;
	enum bt_string_encoding encoding = BT_STRING_ENCODING_UTF8;

	*string_decl = NULL;

	bt_list_for_each_entry(expression, expressions, siblings) {
		struct ctf_node *left, *right;

		left = _BT_LIST_FIRST_ENTRY(&expression->u.ctf_expression.left,
			struct ctf_node, siblings);
		right = _BT_LIST_FIRST_ENTRY(
			&expression->u.ctf_expression.right, struct ctf_node,
			siblings);

		if (left->u.unary_expression.type != UNARY_STRING) {
			_BT_LOGE_NODE(left,
				"Unexpected unary expression type: type=%d",
				left->u.unary_expression.type);
			ret = -EINVAL;
			goto error;
		}

		if (!strcmp(left->u.unary_expression.u.string, "encoding")) {
			char *s_right;

			if (_IS_SET(&set, _STRING_ENCODING_SET)) {
				_BT_LOGE_DUP_ATTR(left, "encoding",
					"string field type");
				ret = -EPERM;
				goto error;
			}

			if (right->u.unary_expression.type != UNARY_STRING) {
				_BT_LOGE_NODE(right,
					"Invalid `encoding` attribute in string field type: "
					"expecting unary string.");
				ret = -EINVAL;
				goto error;
			}

			s_right = concatenate_unary_strings(
				&expression->u.ctf_expression.right);
			if (!s_right) {
				_BT_LOGE_NODE(right,
					"Unexpected unary expression for string field type's `encoding` attribute.");
				ret = -EINVAL;
				goto error;
			}

			if (!strcmp(s_right, "UTF8") ||
					!strcmp(s_right, "utf8") ||
					!strcmp(s_right, "utf-8") ||
					!strcmp(s_right, "UTF-8")) {
				encoding = BT_STRING_ENCODING_UTF8;
			} else if (!strcmp(s_right, "ASCII") ||
					!strcmp(s_right, "ascii")) {
				encoding = BT_STRING_ENCODING_ASCII;
			} else if (!strcmp(s_right, "none")) {
				encoding = BT_STRING_ENCODING_NONE;
			} else {
				_BT_LOGE_NODE(right,
					"Invalid `encoding` attribute in string field type: "
					"unknown encoding: encoding=\"%s\"",
					s_right);
				g_free(s_right);
				ret = -EINVAL;
				goto error;
			}

			g_free(s_right);
			_SET(&set, _STRING_ENCODING_SET);
		} else {
			_BT_LOGW_NODE(left,
				"Unknown attribute in string field type: "
				"attr-name=\"%s\"",
				left->u.unary_expression.u.string);
		}
	}

	*string_decl = bt_field_type_string_create();
	if (!*string_decl) {
		BT_LOGE_STR("Cannot create string field type.");
		ret = -ENOMEM;
		goto error;
	}

	ret = bt_field_type_string_set_encoding(*string_decl, encoding);
	if (ret) {
		BT_LOGE_STR("Cannot configure string field type.");
		ret = -EINVAL;
		goto error;
	}

	return 0;

error:
	BT_PUT(*string_decl);

	return ret;
}

static
int visit_type_specifier_list(struct ctx *ctx,
	struct ctf_node *ts_list,
	struct bt_field_type **decl)
{
	int ret = 0;
	struct ctf_node *first, *node;

	*decl = NULL;

	if (ts_list->type != NODE_TYPE_SPECIFIER_LIST) {
		_BT_LOGE_NODE(ts_list,
			"Unexpected node type: node-type=%d", ts_list->type);
		ret = -EINVAL;
		goto error;
	}

	first = _BT_LIST_FIRST_ENTRY(&ts_list->u.type_specifier_list.head,
		struct ctf_node, siblings);
	if (first->type != NODE_TYPE_SPECIFIER) {
		_BT_LOGE_NODE(first,
			"Unexpected node type: node-type=%d", first->type);
		ret = -EINVAL;
		goto error;
	}

	node = first->u.type_specifier.node;

	switch (first->u.type_specifier.type) {
	case TYPESPEC_INTEGER:
		ret = visit_integer_decl(ctx, &node->u.integer.expressions,
			decl);
		if (ret) {
			assert(!*decl);
			goto error;
		}
		break;
	case TYPESPEC_FLOATING_POINT:
		ret = visit_floating_point_number_decl(ctx,
			&node->u.floating_point.expressions, decl);
		if (ret) {
			assert(!*decl);
			goto error;
		}
		break;
	case TYPESPEC_STRING:
		ret = visit_string_decl(ctx,
			&node->u.string.expressions, decl);
		if (ret) {
			assert(!*decl);
			goto error;
		}
		break;
	case TYPESPEC_STRUCT:
		ret = visit_struct_decl(ctx, node->u._struct.name,
			&node->u._struct.declaration_list,
			node->u._struct.has_body,
			&node->u._struct.min_align, decl);
		if (ret) {
			assert(!*decl);
			goto error;
		}
		break;
	case TYPESPEC_VARIANT:
		ret = visit_variant_decl(ctx, node->u.variant.name,
			node->u.variant.choice,
			&node->u.variant.declaration_list,
			node->u.variant.has_body, decl);
		if (ret) {
			assert(!*decl);
			goto error;
		}
		break;
	case TYPESPEC_ENUM:
		ret = visit_enum_decl(ctx, node->u._enum.enum_id,
			node->u._enum.container_type,
			&node->u._enum.enumerator_list,
			node->u._enum.has_body, decl);
		if (ret) {
			assert(!*decl);
			goto error;
		}
		break;
	case TYPESPEC_VOID:
	case TYPESPEC_CHAR:
	case TYPESPEC_SHORT:
	case TYPESPEC_INT:
	case TYPESPEC_LONG:
	case TYPESPEC_FLOAT:
	case TYPESPEC_DOUBLE:
	case TYPESPEC_SIGNED:
	case TYPESPEC_UNSIGNED:
	case TYPESPEC_BOOL:
	case TYPESPEC_COMPLEX:
	case TYPESPEC_IMAGINARY:
	case TYPESPEC_CONST:
	case TYPESPEC_ID_TYPE:
		ret = visit_type_specifier(ctx, ts_list, decl);
		if (ret) {
			_BT_LOGE_NODE(first,
				"Cannot visit type specifier: ret=%d",
				ret);
			assert(!*decl);
			goto error;
		}
		break;
	default:
		_BT_LOGE_NODE(first,
			"Unexpected type specifier type: node-type=%d",
			first->u.type_specifier.type);
		ret = -EINVAL;
		goto error;
	}

	assert(*decl);

	return 0;

error:
	BT_PUT(*decl);

	return ret;
}

static
int visit_event_decl_entry(struct ctx *ctx, struct ctf_node *node,
	struct bt_event_class *event_class, int64_t *stream_id,
	int *set)
{
	int ret = 0;
	char *left = NULL;
	_BT_FIELD_TYPE_INIT(decl);

	switch (node->type) {
	case NODE_TYPEDEF:
		ret = visit_typedef(ctx, node->u._typedef.type_specifier_list,
			&node->u._typedef.type_declarators);
		if (ret) {
			_BT_LOGE_NODE(node,
				"Cannot add type definition found in event class.");
			goto error;
		}
		break;
	case NODE_TYPEALIAS:
		ret = visit_typealias(ctx, node->u.typealias.target,
			node->u.typealias.alias);
		if (ret) {
			_BT_LOGE_NODE(node,
				"Cannot add type alias found in event class.");
			goto error;
		}
		break;
	case NODE_CTF_EXPRESSION:
	{
		left = concatenate_unary_strings(&node->u.ctf_expression.left);
		if (!left) {
			_BT_LOGE_NODE(node, "Cannot concatenate unary strings.");
			ret = -EINVAL;
			goto error;
		}

		if (!strcmp(left, "name")) {
			/* This is already known at this stage */
			if (_IS_SET(set, _EVENT_NAME_SET)) {
				_BT_LOGE_DUP_ATTR(node, "name",
					"event class");
				ret = -EPERM;
				goto error;
			}

			_SET(set, _EVENT_NAME_SET);
		} else if (!strcmp(left, "id")) {
			int64_t id;

			if (_IS_SET(set, _EVENT_ID_SET)) {
				_BT_LOGE_DUP_ATTR(node, "id",
					"event class");
				ret = -EPERM;
				goto error;
			}

			ret = get_unary_unsigned(&node->u.ctf_expression.right,
				(uint64_t *) &id);
			/* Only read "id" if get_unary_unsigned() succeeded. */
			if (ret || (!ret && id < 0)) {
				_BT_LOGE_NODE(node,
					"Unexpected unary expression for event class's `id` attribute.");
				ret = -EINVAL;
				goto error;
			}

			ret = bt_event_class_set_id(event_class, id);
			if (ret) {
				_BT_LOGE_NODE(node,
					"Cannot set event class's ID: "
					"id=%" PRId64, id);
				goto error;
			}

			_SET(set, _EVENT_ID_SET);
		} else if (!strcmp(left, "stream_id")) {
			if (_IS_SET(set, _EVENT_STREAM_ID_SET)) {
				_BT_LOGE_DUP_ATTR(node, "stream_id",
					"event class");
				ret = -EPERM;
				goto error;
			}

			ret = get_unary_unsigned(&node->u.ctf_expression.right,
				(uint64_t *) stream_id);
			/*
			 * Only read "stream_id" if get_unary_unsigned()
			 * succeeded.
			 */
			if (ret || (!ret && *stream_id < 0)) {
				_BT_LOGE_NODE(node,
					"Unexpected unary expression for event class's `stream_id` attribute.");
				ret = -EINVAL;
				goto error;
			}

			_SET(set, _EVENT_STREAM_ID_SET);
		} else if (!strcmp(left, "context")) {
			if (_IS_SET(set, _EVENT_CONTEXT_SET)) {
				_BT_LOGE_NODE(node,
					"Duplicate `context` entry in event class.");
				ret = -EPERM;
				goto error;
			}

			ret = visit_type_specifier_list(ctx,
				_BT_LIST_FIRST_ENTRY(
					&node->u.ctf_expression.right,
					struct ctf_node, siblings),
				&decl);
			if (ret) {
				_BT_LOGE_NODE(node,
					"Cannot create event class's context field type.");
				goto error;
			}

			assert(decl);
			ret = bt_event_class_set_context_type(
				event_class, decl);
			BT_PUT(decl);
			if (ret) {
				_BT_LOGE_NODE(node,
					"Cannot set event class's context field type.");
				goto error;
			}

			_SET(set, _EVENT_CONTEXT_SET);
		} else if (!strcmp(left, "fields")) {
			if (_IS_SET(set, _EVENT_FIELDS_SET)) {
				_BT_LOGE_NODE(node,
					"Duplicate `fields` entry in event class.");
				ret = -EPERM;
				goto error;
			}

			ret = visit_type_specifier_list(ctx,
				_BT_LIST_FIRST_ENTRY(
					&node->u.ctf_expression.right,
					struct ctf_node, siblings),
				&decl);
			if (ret) {
				_BT_LOGE_NODE(node,
					"Cannot create event class's payload field type.");
				goto error;
			}

			assert(decl);
			ret = bt_event_class_set_payload_type(
				event_class, decl);
			BT_PUT(decl);
			if (ret) {
				_BT_LOGE_NODE(node,
					"Cannot set event class's payload field type.");
				goto error;
			}

			_SET(set, _EVENT_FIELDS_SET);
		} else if (!strcmp(left, "loglevel")) {
			uint64_t loglevel_value;
			enum bt_event_class_log_level log_level =
				BT_EVENT_CLASS_LOG_LEVEL_UNSPECIFIED;

			if (_IS_SET(set, _EVENT_LOGLEVEL_SET)) {
				_BT_LOGE_DUP_ATTR(node, "loglevel",
					"event class");
				ret = -EPERM;
				goto error;
			}

			ret = get_unary_unsigned(&node->u.ctf_expression.right,
				&loglevel_value);
			if (ret) {
				_BT_LOGE_NODE(node,
					"Unexpected unary expression for event class's `loglevel` attribute.");
				ret = -EINVAL;
				goto error;
			}

			switch (loglevel_value) {
			case 0:
				log_level = BT_EVENT_CLASS_LOG_LEVEL_EMERGENCY;
				break;
			case 1:
				log_level = BT_EVENT_CLASS_LOG_LEVEL_ALERT;
				break;
			case 2:
				log_level = BT_EVENT_CLASS_LOG_LEVEL_CRITICAL;
				break;
			case 3:
				log_level = BT_EVENT_CLASS_LOG_LEVEL_ERROR;
				break;
			case 4:
				log_level = BT_EVENT_CLASS_LOG_LEVEL_WARNING;
				break;
			case 5:
				log_level = BT_EVENT_CLASS_LOG_LEVEL_NOTICE;
				break;
			case 6:
				log_level = BT_EVENT_CLASS_LOG_LEVEL_INFO;
				break;
			case 7:
				log_level = BT_EVENT_CLASS_LOG_LEVEL_DEBUG_SYSTEM;
				break;
			case 8:
				log_level = BT_EVENT_CLASS_LOG_LEVEL_DEBUG_PROGRAM;
				break;
			case 9:
				log_level = BT_EVENT_CLASS_LOG_LEVEL_DEBUG_PROCESS;
				break;
			case 10:
				log_level = BT_EVENT_CLASS_LOG_LEVEL_DEBUG_MODULE;
				break;
			case 11:
				log_level = BT_EVENT_CLASS_LOG_LEVEL_DEBUG_UNIT;
				break;
			case 12:
				log_level = BT_EVENT_CLASS_LOG_LEVEL_DEBUG_FUNCTION;
				break;
			case 13:
				log_level = BT_EVENT_CLASS_LOG_LEVEL_DEBUG_LINE;
				break;
			case 14:
				log_level = BT_EVENT_CLASS_LOG_LEVEL_DEBUG;
				break;
			default:
				_BT_LOGW_NODE(node, "Not setting event class's log level because its value is unknown: "
					"log-level=%" PRIu64, loglevel_value);
			}

			if (log_level != BT_EVENT_CLASS_LOG_LEVEL_UNSPECIFIED) {
				ret = bt_event_class_set_log_level(
					event_class, log_level);
				if (ret) {
					_BT_LOGE_NODE(node,
						"Cannot set event class's log level.");
					goto error;
				}
			}

			_SET(set, _EVENT_LOGLEVEL_SET);
		} else if (!strcmp(left, "model.emf.uri")) {
			char *right;

			if (_IS_SET(set, _EVENT_MODEL_EMF_URI_SET)) {
				_BT_LOGE_DUP_ATTR(node, "model.emf.uri",
					"event class");
				ret = -EPERM;
				goto error;
			}

			right = concatenate_unary_strings(
				&node->u.ctf_expression.right);
			if (!right) {
				_BT_LOGE_NODE(node,
					"Unexpected unary expression for event class's `model.emf.uri` attribute.");
				ret = -EINVAL;
				goto error;
			}

			if (strlen(right) == 0) {
				_BT_LOGW_NODE(node,
					"Not setting event class's EMF URI because it's empty.");
			} else {
				ret = bt_event_class_set_emf_uri(
					event_class, right);
				if (ret) {
					_BT_LOGE_NODE(node,
						"Cannot set event class's EMF URI.");
					goto error;
				}
			}

			g_free(right);
			_SET(set, _EVENT_MODEL_EMF_URI_SET);
		} else {
			_BT_LOGW_NODE(node,
				"Unknown attribute in event class: "
				"attr-name=\"%s\"", left);
		}

		g_free(left);
		left = NULL;
		break;
	}
	default:
		ret = -EPERM;
		goto error;
	}

	return 0;

error:
	if (left) {
		g_free(left);
	}

	BT_PUT(decl);

	return ret;
}

static
char *get_event_decl_name(struct ctx *ctx, struct ctf_node *node)
{
	char *left = NULL;
	char *name = NULL;
	struct ctf_node *iter;
	struct bt_list_head *decl_list = &node->u.event.declaration_list;

	bt_list_for_each_entry(iter, decl_list, siblings) {
		if (iter->type != NODE_CTF_EXPRESSION) {
			continue;
		}

		left = concatenate_unary_strings(&iter->u.ctf_expression.left);
		if (!left) {
			_BT_LOGE_NODE(iter,
				"Cannot concatenate unary strings.");
			goto error;
		}

		if (!strcmp(left, "name")) {
			name = concatenate_unary_strings(
				&iter->u.ctf_expression.right);
			if (!name) {
				_BT_LOGE_NODE(iter,
					"Unexpected unary expression for event class's `name` attribute.");
				goto error;
			}
		}

		g_free(left);
		left = NULL;

		if (name) {
			break;
		}
	}

	return name;

error:
	g_free(left);

	return NULL;
}

static
int reset_event_decl_types(struct ctx *ctx,
	struct bt_event_class *event_class)
{
	int ret = 0;

	/* Context type. */
	ret = bt_event_class_set_context_type(event_class, NULL);
	if (ret) {
		BT_LOGE("Cannot reset initial event class's context field type: "
			"event-name=\"%s\"",
			bt_event_class_get_name(event_class));
		goto end;
	}

	/* Event payload. */
	ret = bt_event_class_set_payload_type(event_class, NULL);
	if (ret) {
		BT_LOGE("Cannot reset initial event class's payload field type: "
			"event-name=\"%s\"",
			bt_event_class_get_name(event_class));
		goto end;
	}
end:
	return ret;
}

static
int reset_stream_decl_types(struct ctx *ctx,
	struct bt_stream_class *stream_class)
{
	int ret = 0;

	/* Packet context. */
	ret = bt_stream_class_set_packet_context_type(stream_class, NULL);
	if (ret) {
		BT_LOGE_STR("Cannot reset initial stream class's packet context field type.");
		goto end;
	}

	/* Event header. */
	ret = bt_stream_class_set_event_header_type(stream_class, NULL);
	if (ret) {
		BT_LOGE_STR("Cannot reset initial stream class's event header field type.");
		goto end;
	}

	/* Event context. */
	ret = bt_stream_class_set_event_context_type(stream_class, NULL);
	if (ret) {
		BT_LOGE_STR("Cannot reset initial stream class's event context field type.");
		goto end;
	}
end:
	return ret;
}

static
struct bt_stream_class *create_reset_stream_class(struct ctx *ctx)
{
	int ret;
	struct bt_stream_class *stream_class;

	stream_class = bt_stream_class_create_empty(NULL);
	if (!stream_class) {
		BT_LOGE_STR("Cannot create empty stream class.");
		goto error;
	}

	/*
	 * Set packet context, event header, and event context to NULL to
	 * override the default ones.
	 */
	ret = reset_stream_decl_types(ctx, stream_class);
	if (ret) {
		goto error;
	}

	return stream_class;

error:
	BT_PUT(stream_class);

	return NULL;
}

static
int visit_event_decl(struct ctx *ctx, struct ctf_node *node)
{
	int ret = 0;
	int set = 0;
	int64_t event_id;
	struct ctf_node *iter;
	int64_t stream_id = -1;
	char *event_name = NULL;
	struct bt_event_class *event_class = NULL;
	struct bt_event_class *eevent_class;
	struct bt_stream_class *stream_class = NULL;
	struct bt_list_head *decl_list = &node->u.event.declaration_list;
	bool pop_scope = false;

	if (node->visited) {
		goto end;
	}

	node->visited = TRUE;
	event_name = get_event_decl_name(ctx, node);
	if (!event_name) {
		_BT_LOGE_NODE(node,
			"Missing `name` attribute in event class.");
		ret = -EPERM;
		goto error;
	}

	event_class = bt_event_class_create(event_name);

	/*
	 * Unset context and fields to override the default ones.
	 */
	ret = reset_event_decl_types(ctx, event_class);
	if (ret) {
		_BT_LOGE_NODE(node,
			"Cannot reset event class's field types: "
			"ret=%d", ret);
		goto error;
	}

	ret = ctx_push_scope(ctx);
	if (ret) {
		BT_LOGE_STR("Cannot push scope.");
		goto error;
	}

	pop_scope = true;

	bt_list_for_each_entry(iter, decl_list, siblings) {
		ret = visit_event_decl_entry(ctx, iter, event_class,
			&stream_id, &set);
		if (ret) {
			_BT_LOGE_NODE(iter, "Cannot visit event class's entry: "
				"ret=%d", ret);
			goto error;
		}
	}

	if (!_IS_SET(&set, _EVENT_STREAM_ID_SET)) {
		GList *keys = NULL;
		int64_t *new_stream_id;
		struct bt_stream_class *new_stream_class;
		size_t stream_class_count =
			g_hash_table_size(ctx->stream_classes) +
			bt_trace_get_stream_class_count(ctx->trace);

		/*
		 * Allow missing stream_id if there is only a single
		 * stream class.
		 */
		switch (stream_class_count) {
		case 0:
			/* Create implicit stream class if there's none */
			stream_id = 0;
			new_stream_class = create_reset_stream_class(ctx);
			if (!new_stream_class) {
				_BT_LOGE_NODE(node,
					"Cannot create empty stream class.");
				ret = -EINVAL;
				goto error;
			}

			ret = bt_stream_class_set_id(new_stream_class,
				stream_id);
			if (ret) {
				_BT_LOGE_NODE(node,
					"Cannot set stream class's ID: "
					"id=0, ret=%d", ret);
				BT_PUT(new_stream_class);
				goto error;
			}

			new_stream_id = g_new0(int64_t, 1);
			if (!new_stream_id) {
				BT_LOGE_STR("Failed to allocate a int64_t.");
				ret = -ENOMEM;
				goto error;
			}

			*new_stream_id = stream_id;

			/* Move reference to visitor's context */
			g_hash_table_insert(ctx->stream_classes,
				new_stream_id, new_stream_class);
			new_stream_id = NULL;
			new_stream_class = NULL;
			break;
		case 1:
			/* Single stream class: get its ID */
			if (g_hash_table_size(ctx->stream_classes) == 1) {
				keys = g_hash_table_get_keys(ctx->stream_classes);
				stream_id = *((int64_t *) keys->data);
				g_list_free(keys);
			} else {
				assert(bt_trace_get_stream_class_count(
					ctx->trace) == 1);
				stream_class =
					bt_trace_get_stream_class_by_index(
						ctx->trace, 0);
				assert(stream_class);
				stream_id = bt_stream_class_get_id(
					stream_class);
				BT_PUT(stream_class);
			}
			break;
		default:
			_BT_LOGE_NODE(node,
				"Missing `stream_id` attribute in event class.");
			ret = -EPERM;
			goto error;
		}
	}

	assert(stream_id >= 0);

	/* We have the stream ID now; get the stream class if found */
	stream_class = g_hash_table_lookup(ctx->stream_classes, &stream_id);
	bt_get(stream_class);
	if (!stream_class) {
		stream_class = bt_trace_get_stream_class_by_id(ctx->trace,
			stream_id);
		if (!stream_class) {
			_BT_LOGE_NODE(node,
				"Cannot find stream class at this point: "
				"id=%" PRId64, stream_id);
			ret = -EINVAL;
			goto error;
		}
	}

	assert(stream_class);

	if (!_IS_SET(&set, _EVENT_ID_SET)) {
		/* Allow only one event without ID per stream */
		if (bt_stream_class_get_event_class_count(stream_class) !=
				0) {
			_BT_LOGE_NODE(node,
				"Missing `id` attribute in event class.");
			ret = -EPERM;
			goto error;
		}

		/* Automatic ID */
		ret = bt_event_class_set_id(event_class, 0);
		if (ret) {
			_BT_LOGE_NODE(node,
				"Cannot set event class's ID: id=0, ret=%d",
				ret);
			goto error;
		}
	}

	event_id = bt_event_class_get_id(event_class);
	if (event_id < 0) {
		_BT_LOGE_NODE(node, "Cannot get event class's ID.");
		ret = -EINVAL;
		goto error;
	}

	eevent_class = bt_stream_class_get_event_class_by_id(stream_class,
		event_id);
	if (eevent_class) {
		BT_PUT(eevent_class);
		_BT_LOGE_NODE(node,
			"Duplicate event class (same ID) in the same stream class: "
			"id=%" PRId64, event_id);
		ret = -EEXIST;
		goto error;
	}

	ret = bt_stream_class_add_event_class(stream_class, event_class);
	BT_PUT(event_class);
	if (ret) {
		_BT_LOGE_NODE(node,
			"Cannot add event class to stream class: ret=%d", ret);
		goto error;
	}

	goto end;

error:
	bt_put(event_class);

end:
	if (pop_scope) {
		ctx_pop_scope(ctx);
	}

	g_free(event_name);
	bt_put(stream_class);
	return ret;
}

static
int auto_map_field_to_trace_clock_class(struct ctx *ctx,
		struct bt_field_type *ft)
{
	struct bt_clock_class *clock_class_to_map_to = NULL;
	struct bt_clock_class *mapped_clock_class = NULL;
	int ret = 0;
	int64_t clock_class_count;

	if (!ft || !bt_field_type_is_integer(ft)) {
		goto end;
	}

	mapped_clock_class =
		bt_field_type_integer_get_mapped_clock_class(ft);
	if (mapped_clock_class) {
		goto end;
	}

	clock_class_count = bt_trace_get_clock_class_count(ctx->trace);
	assert(clock_class_count >= 0);

	switch (clock_class_count) {
	case 0:
		/*
		 * No clock class exists in the trace at this
		 * point. Create an implicit one at 1 GHz,
		 * named `default`, and use this clock class.
		 */
		clock_class_to_map_to = bt_clock_class_create("default",
			1000000000);
		if (!clock_class_to_map_to) {
			BT_LOGE_STR("Cannot create a clock class.");
			ret = -1;
			goto end;
		}

		ret = bt_trace_add_clock_class(ctx->trace,
			clock_class_to_map_to);
		if (ret) {
			BT_LOGE_STR("Cannot add clock class to trace.");
			goto end;
		}
		break;
	case 1:
		/*
		 * Only one clock class exists in the trace at
		 * this point: use this one.
		 */
		clock_class_to_map_to =
			bt_trace_get_clock_class_by_index(ctx->trace, 0);
		assert(clock_class_to_map_to);
		break;
	default:
		/*
		 * Timestamp field not mapped to a clock class
		 * and there's more than one clock class in the
		 * trace: this is an error.
		 */
		BT_LOGE_STR("Timestamp field found with no mapped clock class, "
			"but there's more than one clock class in the trace at this point.");
		ret = -1;
		goto end;
	}

	assert(clock_class_to_map_to);
	ret = bt_field_type_integer_set_mapped_clock_class(ft,
		clock_class_to_map_to);
	if (ret) {
		BT_LOGE("Cannot map field type's field to trace's clock class: "
			"clock-class-name=\"%s\", ret=%d",
			bt_clock_class_get_name(clock_class_to_map_to),
			ret);
		goto end;
	}

end:
	bt_put(clock_class_to_map_to);
	bt_put(mapped_clock_class);
	return ret;
}

static
int auto_map_fields_to_trace_clock_class(struct ctx *ctx,
		struct bt_field_type *root_ft, const char *field_name)
{
	int ret = 0;
	int64_t i, count;

	if (!root_ft) {
		goto end;
	}

	if (!bt_field_type_is_structure(root_ft) &&
			!bt_field_type_is_variant(root_ft)) {
		goto end;
	}

	if (bt_field_type_is_structure(root_ft)) {
		count = bt_field_type_structure_get_field_count(root_ft);
	} else {
		count = bt_field_type_variant_get_field_count(root_ft);
	}

	assert(count >= 0);

	for (i = 0; i < count; i++) {
		_BT_FIELD_TYPE_INIT(ft);
		const char *name;

		if (bt_field_type_is_structure(root_ft)) {
			ret = bt_field_type_structure_get_field_by_index(
				root_ft, &name, &ft, i);
		} else if (bt_field_type_is_variant(root_ft)) {
			ret = bt_field_type_variant_get_field_by_index(
				root_ft, &name, &ft, i);
		}

		assert(ret == 0);

		if (strcmp(name, field_name) == 0) {
			ret = auto_map_field_to_trace_clock_class(ctx, ft);
			if (ret) {
				BT_LOGE("Cannot automatically map field to trace's clock class: "
					"field-name=\"%s\"", field_name);
				bt_put(ft);
				goto end;
			}
		}

		ret = auto_map_fields_to_trace_clock_class(ctx, ft, field_name);
		bt_put(ft);
		if (ret) {
			BT_LOGE("Cannot automatically map structure or variant field type's fields to trace's clock class: "
				"field-name=\"%s\", root-field-name=\"%s\"",
				field_name, name);
			goto end;
		}
	}

end:
	return ret;
}

static
int visit_stream_decl_entry(struct ctx *ctx, struct ctf_node *node,
	struct bt_stream_class *stream_class, int *set)
{
	int ret = 0;
	char *left = NULL;
	_BT_FIELD_TYPE_INIT(decl);

	switch (node->type) {
	case NODE_TYPEDEF:
		ret = visit_typedef(ctx, node->u._typedef.type_specifier_list,
			&node->u._typedef.type_declarators);
		if (ret) {
			_BT_LOGE_NODE(node,
				"Cannot add type definition found in stream class.");
			goto error;
		}
		break;
	case NODE_TYPEALIAS:
		ret = visit_typealias(ctx, node->u.typealias.target,
			node->u.typealias.alias);
		if (ret) {
			_BT_LOGE_NODE(node,
				"Cannot add type alias found in stream class.");
			goto error;
		}
		break;
	case NODE_CTF_EXPRESSION:
	{
		left = concatenate_unary_strings(&node->u.ctf_expression.left);
		if (!left) {
			_BT_LOGE_NODE(node, "Cannot concatenate unary strings.");
			ret = -EINVAL;
			goto error;
		}

		if (!strcmp(left, "id")) {
			int64_t id;
			gpointer ptr;

			if (_IS_SET(set, _STREAM_ID_SET)) {
				_BT_LOGE_DUP_ATTR(node, "id",
					"stream declaration");
				ret = -EPERM;
				goto error;
			}

			ret = get_unary_unsigned(&node->u.ctf_expression.right,
				(uint64_t *) &id);
			/* Only read "id" if get_unary_unsigned() succeeded. */
			if (ret || (!ret && id < 0)) {
				_BT_LOGE_NODE(node,
					"Unexpected unary expression for stream class's `id` attribute.");
				ret = -EINVAL;
				goto error;
			}

			ptr = g_hash_table_lookup(ctx->stream_classes, &id);
			if (ptr) {
				_BT_LOGE_NODE(node,
					"Duplicate stream class (same ID): id=%" PRId64,
					id);
				ret = -EEXIST;
				goto error;
			}

			ret = bt_stream_class_set_id(stream_class, id);
			if (ret) {
				_BT_LOGE_NODE(node,
					"Cannot set stream class's ID: "
					"id=%" PRId64 ", ret=%d", id, ret);
				goto error;
			}

			_SET(set, _STREAM_ID_SET);
		} else if (!strcmp(left, "event.header")) {
			if (_IS_SET(set, _STREAM_EVENT_HEADER_SET)) {
				_BT_LOGE_NODE(node,
					"Duplicate `event.header` entry in stream class.");
				ret = -EPERM;
				goto error;
			}

			ret = visit_type_specifier_list(ctx,
				_BT_LIST_FIRST_ENTRY(
					&node->u.ctf_expression.right,
					struct ctf_node, siblings),
				&decl);
			if (ret) {
				_BT_LOGE_NODE(node,
					"Cannot create stream class's event header field type.");
				goto error;
			}

			assert(decl);
			ret = auto_map_fields_to_trace_clock_class(ctx,
				decl, "timestamp");
			if (ret) {
				_BT_LOGE_NODE(node,
					"Cannot automatically map specific event header field type fields named `timestamp` to trace's clock class.");
				goto error;
			}

			ret = bt_stream_class_set_event_header_type(
				stream_class, decl);
			BT_PUT(decl);
			if (ret) {
				_BT_LOGE_NODE(node,
					"Cannot set stream class's event header field type.");
				goto error;
			}

			_SET(set, _STREAM_EVENT_HEADER_SET);
		} else if (!strcmp(left, "event.context")) {
			if (_IS_SET(set, _STREAM_EVENT_CONTEXT_SET)) {
				_BT_LOGE_NODE(node,
					"Duplicate `event.context` entry in stream class.");
				ret = -EPERM;
				goto error;
			}

			ret = visit_type_specifier_list(ctx,
				_BT_LIST_FIRST_ENTRY(
					&node->u.ctf_expression.right,
					struct ctf_node, siblings),
				&decl);
			if (ret) {
				_BT_LOGE_NODE(node,
					"Cannot create stream class's event context field type.");
				goto error;
			}

			assert(decl);

			ret = bt_stream_class_set_event_context_type(
				stream_class, decl);
			BT_PUT(decl);
			if (ret) {
				_BT_LOGE_NODE(node,
					"Cannot set stream class's event context field type.");
				goto error;
			}

			_SET(set, _STREAM_EVENT_CONTEXT_SET);
		} else if (!strcmp(left, "packet.context")) {
			if (_IS_SET(set, _STREAM_PACKET_CONTEXT_SET)) {
				_BT_LOGE_NODE(node,
					"Duplicate `packet.context` entry in stream class.");
				ret = -EPERM;
				goto error;
			}

			ret = visit_type_specifier_list(ctx,
				_BT_LIST_FIRST_ENTRY(
					&node->u.ctf_expression.right,
					struct ctf_node, siblings),
				&decl);
			if (ret) {
				_BT_LOGE_NODE(node,
					"Cannot create stream class's packet context field type.");
				goto error;
			}

			assert(decl);
			ret = auto_map_fields_to_trace_clock_class(ctx,
				decl, "timestamp_begin");
			if (ret) {
				_BT_LOGE_NODE(node,
					"Cannot automatically map specific packet context field type fields named `timestamp_begin` to trace's clock class.");
				goto error;
			}

			ret = auto_map_fields_to_trace_clock_class(ctx,
				decl, "timestamp_end");
			if (ret) {
				_BT_LOGE_NODE(node,
					"Cannot automatically map specific packet context field type fields named `timestamp_end` to trace's clock class.");
				goto error;
			}

			ret = bt_stream_class_set_packet_context_type(
				stream_class, decl);
			BT_PUT(decl);
			if (ret) {
				_BT_LOGE_NODE(node,
					"Cannot set stream class's packet context field type.");
				goto error;
			}

			_SET(set, _STREAM_PACKET_CONTEXT_SET);
		} else {
			_BT_LOGW_NODE(node,
				"Unknown attribute in stream class: "
				"attr-name=\"%s\"", left);
		}

		g_free(left);
		left = NULL;
		break;
	}

	default:
		ret = -EPERM;
		goto error;
	}

	return 0;

error:
	g_free(left);
	BT_PUT(decl);

	return ret;
}

static
int visit_stream_decl(struct ctx *ctx, struct ctf_node *node)
{
	int64_t id;
	int64_t *new_id;
	int set = 0;
	int ret = 0;
	struct ctf_node *iter;
	struct bt_stream_class *stream_class = NULL;
	struct bt_stream_class *existing_stream_class = NULL;
	struct bt_list_head *decl_list = &node->u.stream.declaration_list;

	if (node->visited) {
		goto end;
	}

	node->visited = TRUE;
	stream_class = create_reset_stream_class(ctx);
	if (!stream_class) {
		_BT_LOGE_NODE(node, "Cannot create empty stream class.");
		ret = -EINVAL;
		goto error;
	}

	ret = ctx_push_scope(ctx);
	if (ret) {
		BT_LOGE_STR("Cannot push scope.");
		goto error;
	}

	bt_list_for_each_entry(iter, decl_list, siblings) {
		ret = visit_stream_decl_entry(ctx, iter, stream_class, &set);
		if (ret) {
			_BT_LOGE_NODE(iter,
				"Cannot visit stream class's entry: "
				"ret=%d", ret);
			ctx_pop_scope(ctx);
			goto error;
		}
	}

	ctx_pop_scope(ctx);

	if (_IS_SET(&set, _STREAM_ID_SET)) {
		/* Check that packet header has stream_id field */
		_BT_FIELD_TYPE_INIT(stream_id_decl);
		_BT_FIELD_TYPE_INIT(packet_header_decl);

		packet_header_decl =
			bt_trace_get_packet_header_type(ctx->trace);
		if (!packet_header_decl) {
			_BT_LOGE_NODE(node,
				"Stream class has a `id` attribute, "
				"but trace has no packet header field type.");
			goto error;
		}

		stream_id_decl =
			bt_field_type_structure_get_field_type_by_name(
				packet_header_decl, "stream_id");
		BT_PUT(packet_header_decl);
		if (!stream_id_decl) {
			_BT_LOGE_NODE(node,
				"Stream class has a `id` attribute, "
				"but trace's packet header field type has no `stream_id` field.");
			goto error;
		}

		if (!bt_field_type_is_integer(stream_id_decl)) {
			BT_PUT(stream_id_decl);
			_BT_LOGE_NODE(node,
				"Stream class has a `id` attribute, "
				"but trace's packet header field type's `stream_id` field is not an integer field type.");
			goto error;
		}

		BT_PUT(stream_id_decl);
	} else {
		/* Allow only _one_ ID-less stream */
		if (g_hash_table_size(ctx->stream_classes) != 0) {
			_BT_LOGE_NODE(node,
				"Missing `id` attribute in stream class as there's more than one stream class in the trace.");
			ret = -EPERM;
			goto error;
		}

		/* Automatic ID: 0 */
		ret = bt_stream_class_set_id(stream_class, 0);
		assert(ret == 0);
	}

	id = bt_stream_class_get_id(stream_class);
	if (id < 0) {
		_BT_LOGE_NODE(node,
			"Cannot get stream class's ID.");
		ret = -EINVAL;
		goto error;
	}

	/*
	 * Make sure that this stream class's ID is currently unique in
	 * the trace.
	 */
	existing_stream_class = bt_trace_get_stream_class_by_id(ctx->trace,
		id);
	if (g_hash_table_lookup(ctx->stream_classes, &id) ||
			existing_stream_class) {
		_BT_LOGE_NODE(node,
			"Duplicate stream class (same ID): id=%" PRId64,
			id);
		ret = -EINVAL;
		goto error;
	}

	new_id = g_new0(int64_t, 1);
	if (!new_id) {
		BT_LOGE_STR("Failed to allocate a int64_t.");
		ret = -ENOMEM;
		goto error;
	}
	*new_id = id;

	/* Move reference to visitor's context */
	g_hash_table_insert(ctx->stream_classes, new_id,
		stream_class);
	stream_class = NULL;
	goto end;

error:
	bt_put(stream_class);

end:
	bt_put(existing_stream_class);
	return ret;
}

static
int visit_trace_decl_entry(struct ctx *ctx, struct ctf_node *node, int *set)
{
	int ret = 0;
	char *left = NULL;
	_BT_FIELD_TYPE_INIT(packet_header_decl);

	switch (node->type) {
	case NODE_TYPEDEF:
		ret = visit_typedef(ctx, node->u._typedef.type_specifier_list,
			&node->u._typedef.type_declarators);
		if (ret) {
			_BT_LOGE_NODE(node,
				"Cannot add type definition found in trace (`trace` block).");
			goto error;
		}
		break;
	case NODE_TYPEALIAS:
		ret = visit_typealias(ctx, node->u.typealias.target,
			node->u.typealias.alias);
		if (ret) {
			_BT_LOGE_NODE(node,
				"Cannot add type alias found in trace (`trace` block).");
			goto error;
		}
		break;
	case NODE_CTF_EXPRESSION:
	{
		left = concatenate_unary_strings(&node->u.ctf_expression.left);
		if (!left) {
			_BT_LOGE_NODE(node, "Cannot concatenate unary strings.");
			ret = -EINVAL;
			goto error;
		}

		if (!strcmp(left, "major")) {
			if (_IS_SET(set, _TRACE_MAJOR_SET)) {
				_BT_LOGE_DUP_ATTR(node, "major", "trace");
				ret = -EPERM;
				goto error;
			}

			ret = get_unary_unsigned(&node->u.ctf_expression.right,
				&ctx->trace_major);
			if (ret) {
				_BT_LOGE_NODE(node,
					"Unexpected unary expression for trace's `major` attribute.");
				ret = -EINVAL;
				goto error;
			}

			_SET(set, _TRACE_MAJOR_SET);
		} else if (!strcmp(left, "minor")) {
			if (_IS_SET(set, _TRACE_MINOR_SET)) {
				_BT_LOGE_DUP_ATTR(node, "minor", "trace");
				ret = -EPERM;
				goto error;
			}

			ret = get_unary_unsigned(&node->u.ctf_expression.right,
				&ctx->trace_minor);
			if (ret) {
				_BT_LOGE_NODE(node,
					"Unexpected unary expression for trace's `minor` attribute.");
				ret = -EINVAL;
				goto error;
			}

			_SET(set, _TRACE_MINOR_SET);
		} else if (!strcmp(left, "uuid")) {
			if (_IS_SET(set, _TRACE_UUID_SET)) {
				_BT_LOGE_DUP_ATTR(node, "uuid", "trace");
				ret = -EPERM;
				goto error;
			}

			ret = get_unary_uuid(&node->u.ctf_expression.right,
				ctx->trace_uuid);
			if (ret) {
				_BT_LOGE_NODE(node,
					"Invalid trace's `uuid` attribute.");
				goto error;
			}

			ret = bt_trace_set_uuid(ctx->trace, ctx->trace_uuid);
			if (ret) {
				_BT_LOGE_NODE(node, "Cannot set trace's UUID.");
				goto error;
			}

			_SET(set, _TRACE_UUID_SET);
		} else if (!strcmp(left, "byte_order")) {
			/* Native byte order is already known at this stage */
			if (_IS_SET(set, _TRACE_BYTE_ORDER_SET)) {
				_BT_LOGE_DUP_ATTR(node, "byte_order",
					"trace");
				ret = -EPERM;
				goto error;
			}

			_SET(set, _TRACE_BYTE_ORDER_SET);
		} else if (!strcmp(left, "packet.header")) {
			if (_IS_SET(set, _TRACE_PACKET_HEADER_SET)) {
				_BT_LOGE_NODE(node,
					"Duplicate `packet.header` entry in trace.");
				ret = -EPERM;
				goto error;
			}

			ret = visit_type_specifier_list(ctx,
				_BT_LIST_FIRST_ENTRY(
					&node->u.ctf_expression.right,
					struct ctf_node, siblings),
				&packet_header_decl);
			if (ret) {
				_BT_LOGE_NODE(node,
					"Cannot create trace's packet header field type.");
				goto error;
			}

			assert(packet_header_decl);
			ret = bt_trace_set_packet_header_type(ctx->trace,
				packet_header_decl);
			BT_PUT(packet_header_decl);
			if (ret) {
				_BT_LOGE_NODE(node,
					"Cannot set trace's packet header field type.");
				goto error;
			}

			_SET(set, _TRACE_PACKET_HEADER_SET);
		} else {
			_BT_LOGW_NODE(node,
				"Unknown attribute in stream class: "
				"attr-name=\"%s\"", left);
		}

		g_free(left);
		left = NULL;
		break;
	}
	default:
		_BT_LOGE_NODE(node, "Unknown expression in trace.");
		ret = -EINVAL;
		goto error;
	}

	return 0;

error:
	g_free(left);
	BT_PUT(packet_header_decl);

	return ret;
}

static
int visit_trace_decl(struct ctx *ctx, struct ctf_node *node)
{
	int ret = 0;
	int set = 0;
	struct ctf_node *iter;
	struct bt_list_head *decl_list = &node->u.trace.declaration_list;

	if (node->visited) {
		goto end;
	}

	node->visited = TRUE;

	if (ctx->is_trace_visited) {
		_BT_LOGE_NODE(node, "Duplicate trace (`trace` block).");
		ret = -EEXIST;
		goto error;
	}

	ret = ctx_push_scope(ctx);
	if (ret) {
		BT_LOGE_STR("Cannot push scope.");
		goto error;
	}

	bt_list_for_each_entry(iter, decl_list, siblings) {
		ret = visit_trace_decl_entry(ctx, iter, &set);
		if (ret) {
			_BT_LOGE_NODE(iter, "Cannot visit trace's entry (`trace` block): "
				"ret=%d", ret);
			ctx_pop_scope(ctx);
			goto error;
		}
	}

	ctx_pop_scope(ctx);

	if (!_IS_SET(&set, _TRACE_MAJOR_SET)) {
		_BT_LOGE_NODE(node,
			"Missing `major` attribute in trace (`trace` block).");
		ret = -EPERM;
		goto error;
	}

	if (!_IS_SET(&set, _TRACE_MINOR_SET)) {
		_BT_LOGE_NODE(node,
			"Missing `minor` attribute in trace (`trace` block).");
		ret = -EPERM;
		goto error;
	}

	if (!_IS_SET(&set, _TRACE_BYTE_ORDER_SET)) {
		_BT_LOGE_NODE(node,
			"Missing `byte_order` attribute in trace (`trace` block).");
		ret = -EPERM;
		goto error;
	}

	ctx->is_trace_visited = TRUE;

end:
	return 0;

error:
	return ret;
}

static
int visit_env(struct ctx *ctx, struct ctf_node *node)
{
	int ret = 0;
	char *left = NULL;
	struct ctf_node *entry_node;
	struct bt_list_head *decl_list = &node->u.env.declaration_list;

	if (node->visited) {
		goto end;
	}

	node->visited = TRUE;

	bt_list_for_each_entry(entry_node, decl_list, siblings) {
		struct bt_list_head *right_head =
			&entry_node->u.ctf_expression.right;

		if (entry_node->type != NODE_CTF_EXPRESSION) {
			_BT_LOGE_NODE(entry_node,
				"Wrong expression in environment entry: "
				"node-type=%d", entry_node->type);
			ret = -EPERM;
			goto error;
		}

		left = concatenate_unary_strings(
			&entry_node->u.ctf_expression.left);
		if (!left) {
			_BT_LOGE_NODE(entry_node,
				"Cannot get environment entry's name.");
			ret = -EINVAL;
			goto error;
		}

		if (is_unary_string(right_head)) {
			char *right = concatenate_unary_strings(right_head);

			if (!right) {
				_BT_LOGE_NODE(entry_node,
					"Unexpected unary expression for environment entry's value: "
					"name=\"%s\"", left);
				ret = -EINVAL;
				goto error;
			}

			if (strcmp(left, "tracer_name") == 0) {
				if (strncmp(right, "lttng", 5) == 0) {
					BT_LOGI("Detected LTTng trace from `%s` environment value: "
						"tracer-name=\"%s\"",
						left, right);
					ctx->is_lttng = 1;
				}
			}

			ret = bt_trace_set_environment_field_string(
				ctx->trace, left, right);
			g_free(right);

			if (ret) {
				_BT_LOGE_NODE(entry_node,
					"Cannot add string environment entry to trace: "
					"name=\"%s\", ret=%d", left, ret);
				goto error;
			}
		} else if (is_unary_unsigned(right_head) ||
				is_unary_signed(right_head)) {
			int64_t v;

			if (is_unary_unsigned(right_head)) {
				ret = get_unary_unsigned(right_head,
					(uint64_t *) &v);
			} else {
				ret = get_unary_signed(right_head, &v);
			}
			if (ret) {
				_BT_LOGE_NODE(entry_node,
					"Unexpected unary expression for environment entry's value: "
					"name=\"%s\"", left);
				ret = -EINVAL;
				goto error;
			}

			ret = bt_trace_set_environment_field_integer(
				ctx->trace, left, v);
			if (ret) {
				_BT_LOGE_NODE(entry_node,
					"Cannot add integer environment entry to trace: "
					"name=\"%s\", ret=%d", left, ret);
				goto error;
			}
		} else {
			_BT_LOGW_NODE(entry_node,
				"Environment entry has unknown type: "
				"name=\"%s\"", left);
		}

		g_free(left);
		left = NULL;
	}

end:
	return 0;

error:
	g_free(left);

	return ret;
}

static
int set_trace_byte_order(struct ctx *ctx, struct ctf_node *trace_node)
{
	int ret = 0;
	int set = 0;
	char *left = NULL;
	struct ctf_node *node;
	struct bt_list_head *decl_list = &trace_node->u.trace.declaration_list;

	bt_list_for_each_entry(node, decl_list, siblings) {
		if (node->type == NODE_CTF_EXPRESSION) {
			struct ctf_node *right_node;

			left = concatenate_unary_strings(
				&node->u.ctf_expression.left);
			if (!left) {
				_BT_LOGE_NODE(node,
					"Cannot concatenate unary strings.");
				ret = -EINVAL;
				goto error;
			}

			if (!strcmp(left, "byte_order")) {
				enum bt_byte_order bo;

				if (_IS_SET(&set, _TRACE_BYTE_ORDER_SET)) {
					_BT_LOGE_DUP_ATTR(node, "byte_order",
						"trace");
					ret = -EPERM;
					goto error;
				}

				_SET(&set, _TRACE_BYTE_ORDER_SET);
				right_node = _BT_LIST_FIRST_ENTRY(
					&node->u.ctf_expression.right,
					struct ctf_node, siblings);
				bo = byte_order_from_unary_expr(right_node);
				if (bo == BT_BYTE_ORDER_UNKNOWN) {
					_BT_LOGE_NODE(node,
						"Invalid `byte_order` attribute in trace (`trace` block): "
						"expecting `le`, `be`, or `network`.");
					ret = -EINVAL;
					goto error;
				} else if (bo == BT_BYTE_ORDER_NATIVE) {
					_BT_LOGE_NODE(node,
						"Invalid `byte_order` attribute in trace (`trace` block): "
						"cannot be set to `native` here.");
					ret = -EPERM;
					goto error;
				}

				ctx->trace_bo = bo;
				ret = bt_trace_set_native_byte_order(
					ctx->trace, bo);
				if (ret) {
					_BT_LOGE_NODE(node,
						"Cannot set trace's byte order: "
						"ret=%d", ret);
					goto error;
				}
			}

			g_free(left);
			left = NULL;
		}
	}

	if (!_IS_SET(&set, _TRACE_BYTE_ORDER_SET)) {
		_BT_LOGE_NODE(trace_node,
			"Missing `byte_order` attribute in trace (`trace` block).");
		ret = -EINVAL;
		goto error;
	}

	return 0;

error:
	g_free(left);

	return ret;
}

static
int visit_clock_decl_entry(struct ctx *ctx, struct ctf_node *entry_node,
	struct bt_clock_class *clock, int *set)
{
	int ret = 0;
	char *left = NULL;

	if (entry_node->type != NODE_CTF_EXPRESSION) {
		_BT_LOGE_NODE(entry_node,
			"Unexpected node type: node-type=%d",
			entry_node->type);
		ret = -EPERM;
		goto error;
	}

	left = concatenate_unary_strings(&entry_node->u.ctf_expression.left);
	if (!left) {
		_BT_LOGE_NODE(entry_node, "Cannot concatenate unary strings.");
		ret = -EINVAL;
		goto error;
	}

	if (!strcmp(left, "name")) {
		char *right;

		if (_IS_SET(set, _CLOCK_NAME_SET)) {
			_BT_LOGE_DUP_ATTR(entry_node, "name", "clock class");
			ret = -EPERM;
			goto error;
		}

		right = concatenate_unary_strings(
			&entry_node->u.ctf_expression.right);
		if (!right) {
			_BT_LOGE_NODE(entry_node,
				"Unexpected unary expression for clock class's `name` attribute.");
			ret = -EINVAL;
			goto error;
		}

		ret = bt_clock_class_set_name(clock, right);
		if (ret) {
			_BT_LOGE_NODE(entry_node,
				"cannot set clock class's name");
			g_free(right);
			goto error;
		}

		g_free(right);
		_SET(set, _CLOCK_NAME_SET);
	} else if (!strcmp(left, "uuid")) {
		unsigned char uuid[BABELTRACE_UUID_LEN];

		if (_IS_SET(set, _CLOCK_UUID_SET)) {
			_BT_LOGE_DUP_ATTR(entry_node, "uuid", "clock class");
			ret = -EPERM;
			goto error;
		}

		ret = get_unary_uuid(&entry_node->u.ctf_expression.right, uuid);
		if (ret) {
			_BT_LOGE_NODE(entry_node,
				"Invalid clock class's `uuid` attribute.");
			goto error;
		}

		ret = bt_clock_class_set_uuid(clock, uuid);
		if (ret) {
			_BT_LOGE_NODE(entry_node,
				"Cannot set clock class's UUID.");
			goto error;
		}

		_SET(set, _CLOCK_UUID_SET);
	} else if (!strcmp(left, "description")) {
		char *right;

		if (_IS_SET(set, _CLOCK_DESCRIPTION_SET)) {
			_BT_LOGE_DUP_ATTR(entry_node, "description",
				"clock class");
			ret = -EPERM;
			goto error;
		}

		right = concatenate_unary_strings(
			&entry_node->u.ctf_expression.right);
		if (!right) {
			_BT_LOGE_NODE(entry_node,
				"Unexpected unary expression for clock class's `description` attribute.");
			ret = -EINVAL;
			goto error;
		}

		ret = bt_clock_class_set_description(clock, right);
		if (ret) {
			_BT_LOGE_NODE(entry_node,
				"Cannot set clock class's description.");
			g_free(right);
			goto error;
		}

		g_free(right);
		_SET(set, _CLOCK_DESCRIPTION_SET);
	} else if (!strcmp(left, "freq")) {
		uint64_t freq;

		if (_IS_SET(set, _CLOCK_FREQ_SET)) {
			_BT_LOGE_DUP_ATTR(entry_node, "freq", "clock class");
			ret = -EPERM;
			goto error;
		}

		ret = get_unary_unsigned(
			&entry_node->u.ctf_expression.right, &freq);
		if (ret) {
			_BT_LOGE_NODE(entry_node,
				"Unexpected unary expression for clock class's `freq` attribute.");
			ret = -EINVAL;
			goto error;
		}

		if (freq == -1ULL || freq == 0) {
			_BT_LOGE_NODE(entry_node,
				"Invalid clock class frequency: freq=%" PRIu64,
				freq);
			ret = -EINVAL;
			goto error;
		}

		ret = bt_clock_class_set_frequency(clock, freq);
		if (ret) {
			_BT_LOGE_NODE(entry_node,
				"Cannot set clock class's frequency.");
			goto error;
		}

		_SET(set, _CLOCK_FREQ_SET);
	} else if (!strcmp(left, "precision")) {
		uint64_t precision;

		if (_IS_SET(set, _CLOCK_PRECISION_SET)) {
			_BT_LOGE_DUP_ATTR(entry_node, "precision",
				"clock class");
			ret = -EPERM;
			goto error;
		}

		ret = get_unary_unsigned(
			&entry_node->u.ctf_expression.right, &precision);
		if (ret) {
			_BT_LOGE_NODE(entry_node,
				"Unexpected unary expression for clock class's `precision` attribute.");
			ret = -EINVAL;
			goto error;
		}

		ret = bt_clock_class_set_precision(clock, precision);
		if (ret) {
			_BT_LOGE_NODE(entry_node,
				"Cannot set clock class's precision.");
			goto error;
		}

		_SET(set, _CLOCK_PRECISION_SET);
	} else if (!strcmp(left, "offset_s")) {
		int64_t offset_s;

		if (_IS_SET(set, _CLOCK_OFFSET_S_SET)) {
			_BT_LOGE_DUP_ATTR(entry_node, "offset_s",
				"clock class");
			ret = -EPERM;
			goto error;
		}

		ret = get_unary_signed(
			&entry_node->u.ctf_expression.right, &offset_s);
		if (ret) {
			_BT_LOGE_NODE(entry_node,
				"Unexpected unary expression for clock class's `offset_s` attribute.");
			ret = -EINVAL;
			goto error;
		}

		ret = bt_clock_class_set_offset_s(clock, offset_s);
		if (ret) {
			_BT_LOGE_NODE(entry_node,
				"Cannot set clock class's offset in seconds.");
			goto error;
		}

		_SET(set, _CLOCK_OFFSET_S_SET);
	} else if (!strcmp(left, "offset")) {
		int64_t offset;

		if (_IS_SET(set, _CLOCK_OFFSET_SET)) {
			_BT_LOGE_DUP_ATTR(entry_node, "offset", "clock class");
			ret = -EPERM;
			goto error;
		}

		ret = get_unary_signed(
			&entry_node->u.ctf_expression.right, &offset);
		if (ret) {
			_BT_LOGE_NODE(entry_node,
				"Unexpected unary expression for clock class's `offset` attribute.");
			ret = -EINVAL;
			goto error;
		}

		ret = bt_clock_class_set_offset_cycles(clock, offset);
		if (ret) {
			_BT_LOGE_NODE(entry_node,
				"Cannot set clock class's offset in cycles.");
			goto error;
		}

		_SET(set, _CLOCK_OFFSET_SET);
	} else if (!strcmp(left, "absolute")) {
		struct ctf_node *right;

		if (_IS_SET(set, _CLOCK_ABSOLUTE_SET)) {
			_BT_LOGE_DUP_ATTR(entry_node, "absolute",
				"clock class");
			ret = -EPERM;
			goto error;
		}

		right = _BT_LIST_FIRST_ENTRY(
			&entry_node->u.ctf_expression.right,
			struct ctf_node, siblings);
		ret = get_boolean(right);
		if (ret < 0) {
			_BT_LOGE_NODE(entry_node,
				"Unexpected unary expression for clock class's `absolute` attribute.");
			ret = -EINVAL;
			goto error;
		}

		ret = bt_clock_class_set_is_absolute(clock, ret);
		if (ret) {
			_BT_LOGE_NODE(entry_node,
				"Cannot set clock class's absolute flag.");
			goto error;
		}

		_SET(set, _CLOCK_ABSOLUTE_SET);
	} else {
		_BT_LOGW_NODE(entry_node,
			"Unknown attribute in clock class: attr-name=\"%s\"",
			left);
	}

	g_free(left);
	left = NULL;

	return 0;

error:
	g_free(left);

	return ret;
}

static
int64_t cycles_from_ns(uint64_t frequency, int64_t ns)
{
	int64_t cycles;

	/* 1GHz */
	if (frequency == 1000000000ULL) {
		cycles = ns;
	} else {
		cycles = (uint64_t) (((double) ns * (double) frequency) / 1e9);
	}

	return cycles;
}

static
int apply_clock_class_offset(struct ctx *ctx, struct bt_clock_class *clock)
{
	int ret;
	uint64_t freq;
	int64_t offset_cycles;
	int64_t offset_to_apply;

	freq = bt_clock_class_get_frequency(clock);
	if (freq == -1ULL) {
		BT_LOGE_STR("Cannot get clock class's frequency.");
		ret = -1;
		goto end;
	}

	ret = bt_clock_class_get_offset_cycles(clock, &offset_cycles);
	if (ret) {
		BT_LOGE_STR("Cannot get clock class's offset in cycles.");
		ret = -1;
		goto end;
	}

	offset_to_apply =
		ctx->decoder_config.clock_class_offset_s * 1000000000LL +
		ctx->decoder_config.clock_class_offset_ns;
	offset_cycles += cycles_from_ns(freq, offset_to_apply);
	ret = bt_clock_class_set_offset_cycles(clock, offset_cycles);

end:
	return ret;
}

static
int visit_clock_decl(struct ctx *ctx, struct ctf_node *clock_node)
{
	int ret = 0;
	int set = 0;
	struct bt_clock_class *clock;
	struct ctf_node *entry_node;
	struct bt_list_head *decl_list = &clock_node->u.clock.declaration_list;
	const char *clock_class_name;

	if (clock_node->visited) {
		return 0;
	}

	clock_node->visited = TRUE;

	/* CTF 1.8's default frequency for a clock class is 1 GHz */
	clock = bt_clock_class_create(NULL, 1000000000);
	if (!clock) {
		_BT_LOGE_NODE(clock_node,
			"Cannot create default clock class.");
		ret = -ENOMEM;
		goto error;
	}

	bt_list_for_each_entry(entry_node, decl_list, siblings) {
		ret = visit_clock_decl_entry(ctx, entry_node, clock, &set);
		if (ret) {
			_BT_LOGE_NODE(entry_node,
				"Cannot visit clock class's entry: ret=%d",
				ret);
			goto error;
		}
	}

	if (!_IS_SET(&set, _CLOCK_NAME_SET)) {
		_BT_LOGE_NODE(clock_node,
			"Missing `name` attribute in clock class.");
		ret = -EPERM;
		goto error;
	}

	clock_class_name = bt_clock_class_get_name(clock);
	assert(clock_class_name);
	if (ctx->is_lttng && strcmp(clock_class_name, "monotonic") == 0) {
		/*
		 * Old versions of LTTng forgot to set its clock class
		 * as absolute, even if it is. This is important because
		 * it's a condition to be able to sort notifications
		 * from different sources.
		 */
		ret = bt_clock_class_set_is_absolute(clock, 1);
		if (ret) {
			_BT_LOGE_NODE(clock_node,
				"Cannot set clock class's absolute flag.");
			goto error;
		}
	}

	ret = apply_clock_class_offset(ctx, clock);
	if (ret) {
		_BT_LOGE_NODE(clock_node,
			"Cannot apply clock class's custom offset.");
		goto error;
	}

	ret = bt_trace_add_clock_class(ctx->trace, clock);
	if (ret) {
		_BT_LOGE_NODE(clock_node,
			"Cannot add clock class to trace.");
		goto error;
	}

error:
	BT_PUT(clock);

	return ret;
}

static
int visit_root_decl(struct ctx *ctx, struct ctf_node *root_decl_node)
{
	int ret = 0;

	if (root_decl_node->visited) {
		goto end;
	}

	root_decl_node->visited = TRUE;

	switch (root_decl_node->type) {
	case NODE_TYPEDEF:
		ret = visit_typedef(ctx,
			root_decl_node->u._typedef.type_specifier_list,
			&root_decl_node->u._typedef.type_declarators);
		if (ret) {
			_BT_LOGE_NODE(root_decl_node,
				"Cannot add type definition found in root scope.");
			goto end;
		}
		break;
	case NODE_TYPEALIAS:
		ret = visit_typealias(ctx, root_decl_node->u.typealias.target,
			root_decl_node->u.typealias.alias);
		if (ret) {
			_BT_LOGE_NODE(root_decl_node,
				"Cannot add type alias found in root scope.");
			goto end;
		}
		break;
	case NODE_TYPE_SPECIFIER_LIST:
	{
		_BT_FIELD_TYPE_INIT(decl);

		/*
		 * Just add the type specifier to the root
		 * declaration scope. Put local reference.
		 */
		ret = visit_type_specifier_list(ctx, root_decl_node, &decl);
		if (ret) {
			_BT_LOGE_NODE(root_decl_node,
				"Cannot visit root scope's field type: "
				"ret=%d", ret);
			assert(!decl);
			goto end;
		}

		BT_PUT(decl);
		break;
	}
	default:
		_BT_LOGE_NODE(root_decl_node,
			"Unexpected node type: node-type=%d",
			root_decl_node->type);
		ret = -EPERM;
		goto end;
	}

end:
	return ret;
}

static
int set_trace_name(struct ctx *ctx)
{
	GString *name;
	int ret = 0;
	struct bt_value *value = NULL;

	assert(bt_trace_get_stream_class_count(ctx->trace) == 0);
	name = g_string_new(NULL);
	if (!name) {
		BT_LOGE_STR("Failed to allocate a GString.");
		ret = -1;
		goto end;
	}

	/*
	 * Check if we have a trace environment string value named `hostname`.
	 * If so, use it as the trace name's prefix.
	 */
	value = bt_trace_get_environment_field_value_by_name(ctx->trace,
		"hostname");
	if (bt_value_is_string(value)) {
		const char *hostname;

		ret = bt_value_string_get(value, &hostname);
		assert(ret == 0);
		g_string_append(name, hostname);

		if (ctx->trace_name_suffix) {
			g_string_append_c(name, G_DIR_SEPARATOR);
		}
	}

	if (ctx->trace_name_suffix) {
		g_string_append(name, ctx->trace_name_suffix);
	}

	ret = bt_trace_set_name(ctx->trace, name->str);
	if (ret) {
		BT_LOGE("Cannot set trace's name: name=\"%s\"", name->str);
		goto error;
	}

	goto end;

error:
	ret = -1;

end:
	bt_put(value);

	if (name) {
		g_string_free(name, TRUE);
	}

	return ret;
}

static
int move_ctx_stream_classes_to_trace(struct ctx *ctx)
{
	int ret = 0;
	GHashTableIter iter;
	gpointer key, stream_class;

	if (g_hash_table_size(ctx->stream_classes) > 0 &&
			bt_trace_get_stream_class_count(ctx->trace) == 0) {
		/*
		 * We're about to add the first stream class to the
		 * trace. This will freeze the trace, and after this
		 * we cannot set the name anymore. At this point,
		 * set the trace name.
		 */
		ret = set_trace_name(ctx);
		if (ret) {
			BT_LOGE_STR("Cannot set trace's name.");
			goto end;
		}
	}

	g_hash_table_iter_init(&iter, ctx->stream_classes);

	while (g_hash_table_iter_next(&iter, &key, &stream_class)) {
		ret = bt_trace_add_stream_class(ctx->trace,
			stream_class);
		if (ret) {
			int64_t id = bt_stream_class_get_id(stream_class);
			BT_LOGE("Cannot add stream class to trace: id=%" PRId64,
				id);
			goto end;
		}
	}

	g_hash_table_remove_all(ctx->stream_classes);

end:
	return ret;
}

BT_HIDDEN
struct ctf_visitor_generate_ir *ctf_visitor_generate_ir_create(
		const struct ctf_metadata_decoder_config *decoder_config,
		const char *name)
{
	int ret;
	struct ctx *ctx = NULL;
	struct bt_trace *trace;

	trace = bt_trace_create();
	if (!trace) {
		BT_LOGE_STR("Cannot create empty trace.");
		goto error;
	}

	/* Set packet header to NULL to override the default one */
	ret = bt_trace_set_packet_header_type(trace, NULL);
	if (ret) {
		BT_LOGE_STR("Cannot reset initial trace's packet header field type.");
		goto error;
	}

	/* Create visitor's context */
	ctx = ctx_create(trace, decoder_config, name);
	if (!ctx) {
		BT_LOGE_STR("Cannot create visitor's context.");
		goto error;
	}

	trace = NULL;
	goto end;

error:
	ctx_destroy(ctx);
	ctx = NULL;

end:
	bt_put(trace);
	return (void *) ctx;
}

BT_HIDDEN
void ctf_visitor_generate_ir_destroy(struct ctf_visitor_generate_ir *visitor)
{
	ctx_destroy((void *) visitor);
}

BT_HIDDEN
struct bt_trace *ctf_visitor_generate_ir_get_trace(
		struct ctf_visitor_generate_ir *visitor)
{
	struct ctx *ctx = (void *) visitor;

	assert(ctx);
	assert(ctx->trace);
	return bt_get(ctx->trace);
}

BT_HIDDEN
int ctf_visitor_generate_ir_visit_node(struct ctf_visitor_generate_ir *visitor,
		struct ctf_node *node)
{
	int ret = 0;
	struct ctx *ctx = (void *) visitor;

	BT_LOGI_STR("Visiting metadata's AST to generate CTF IR objects.");

	switch (node->type) {
	case NODE_ROOT:
	{
		struct ctf_node *iter;
		int got_trace_decl = FALSE;

		/*
		 * The first thing we need is the native byte order of
		 * the trace block, because early type aliases can have
		 * a `byte_order` attribute set to `native`. If we don't
		 * have the native byte order yet, and we don't have any
		 * trace block yet, then fail with EINCOMPLETE.
		 */
		if (ctx->trace_bo == BT_BYTE_ORDER_NATIVE) {
			bt_list_for_each_entry(iter, &node->u.root.trace, siblings) {
				if (got_trace_decl) {
					_BT_LOGE_NODE(node,
						"Duplicate trace (`trace` block).");
					ret = -1;
					goto end;
				}

				ret = set_trace_byte_order(ctx, iter);
				if (ret) {
					_BT_LOGE_NODE(node,
						"Cannot set trace's native byte order: "
						"ret=%d", ret);
					goto end;
				}

				got_trace_decl = TRUE;
			}

			if (!got_trace_decl) {
				BT_LOGD_STR("Incomplete AST: need trace (`trace` block).");
				ret = -EINCOMPLETE;
				goto end;
			}
		}

		assert(ctx->trace_bo == BT_BYTE_ORDER_LITTLE_ENDIAN ||
			ctx->trace_bo == BT_BYTE_ORDER_BIG_ENDIAN);
		assert(ctx->current_scope &&
			ctx->current_scope->parent_scope == NULL);

		/* Environment */
		bt_list_for_each_entry(iter, &node->u.root.env, siblings) {
			ret = visit_env(ctx, iter);
			if (ret) {
				_BT_LOGE_NODE(iter,
					"Cannot visit trace's environment (`env` block) entry: "
					"ret=%d", ret);
				goto end;
			}
		}

		assert(ctx->current_scope &&
			ctx->current_scope->parent_scope == NULL);

		/*
		 * Visit clock blocks.
		 */
		bt_list_for_each_entry(iter, &node->u.root.clock, siblings) {
			ret = visit_clock_decl(ctx, iter);
			if (ret) {
				_BT_LOGE_NODE(iter,
					"Cannot visit clock class: ret=%d",
					ret);
				goto end;
			}
		}

		assert(ctx->current_scope &&
			ctx->current_scope->parent_scope == NULL);

		/*
		 * Visit root declarations next, as they can be used by any
		 * following entity.
		 */
		bt_list_for_each_entry(iter, &node->u.root.declaration_list,
				siblings) {
			ret = visit_root_decl(ctx, iter);
			if (ret) {
				_BT_LOGE_NODE(iter,
					"Cannot visit root entry: ret=%d",
					ret);
				goto end;
			}
		}

		assert(ctx->current_scope &&
			ctx->current_scope->parent_scope == NULL);

		/* Callsite blocks are not supported */
		bt_list_for_each_entry(iter, &node->u.root.callsite, siblings) {
			_BT_LOGW_NODE(iter,
				"\"callsite\" blocks are not supported as of this version.");
		}

		assert(ctx->current_scope &&
			ctx->current_scope->parent_scope == NULL);

		/* Trace */
		bt_list_for_each_entry(iter, &node->u.root.trace, siblings) {
			ret = visit_trace_decl(ctx, iter);
			if (ret) {
				_BT_LOGE_NODE(iter,
					"Cannot visit trace (`trace` block): "
					"ret=%d", ret);
				goto end;
			}
		}

		assert(ctx->current_scope &&
			ctx->current_scope->parent_scope == NULL);

		/* Streams */
		bt_list_for_each_entry(iter, &node->u.root.stream, siblings) {
			ret = visit_stream_decl(ctx, iter);
			if (ret) {
				_BT_LOGE_NODE(iter,
					"Cannot visit stream class: ret=%d",
					ret);
				goto end;
			}
		}

		assert(ctx->current_scope &&
			ctx->current_scope->parent_scope == NULL);

		/* Events */
		bt_list_for_each_entry(iter, &node->u.root.event, siblings) {
			ret = visit_event_decl(ctx, iter);
			if (ret) {
				_BT_LOGE_NODE(iter,
					"Cannot visit event class: ret=%d",
					ret);
				goto end;
			}
		}

		assert(ctx->current_scope &&
			ctx->current_scope->parent_scope == NULL);
		break;
	}
	default:
		_BT_LOGE_NODE(node,
			"Unexpected node type: node-type=%d",
			node->type);
		ret = -EINVAL;
		goto end;
	}

	/* Move decoded stream classes to trace, if any */
	ret = move_ctx_stream_classes_to_trace(ctx);
	if (ret) {
		BT_LOGE("Cannot move stream classes to trace: ret=%d", ret);
	}

end:
	return ret;
}
