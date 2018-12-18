/*
 * ctf-visitor-generate-ir.c
 *
 * Common Trace Format metadata visitor (generates CTF IR objects).
 *
 * Based on older ctf-visitor-generate-io-struct.c.
 *
 * Copyright 2010 - Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
 * Copyright 2015-2018 - Philippe Proulx <philippe.proulx@efficios.com>
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
#include <babeltrace/assert-internal.h>
#include <glib.h>
#include <inttypes.h>
#include <errno.h>
#include <babeltrace/common-internal.h>
#include <babeltrace/compat/uuid-internal.h>
#include <babeltrace/endian-internal.h>
#include <babeltrace/babeltrace.h>

#include "scanner.h"
#include "parser.h"
#include "ast.h"
#include "decoder.h"
#include "ctf-meta.h"
#include "ctf-meta-visitors.h"

/* Bit value (left shift) */
#define _BV(_val)		(1 << (_val))

/* Bit is set in a set of bits */
#define _IS_SET(_set, _mask)	(*(_set) & (_mask))

/* Set bit in a set of bits */
#define _SET(_set, _mask)	(*(_set) |= (_mask))

/* Try to push scope, or go to the `error` label */
#define _TRY_PUSH_SCOPE_OR_GOTO_ERROR()					\
	do {								\
		ret = ctx_push_scope(ctx);				\
		if (ret) {						\
			BT_LOGE_STR("Cannot push scope.");		\
			goto error;					\
		}							\
	} while (0)

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
	_EVENT_LOG_LEVEL_SET =		_BV(4),
	_EVENT_CONTEXT_SET =		_BV(5),
	_EVENT_FIELDS_SET =		_BV(6),
};

enum loglevel {
        LOG_LEVEL_EMERG                  = 0,
        LOG_LEVEL_ALERT                  = 1,
        LOG_LEVEL_CRIT                   = 2,
        LOG_LEVEL_ERR                    = 3,
        LOG_LEVEL_WARNING                = 4,
        LOG_LEVEL_NOTICE                 = 5,
        LOG_LEVEL_INFO                   = 6,
        LOG_LEVEL_DEBUG_SYSTEM           = 7,
        LOG_LEVEL_DEBUG_PROGRAM          = 8,
        LOG_LEVEL_DEBUG_PROCESS          = 9,
        LOG_LEVEL_DEBUG_MODULE           = 10,
        LOG_LEVEL_DEBUG_UNIT             = 11,
        LOG_LEVEL_DEBUG_FUNCTION         = 12,
        LOG_LEVEL_DEBUG_LINE             = 13,
        LOG_LEVEL_DEBUG                  = 14,
	_NR_LOGLEVELS			= 15,
};

/* Prefixes of class aliases */
#define _PREFIX_ALIAS			'a'
#define _PREFIX_ENUM			'e'
#define _PREFIX_STRUCT			's'
#define _PREFIX_VARIANT			'v'

/* First entry in a BT list */
#define _BT_LIST_FIRST_ENTRY(_ptr, _class, _member)			\
	bt_list_entry((_ptr)->next, _class, _member)

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
	 * Alias name to field class.
	 *
	 * GQuark -> struct ctf_field_class * (owned by this)
	 */
	GHashTable *decl_map;

	/* Parent scope; NULL if this is the root declaration scope */
	struct ctx_decl_scope *parent_scope;
};

/*
 * Visitor context (private).
 */
struct ctx {
	/* Trace IR trace class being filled (owned by this) */
	bt_trace_class *trace_class;

	/* CTF meta trace being filled (owned by this) */
	struct ctf_trace_class *ctf_tc;

	/* Current declaration scope (top of the stack) (owned by this) */
	struct ctx_decl_scope *current_scope;

	/* True if trace declaration is visited */
	bool is_trace_visited;

	/* True if this is an LTTng trace */
	bool is_lttng;

	/* Config passed by the user */
	struct ctf_metadata_decoder_config decoder_config;
};

/*
 * Visitor (public).
 */
struct ctf_visitor_generate_ir;

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
		NULL, (GDestroyNotify) ctf_field_class_destroy);
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

	BT_ASSERT(name);

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
 * Looks up a prefixed class alias within a declaration scope.
 *
 * @param scope		Declaration scope
 * @param prefix	Prefix character
 * @param name		Alias name
 * @param levels	Number of levels to dig into (-1 means infinite)
 * @param copy		True to return a copy
 * @returns		Declaration (owned by caller if \p copy is true),
 *			or NULL if not found
 */
static
struct ctf_field_class *ctx_decl_scope_lookup_prefix_alias(
		struct ctx_decl_scope *scope, char prefix, const char *name,
		int levels, bool copy)
{
	GQuark qname = 0;
	int cur_levels = 0;
	struct ctf_field_class *decl = NULL;
	struct ctx_decl_scope *cur_scope = scope;

	BT_ASSERT(scope);
	BT_ASSERT(name);
	qname = get_prefixed_named_quark(prefix, name);
	if (!qname) {
		goto end;
	}

	if (levels < 0) {
		levels = INT_MAX;
	}

	while (cur_scope && cur_levels < levels) {
		decl = g_hash_table_lookup(cur_scope->decl_map,
			(gconstpointer) GUINT_TO_POINTER(qname));
		if (decl) {
			/* Caller's reference */
			if (copy) {
				decl = ctf_field_class_copy(decl);
				BT_ASSERT(decl);
			}

			goto end;
		}

		cur_scope = cur_scope->parent_scope;
		cur_levels++;
	}

end:
	return decl;
}

/**
 * Looks up a class alias within a declaration scope.
 *
 * @param scope		Declaration scope
 * @param name		Alias name
 * @param levels	Number of levels to dig into (-1 means infinite)
 * @param copy		True to return a copy
 * @returns		Declaration (owned by caller if \p copy is true),
 *			or NULL if not found
 */
static
struct ctf_field_class *ctx_decl_scope_lookup_alias(
		struct ctx_decl_scope *scope, const char *name, int levels,
		bool copy)
{
	return ctx_decl_scope_lookup_prefix_alias(scope, _PREFIX_ALIAS,
		name, levels, copy);
}

/**
 * Looks up an enumeration within a declaration scope.
 *
 * @param scope		Declaration scope
 * @param name		Enumeration name
 * @param levels	Number of levels to dig into (-1 means infinite)
 * @param copy		True to return a copy
 * @returns		Declaration (owned by caller if \p copy is true),
 *			or NULL if not found
 */
static
struct ctf_field_class_enum *ctx_decl_scope_lookup_enum(
		struct ctx_decl_scope *scope, const char *name, int levels,
		bool copy)
{
	return (void *) ctx_decl_scope_lookup_prefix_alias(scope, _PREFIX_ENUM,
		name, levels, copy);
}

/**
 * Looks up a structure within a declaration scope.
 *
 * @param scope		Declaration scope
 * @param name		Structure name
 * @param levels	Number of levels to dig into (-1 means infinite)
 * @param copy		True to return a copy
 * @returns		Declaration (owned by caller if \p copy is true),
 *			or NULL if not found
 */
static
struct ctf_field_class_struct *ctx_decl_scope_lookup_struct(
		struct ctx_decl_scope *scope, const char *name, int levels,
		bool copy)
{
	return (void *) ctx_decl_scope_lookup_prefix_alias(scope,
		_PREFIX_STRUCT, name, levels, copy);
}

/**
 * Looks up a variant within a declaration scope.
 *
 * @param scope		Declaration scope
 * @param name		Variant name
 * @param levels	Number of levels to dig into (-1 means infinite)
 * @param copy		True to return a copy
 * @returns		Declaration (owned by caller if \p copy is true),
 *			or NULL if not found
 */
static
struct ctf_field_class_variant *ctx_decl_scope_lookup_variant(
		struct ctx_decl_scope *scope, const char *name, int levels,
		bool copy)
{
	return (void *) ctx_decl_scope_lookup_prefix_alias(scope,
		_PREFIX_VARIANT, name, levels, copy);
}

/**
 * Registers a prefixed class alias within a declaration scope.
 *
 * @param scope		Declaration scope
 * @param prefix	Prefix character
 * @param name		Alias name (non-NULL)
 * @param decl		Field class to register (copied)
 * @returns		0 if registration went okay, negative value otherwise
 */
static
int ctx_decl_scope_register_prefix_alias(struct ctx_decl_scope *scope,
		char prefix, const char *name, struct ctf_field_class *decl)
{
	int ret = 0;
	GQuark qname = 0;

	BT_ASSERT(scope);
	BT_ASSERT(name);
	BT_ASSERT(decl);
	qname = get_prefixed_named_quark(prefix, name);
	if (!qname) {
		ret = -ENOMEM;
		goto end;
	}

	/* Make sure alias does not exist in local scope */
	if (ctx_decl_scope_lookup_prefix_alias(scope, prefix, name, 1,
			false)) {
		ret = -EEXIST;
		goto end;
	}

	decl = ctf_field_class_copy(decl);
	BT_ASSERT(decl);
	g_hash_table_insert(scope->decl_map, GUINT_TO_POINTER(qname), decl);

end:
	return ret;
}

/**
 * Registers a class alias within a declaration scope.
 *
 * @param scope	Declaration scope
 * @param name	Alias name (non-NULL)
 * @param decl	Field class to register (copied)
 * @returns	0 if registration went okay, negative value otherwise
 */
static
int ctx_decl_scope_register_alias(struct ctx_decl_scope *scope,
		const char *name, struct ctf_field_class *decl)
{
	return ctx_decl_scope_register_prefix_alias(scope, _PREFIX_ALIAS,
		name, (void *) decl);
}

/**
 * Registers an enumeration declaration within a declaration scope.
 *
 * @param scope	Declaration scope
 * @param name	Enumeration name (non-NULL)
 * @param decl	Enumeration field class to register (copied)
 * @returns	0 if registration went okay, negative value otherwise
 */
static
int ctx_decl_scope_register_enum(struct ctx_decl_scope *scope,
		const char *name, struct ctf_field_class_enum *decl)
{
	return ctx_decl_scope_register_prefix_alias(scope, _PREFIX_ENUM,
		name, (void *) decl);
}

/**
 * Registers a structure declaration within a declaration scope.
 *
 * @param scope	Declaration scope
 * @param name	Structure name (non-NULL)
 * @param decl	Structure field class to register (copied)
 * @returns	0 if registration went okay, negative value otherwise
 */
static
int ctx_decl_scope_register_struct(struct ctx_decl_scope *scope,
		const char *name, struct ctf_field_class_struct *decl)
{
	return ctx_decl_scope_register_prefix_alias(scope, _PREFIX_STRUCT,
		name, (void *) decl);
}

/**
 * Registers a variant declaration within a declaration scope.
 *
 * @param scope	Declaration scope
 * @param name	Variant name (non-NULL)
 * @param decl	Variant field class to register
 * @returns	0 if registration went okay, negative value otherwise
 */
static
int ctx_decl_scope_register_variant(struct ctx_decl_scope *scope,
		const char *name, struct ctf_field_class_variant *decl)
{
	return ctx_decl_scope_register_prefix_alias(scope, _PREFIX_VARIANT,
		name, (void *) decl);
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

	if (!ctx) {
		goto end;
	}

	scope = ctx->current_scope;

	/*
	 * Destroy all scopes, from current one to the root scope.
	 */
	while (scope) {
		struct ctx_decl_scope *parent_scope = scope->parent_scope;

		ctx_decl_scope_destroy(scope);
		scope = parent_scope;
	}

	bt_trace_class_put_ref(ctx->trace_class);

	if (ctx->ctf_tc) {
		ctf_trace_class_destroy(ctx->ctf_tc);
	}

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
struct ctx *ctx_create(bt_self_component_source *self_comp,
		const struct ctf_metadata_decoder_config *decoder_config)
{
	struct ctx *ctx = NULL;

	BT_ASSERT(decoder_config);

	ctx = g_new0(struct ctx, 1);
	if (!ctx) {
		BT_LOGE_STR("Failed to allocate one visitor context.");
		goto error;
	}

	if (self_comp) {
		ctx->trace_class = bt_trace_class_create(
			bt_self_component_source_as_self_component(self_comp));
		if (!ctx->trace_class) {
			BT_LOGE_STR("Cannot create empty trace class.");
			goto error;
		}
	}

	ctx->ctf_tc = ctf_trace_class_create();
	if (!ctx->ctf_tc) {
		BT_LOGE_STR("Cannot create CTF trace class.");
		goto error;
	}

	/* Root declaration scope */
	ctx->current_scope = ctx_decl_scope_create(NULL);
	if (!ctx->current_scope) {
		BT_LOGE_STR("Cannot create declaration scope.");
		goto error;
	}

	ctx->decoder_config = *decoder_config;
	goto end;

error:
	ctx_destroy(ctx);
	ctx = NULL;

end:
	return ctx;
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

	BT_ASSERT(ctx);
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

	BT_ASSERT(ctx);

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
int visit_field_class_specifier_list(struct ctx *ctx, struct ctf_node *ts_list,
		struct ctf_field_class **decl);

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

	BT_ASSERT(field_ref);
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
	BT_ASSERT(str);

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

	*value = 0;

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
enum ctf_byte_order byte_order_from_unary_expr(struct ctf_node *unary_expr)
{
	const char *str;
	enum ctf_byte_order bo = -1;

	if (unary_expr->u.unary_expression.type != UNARY_STRING) {
		_BT_LOGE_NODE(unary_expr,
			"\"byte_order\" attribute: expecting `be`, `le`, `network`, or `native`.");
		goto end;
	}

	str = unary_expr->u.unary_expression.u.string;

	if (!strcmp(str, "be") || !strcmp(str, "network")) {
		bo = CTF_BYTE_ORDER_BIG;
	} else if (!strcmp(str, "le")) {
		bo = CTF_BYTE_ORDER_LITTLE;
	} else if (!strcmp(str, "native")) {
		bo = CTF_BYTE_ORDER_DEFAULT;
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
enum ctf_byte_order get_real_byte_order(struct ctx *ctx,
		struct ctf_node *uexpr)
{
	enum ctf_byte_order bo = byte_order_from_unary_expr(uexpr);

	if (bo == CTF_BYTE_ORDER_DEFAULT) {
		bo = ctx->ctf_tc->default_byte_order;
	}

	return bo;
}

static
int is_align_valid(uint64_t align)
{
	return (align != 0) && !(align & (align - UINT64_C(1)));
}

static
int get_class_specifier_name(struct ctx *ctx, struct ctf_node *cls_specifier,
		GString *str)
{
	int ret = 0;

	if (cls_specifier->type != NODE_TYPE_SPECIFIER) {
		_BT_LOGE_NODE(cls_specifier,
			"Unexpected node type: node-type=%d",
			cls_specifier->type);
		ret = -EINVAL;
		goto end;
	}

	switch (cls_specifier->u.field_class_specifier.type) {
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
		if (cls_specifier->u.field_class_specifier.id_type) {
			g_string_append(str,
				cls_specifier->u.field_class_specifier.id_type);
		}
		break;
	case TYPESPEC_STRUCT:
	{
		struct ctf_node *node = cls_specifier->u.field_class_specifier.node;

		if (!node->u._struct.name) {
			_BT_LOGE_NODE(node, "Unexpected empty structure field class name.");
			ret = -EINVAL;
			goto end;
		}

		g_string_append(str, "struct ");
		g_string_append(str, node->u._struct.name);
		break;
	}
	case TYPESPEC_VARIANT:
	{
		struct ctf_node *node = cls_specifier->u.field_class_specifier.node;

		if (!node->u.variant.name) {
			_BT_LOGE_NODE(node, "Unexpected empty variant field class name.");
			ret = -EINVAL;
			goto end;
		}

		g_string_append(str, "variant ");
		g_string_append(str, node->u.variant.name);
		break;
	}
	case TYPESPEC_ENUM:
	{
		struct ctf_node *node = cls_specifier->u.field_class_specifier.node;

		if (!node->u._enum.enum_id) {
			_BT_LOGE_NODE(node,
				"Unexpected empty enumeration field class (`enum`) name.");
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
		_BT_LOGE_NODE(cls_specifier->u.field_class_specifier.node,
			"Unexpected field class specifier type: %d",
			cls_specifier->u.field_class_specifier.type);
		ret = -EINVAL;
		goto end;
	}

end:
	return ret;
}

static
int get_class_specifier_list_name(struct ctx *ctx,
		struct ctf_node *cls_specifier_list, GString *str)
{
	int ret = 0;
	struct ctf_node *iter;
	int alias_item_nr = 0;
	struct bt_list_head *head =
		&cls_specifier_list->u.field_class_specifier_list.head;

	bt_list_for_each_entry(iter, head, siblings) {
		if (alias_item_nr != 0) {
			g_string_append(str, " ");
		}

		alias_item_nr++;
		ret = get_class_specifier_name(ctx, iter, str);
		if (ret) {
			goto end;
		}
	}

end:
	return ret;
}

static
GQuark create_class_alias_identifier(struct ctx *ctx,
		struct ctf_node *cls_specifier_list,
		struct ctf_node *node_field_class_declarator)
{
	int ret;
	char *str_c;
	GString *str;
	GQuark qalias = 0;
	struct ctf_node *iter;
	struct bt_list_head *pointers =
		&node_field_class_declarator->u.field_class_declarator.pointers;

	str = g_string_new("");
	ret = get_class_specifier_list_name(ctx, cls_specifier_list, str);
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
int visit_field_class_declarator(struct ctx *ctx,
		struct ctf_node *cls_specifier_list,
		GQuark *field_name, struct ctf_node *node_field_class_declarator,
		struct ctf_field_class **field_decl,
		struct ctf_field_class *nested_decl)
{
	/*
	 * During this whole function, nested_decl is always OURS,
	 * whereas field_decl is an output which we create, but
	 * belongs to the caller (it is moved).
	 */
	int ret = 0;
	*field_decl = NULL;

	/* Validate field class declarator node */
	if (node_field_class_declarator) {
		if (node_field_class_declarator->u.field_class_declarator.type ==
				TYPEDEC_UNKNOWN) {
			_BT_LOGE_NODE(node_field_class_declarator,
				"Unexpected field class declarator type: type=%d",
				node_field_class_declarator->u.field_class_declarator.type);
			ret = -EINVAL;
			goto error;
		}

		/* TODO: GCC bitfields not supported yet */
		if (node_field_class_declarator->u.field_class_declarator.bitfield_len !=
				NULL) {
			_BT_LOGE_NODE(node_field_class_declarator,
				"GCC bitfields are not supported as of this version.");
			ret = -EPERM;
			goto error;
		}
	}

	/* Find the right nested declaration if not provided */
	if (!nested_decl) {
		struct bt_list_head *pointers =
			&node_field_class_declarator->u.field_class_declarator.pointers;

		if (node_field_class_declarator && !bt_list_empty(pointers)) {
			GQuark qalias;

			/*
			 * If we have a pointer declarator, it HAS to
		 	 * be present in the field class aliases (else
		 	 * fail).
			 */
			qalias = create_class_alias_identifier(ctx,
				cls_specifier_list, node_field_class_declarator);
			nested_decl =
				ctx_decl_scope_lookup_alias(ctx->current_scope,
					g_quark_to_string(qalias), -1, true);
			if (!nested_decl) {
				_BT_LOGE_NODE(node_field_class_declarator,
					"Cannot find class alias: name=\"%s\"",
					g_quark_to_string(qalias));
				ret = -EINVAL;
				goto error;
			}

			if (nested_decl->type == CTF_FIELD_CLASS_TYPE_INT) {
				/* Pointer: force integer's base to 16 */
				struct ctf_field_class_int *int_fc =
					(void *) nested_decl;

				int_fc->disp_base =
					BT_FIELD_CLASS_INTEGER_PREFERRED_DISPLAY_BASE_HEXADECIMAL;
			}
		} else {
			ret = visit_field_class_specifier_list(ctx,
				cls_specifier_list, &nested_decl);
			if (ret) {
				BT_ASSERT(!nested_decl);
				goto error;
			}
		}
	}

	BT_ASSERT(nested_decl);

	if (!node_field_class_declarator) {
		*field_decl = nested_decl;
		nested_decl = NULL;
		goto end;
	}

	if (node_field_class_declarator->u.field_class_declarator.type == TYPEDEC_ID) {
		if (node_field_class_declarator->u.field_class_declarator.u.id) {
			const char *id =
				node_field_class_declarator->u.field_class_declarator.u.id;

			if (id[0] == '_') {
				id++;
			}

			*field_name = g_quark_from_string(id);
		} else {
			*field_name = 0;
		}

		*field_decl = nested_decl;
		nested_decl = NULL;
		goto end;
	} else {
		struct ctf_node *first;
		struct ctf_field_class *decl = NULL;
		struct ctf_field_class *outer_field_decl = NULL;
		struct bt_list_head *length =
			&node_field_class_declarator->
				u.field_class_declarator.u.nested.length;

		/* Create array/sequence, pass nested_decl as child */
		if (bt_list_empty(length)) {
			_BT_LOGE_NODE(node_field_class_declarator,
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
			struct ctf_field_class_array *array_decl = NULL;

			array_decl = ctf_field_class_array_create();
			BT_ASSERT(array_decl);
			array_decl->length =
				first->u.unary_expression.u.unsigned_constant;
			array_decl->base.elem_fc = nested_decl;
			nested_decl = NULL;
			decl = (void *) array_decl;
			break;
		}
		case UNARY_STRING:
		{
			/* Lookup unsigned integer definition, create seq. */
			struct ctf_field_class_sequence *seq_decl = NULL;
			char *length_name = concatenate_unary_strings(length);

			if (!length_name) {
				_BT_LOGE_NODE(node_field_class_declarator,
					"Cannot concatenate unary strings.");
				ret = -EINVAL;
				goto error;
			}

			if (strncmp(length_name, "env.", 4) == 0) {
				/* This is, in fact, an array */
				const char *env_entry_name = &length_name[4];
				struct ctf_trace_class_env_entry *env_entry =
					ctf_trace_class_borrow_env_entry_by_name(
						ctx->ctf_tc, env_entry_name);
				struct ctf_field_class_array *array_decl;

				if (!env_entry) {
					_BT_LOGE_NODE(node_field_class_declarator,
						"Cannot find environment entry: "
						"name=\"%s\"", env_entry_name);
					ret = -EINVAL;
					goto error;
				}

				if (env_entry->type != CTF_TRACE_CLASS_ENV_ENTRY_TYPE_INT) {
					_BT_LOGE_NODE(node_field_class_declarator,
						"Wrong environment entry type "
						"(expecting integer): "
						"name=\"%s\"", env_entry_name);
					ret = -EINVAL;
					goto error;
				}

				if (env_entry->value.i < 0) {
					_BT_LOGE_NODE(node_field_class_declarator,
						"Invalid, negative array length: "
						"env-entry-name=\"%s\", "
						"value=%" PRId64,
						env_entry_name,
						env_entry->value.i);
					ret = -EINVAL;
					goto error;
				}

				array_decl = ctf_field_class_array_create();
				BT_ASSERT(array_decl);
				array_decl->length =
					(uint64_t) env_entry->value.i;
				array_decl->base.elem_fc = nested_decl;
				nested_decl = NULL;
				decl = (void *) array_decl;
			} else {
				char *length_name_no_underscore =
					remove_underscores_from_field_ref(
						length_name);
				if (!length_name_no_underscore) {
					/*
					 * remove_underscores_from_field_ref()
					 * logs errors
					 */
					ret = -EINVAL;
					goto error;
				}
				seq_decl = ctf_field_class_sequence_create();
				BT_ASSERT(seq_decl);
				seq_decl->base.elem_fc = nested_decl;
				nested_decl = NULL;
				g_string_assign(seq_decl->length_ref,
					length_name_no_underscore);
				free(length_name_no_underscore);
				decl = (void *) seq_decl;
			}

			g_free(length_name);
			break;
		}
		default:
			ret = -EINVAL;
			goto error;
		}

		BT_ASSERT(!nested_decl);
		BT_ASSERT(decl);
		BT_ASSERT(!*field_decl);

		/*
		 * At this point, we found the next nested declaration.
		 * We currently own this (and lost the ownership of
		 * nested_decl in the meantime). Pass this next
		 * nested declaration as the content of the outer
		 * container, MOVING its ownership.
		 */
		ret = visit_field_class_declarator(ctx, cls_specifier_list,
			field_name,
			node_field_class_declarator->
				u.field_class_declarator.u.nested.field_class_declarator,
			&outer_field_decl, decl);
		decl = NULL;
		if (ret) {
			BT_ASSERT(!outer_field_decl);
			ret = -EINVAL;
			goto error;
		}

		BT_ASSERT(outer_field_decl);
		*field_decl = outer_field_decl;
		outer_field_decl = NULL;
	}

	BT_ASSERT(*field_decl);
	goto end;

error:
	ctf_field_class_destroy(*field_decl);
	*field_decl = NULL;

	if (ret >= 0) {
		ret = -1;
	}

end:
	ctf_field_class_destroy(nested_decl);
	nested_decl = NULL;
	return ret;
}

static
int visit_struct_decl_field(struct ctx *ctx,
		struct ctf_field_class_struct *struct_decl,
		struct ctf_node *cls_specifier_list,
		struct bt_list_head *field_class_declarators)
{
	int ret = 0;
	struct ctf_node *iter;
	struct ctf_field_class *field_decl = NULL;

	bt_list_for_each_entry(iter, field_class_declarators, siblings) {
		field_decl = NULL;
		GQuark qfield_name;
		const char *field_name;

		ret = visit_field_class_declarator(ctx, cls_specifier_list,
			&qfield_name, iter, &field_decl, NULL);
		if (ret) {
			BT_ASSERT(!field_decl);
			_BT_LOGE_NODE(cls_specifier_list,
				"Cannot visit field class declarator: ret=%d", ret);
			goto error;
		}

		BT_ASSERT(field_decl);
		field_name = g_quark_to_string(qfield_name);

		/* Check if field with same name already exists */
		if (ctf_field_class_struct_borrow_member_by_name(
				struct_decl, field_name)) {
			_BT_LOGE_NODE(cls_specifier_list,
				"Duplicate field in structure field class: "
				"field-name=\"%s\"", field_name);
			ret = -EINVAL;
			goto error;
		}

		/* Add field to structure */
		ctf_field_class_struct_append_member(struct_decl,
			field_name, field_decl);
		field_decl = NULL;
	}

	return 0;

error:
	ctf_field_class_destroy(field_decl);
	field_decl = NULL;
	return ret;
}

static
int visit_variant_decl_field(struct ctx *ctx,
		struct ctf_field_class_variant *variant_decl,
		struct ctf_node *cls_specifier_list,
		struct bt_list_head *field_class_declarators)
{
	int ret = 0;
	struct ctf_node *iter;
	struct ctf_field_class *field_decl = NULL;

	bt_list_for_each_entry(iter, field_class_declarators, siblings) {
		field_decl = NULL;
		GQuark qfield_name;
		const char *field_name;

		ret = visit_field_class_declarator(ctx, cls_specifier_list,
			&qfield_name, iter, &field_decl, NULL);
		if (ret) {
			BT_ASSERT(!field_decl);
			_BT_LOGE_NODE(cls_specifier_list,
				"Cannot visit field class declarator: ret=%d", ret);
			goto error;
		}

		BT_ASSERT(field_decl);
		field_name = g_quark_to_string(qfield_name);

		/* Check if field with same name already exists */
		if (ctf_field_class_variant_borrow_option_by_name(
				variant_decl, field_name)) {
			_BT_LOGE_NODE(cls_specifier_list,
				"Duplicate field in variant field class: "
				"field-name=\"%s\"", field_name);
			ret = -EINVAL;
			goto error;
		}

		/* Add field to structure */
		ctf_field_class_variant_append_option(variant_decl,
			field_name, field_decl);
		field_decl = NULL;
	}

	return 0;

error:
	ctf_field_class_destroy(field_decl);
	field_decl = NULL;
	return ret;
}

static
int visit_field_class_def(struct ctx *ctx, struct ctf_node *cls_specifier_list,
		struct bt_list_head *field_class_declarators)
{
	int ret = 0;
	GQuark qidentifier;
	struct ctf_node *iter;
	struct ctf_field_class *class_decl = NULL;

	bt_list_for_each_entry(iter, field_class_declarators, siblings) {
		ret = visit_field_class_declarator(ctx, cls_specifier_list,
			&qidentifier, iter, &class_decl, NULL);
		if (ret) {
			_BT_LOGE_NODE(iter,
				"Cannot visit field class declarator: ret=%d", ret);
			ret = -EINVAL;
			goto end;
		}

		/* Do not allow field class def and alias of untagged variants */
		if (class_decl->type == CTF_FIELD_CLASS_TYPE_VARIANT) {
			struct ctf_field_class_variant *var_fc =
				(void *) class_decl;

			if (var_fc->tag_path.path->len == 0) {
				_BT_LOGE_NODE(iter,
					"Type definition of untagged variant field class is not allowed.");
				ret = -EPERM;
				goto end;
			}
		}

		ret = ctx_decl_scope_register_alias(ctx->current_scope,
			g_quark_to_string(qidentifier), class_decl);
		if (ret) {
			_BT_LOGE_NODE(iter,
				"Cannot register field class alias: name=\"%s\"",
				g_quark_to_string(qidentifier));
			goto end;
		}
	}

end:
	ctf_field_class_destroy(class_decl);
	class_decl = NULL;
	return ret;
}

static
int visit_field_class_alias(struct ctx *ctx, struct ctf_node *target,
		struct ctf_node *alias)
{
	int ret = 0;
	GQuark qalias;
	struct ctf_node *node;
	GQuark qdummy_field_name;
	struct ctf_field_class *class_decl = NULL;

	/* Create target field class */
	if (bt_list_empty(&target->u.field_class_alias_target.field_class_declarators)) {
		node = NULL;
	} else {
		node = _BT_LIST_FIRST_ENTRY(
			&target->u.field_class_alias_target.field_class_declarators,
			struct ctf_node, siblings);
	}

	ret = visit_field_class_declarator(ctx,
		target->u.field_class_alias_target.field_class_specifier_list,
		&qdummy_field_name, node, &class_decl, NULL);
	if (ret) {
		BT_ASSERT(!class_decl);
		_BT_LOGE_NODE(node,
			"Cannot visit field class declarator: ret=%d", ret);
		goto end;
	}

	/* Do not allow field class def and alias of untagged variants */
	if (class_decl->type == CTF_FIELD_CLASS_TYPE_VARIANT) {
		struct ctf_field_class_variant *var_fc = (void *) class_decl;

		if (var_fc->tag_path.path->len == 0) {
			_BT_LOGE_NODE(target,
				"Type definition of untagged variant field class is not allowed.");
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
	node = _BT_LIST_FIRST_ENTRY(&alias->u.field_class_alias_name.field_class_declarators,
		struct ctf_node, siblings);
	qalias = create_class_alias_identifier(ctx,
		alias->u.field_class_alias_name.field_class_specifier_list, node);
	ret = ctx_decl_scope_register_alias(ctx->current_scope,
		g_quark_to_string(qalias), class_decl);
	if (ret) {
		_BT_LOGE_NODE(node,
			"Cannot register class alias: name=\"%s\"",
			g_quark_to_string(qalias));
		goto end;
	}

end:
	ctf_field_class_destroy(class_decl);
	class_decl = NULL;
	return ret;
}

static
int visit_struct_decl_entry(struct ctx *ctx, struct ctf_node *entry_node,
		struct ctf_field_class_struct *struct_decl)
{
	int ret = 0;

	switch (entry_node->type) {
	case NODE_TYPEDEF:
		ret = visit_field_class_def(ctx,
			entry_node->u.field_class_def.field_class_specifier_list,
			&entry_node->u.field_class_def.field_class_declarators);
		if (ret) {
			_BT_LOGE_NODE(entry_node,
				"Cannot add field class found in structure field class: ret=%d",
				ret);
			goto end;
		}
		break;
	case NODE_TYPEALIAS:
		ret = visit_field_class_alias(ctx, entry_node->u.field_class_alias.target,
			entry_node->u.field_class_alias.alias);
		if (ret) {
			_BT_LOGE_NODE(entry_node,
				"Cannot add field class alias found in structure field class: ret=%d",
				ret);
			goto end;
		}
		break;
	case NODE_STRUCT_OR_VARIANT_DECLARATION:
		/* Field */
		ret = visit_struct_decl_field(ctx, struct_decl,
			entry_node->u.struct_or_variant_declaration.
				field_class_specifier_list,
			&entry_node->u.struct_or_variant_declaration.
				field_class_declarators);
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
		struct ctf_field_class_variant *variant_decl)
{
	int ret = 0;

	switch (entry_node->type) {
	case NODE_TYPEDEF:
		ret = visit_field_class_def(ctx,
			entry_node->u.field_class_def.field_class_specifier_list,
			&entry_node->u.field_class_def.field_class_declarators);
		if (ret) {
			_BT_LOGE_NODE(entry_node,
				"Cannot add field class found in variant field class: ret=%d",
				ret);
			goto end;
		}
		break;
	case NODE_TYPEALIAS:
		ret = visit_field_class_alias(ctx, entry_node->u.field_class_alias.target,
			entry_node->u.field_class_alias.alias);
		if (ret) {
			_BT_LOGE_NODE(entry_node,
				"Cannot add field class alias found in variant field class: ret=%d",
				ret);
			goto end;
		}
		break;
	case NODE_STRUCT_OR_VARIANT_DECLARATION:
		/* Field */
		ret = visit_variant_decl_field(ctx, variant_decl,
			entry_node->u.struct_or_variant_declaration.
				field_class_specifier_list,
			&entry_node->u.struct_or_variant_declaration.
				field_class_declarators);
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
		struct ctf_field_class_struct **struct_decl)
{
	int ret = 0;

	BT_ASSERT(struct_decl);
	*struct_decl = NULL;

	/* For named struct (without body), lookup in declaration scope */
	if (!has_body) {
		if (!name) {
			BT_LOGE_STR("Bodyless structure field class: missing name.");
			ret = -EPERM;
			goto error;
		}

		*struct_decl = ctx_decl_scope_lookup_struct(ctx->current_scope,
			name, -1, true);
		if (!*struct_decl) {
			BT_LOGE("Cannot find structure field class: name=\"struct %s\"",
				name);
			ret = -EINVAL;
			goto error;
		}
	} else {
		struct ctf_node *entry_node;
		uint64_t min_align_value = 0;

		if (name) {
			if (ctx_decl_scope_lookup_struct(
					ctx->current_scope, name, 1, false)) {
				BT_LOGE("Structure field class already declared in local scope: "
					"name=\"struct %s\"", name);
				ret = -EINVAL;
				goto error;
			}
		}

		if (!bt_list_empty(min_align)) {
			ret = get_unary_unsigned(min_align, &min_align_value);
			if (ret) {
				BT_LOGE("Unexpected unary expression for structure field class's `align` attribute: "
					"ret=%d", ret);
				goto error;
			}
		}

		*struct_decl = ctf_field_class_struct_create();
		BT_ASSERT(*struct_decl);

		if (min_align_value != 0) {
			(*struct_decl)->base.alignment = min_align_value;
		}

		_TRY_PUSH_SCOPE_OR_GOTO_ERROR();

		bt_list_for_each_entry(entry_node, decl_list, siblings) {
			ret = visit_struct_decl_entry(ctx, entry_node,
				*struct_decl);
			if (ret) {
				_BT_LOGE_NODE(entry_node,
					"Cannot visit structure field class entry: "
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
				BT_LOGE("Cannot register structure field class in declaration scope: "
					"name=\"struct %s\", ret=%d", name, ret);
				goto error;
			}
		}
	}

	return 0;

error:
	ctf_field_class_destroy((void *) *struct_decl);
	*struct_decl = NULL;
	return ret;
}

static
int visit_variant_decl(struct ctx *ctx, const char *name,
	const char *tag, struct bt_list_head *decl_list,
	int has_body, struct ctf_field_class_variant **variant_decl)
{
	int ret = 0;
	struct ctf_field_class_variant *untagged_variant_decl = NULL;

	BT_ASSERT(variant_decl);
	*variant_decl = NULL;

	/* For named variant (without body), lookup in declaration scope */
	if (!has_body) {
		if (!name) {
			BT_LOGE_STR("Bodyless variant field class: missing name.");
			ret = -EPERM;
			goto error;
		}

		untagged_variant_decl =
			ctx_decl_scope_lookup_variant(ctx->current_scope,
				name, -1, true);
		if (!untagged_variant_decl) {
			BT_LOGE("Cannot find variant field class: name=\"variant %s\"",
				name);
			ret = -EINVAL;
			goto error;
		}
	} else {
		struct ctf_node *entry_node;

		if (name) {
			if (ctx_decl_scope_lookup_variant(ctx->current_scope,
					name, 1, false)) {
				BT_LOGE("Variant field class already declared in local scope: "
					"name=\"variant %s\"", name);
				ret = -EINVAL;
				goto error;
			}
		}

		untagged_variant_decl = ctf_field_class_variant_create();
		BT_ASSERT(untagged_variant_decl);
		_TRY_PUSH_SCOPE_OR_GOTO_ERROR();

		bt_list_for_each_entry(entry_node, decl_list, siblings) {
			ret = visit_variant_decl_entry(ctx, entry_node,
				untagged_variant_decl);
			if (ret) {
				_BT_LOGE_NODE(entry_node,
					"Cannot visit variant field class entry: "
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
				BT_LOGE("Cannot register variant field class in declaration scope: "
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
		*variant_decl = untagged_variant_decl;
		untagged_variant_decl = NULL;
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

		g_string_assign(untagged_variant_decl->tag_ref,
			tag_no_underscore);
		free(tag_no_underscore);
		*variant_decl = untagged_variant_decl;
		untagged_variant_decl = NULL;
	}

	BT_ASSERT(!untagged_variant_decl);
	BT_ASSERT(*variant_decl);
	return 0;

error:
	ctf_field_class_destroy((void *) untagged_variant_decl);
	untagged_variant_decl = NULL;
	ctf_field_class_destroy((void *) *variant_decl);
	*variant_decl = NULL;
	return ret;
}

struct uori {
	bool is_signed;
	union {
		uint64_t u;
		uint64_t i;
	} value;
};

static
int visit_enum_decl_entry(struct ctx *ctx, struct ctf_node *enumerator,
		struct ctf_field_class_enum *enum_decl, struct uori *last)
{
	int ret = 0;
	int nr_vals = 0;
	struct ctf_node *iter;
	struct uori start = {
		.is_signed = false,
		.value.u = 0,
	};
	struct uori end = {
		.is_signed = false,
		.value.u = 0,
	};
	const char *label = enumerator->u.enumerator.id;
	const char *effective_label = label;
	struct bt_list_head *values = &enumerator->u.enumerator.values;

	bt_list_for_each_entry(iter, values, siblings) {
		struct uori *target;

		if (iter->type != NODE_UNARY_EXPRESSION) {
			_BT_LOGE_NODE(iter,
				"Wrong expression for enumeration field class label: "
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
			target->is_signed = true;
			target->value.i =
				iter->u.unary_expression.u.signed_constant;
			break;
		case UNARY_UNSIGNED_CONSTANT:
			target->is_signed = false;
			target->value.u =
				iter->u.unary_expression.u.unsigned_constant;
			break;
		default:
			_BT_LOGE_NODE(iter,
				"Invalid enumeration field class entry: "
				"expecting constant signed or unsigned integer: "
				"node-type=%d, label=\"%s\"",
				iter->u.unary_expression.type, label);
			ret = -EINVAL;
			goto error;
		}

		if (nr_vals > 1) {
			_BT_LOGE_NODE(iter,
				"Invalid enumeration field class entry: label=\"%s\"",
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

	if (end.is_signed) {
		last->value.i = end.value.i + 1;
	} else {
		last->value.u = end.value.u + 1;
	}

	if (label[0] == '_') {
		/*
		 * Strip the first underscore of any enumeration field
		 * class's label in case this enumeration FC is used as
		 * a variant FC tag later. The variant FC choice names
		 * could also start with `_`, in which case the prefix
		 * is removed, and it the resulting choice name needs to
		 * match tag labels.
		 */
		effective_label = &label[1];
	}

	ctf_field_class_enum_append_mapping(enum_decl, effective_label,
		start.value.u, end.value.u);
	return 0;

error:
	return ret;
}

static
int visit_enum_decl(struct ctx *ctx, const char *name,
		struct ctf_node *container_cls,
		struct bt_list_head *enumerator_list,
		int has_body, struct ctf_field_class_enum **enum_decl)
{
	int ret = 0;
	GQuark qdummy_id;
	struct ctf_field_class_int *integer_decl = NULL;

	BT_ASSERT(enum_decl);
	*enum_decl = NULL;

	/* For named enum (without body), lookup in declaration scope */
	if (!has_body) {
		if (!name) {
			BT_LOGE_STR("Bodyless enumeration field class: missing name.");
			ret = -EPERM;
			goto error;
		}

		*enum_decl = ctx_decl_scope_lookup_enum(ctx->current_scope,
			name, -1, true);
		if (!*enum_decl) {
			BT_LOGE("Cannot find enumeration field class: "
				"name=\"enum %s\"", name);
			ret = -EINVAL;
			goto error;
		}
	} else {
		struct ctf_node *iter;
		struct uori last_value = {
			.is_signed = false,
			.value.u = 0,
		};

		if (name) {
			if (ctx_decl_scope_lookup_enum(ctx->current_scope,
					name, 1, false)) {
				BT_LOGE("Enumeration field class already declared in local scope: "
					"name=\"enum %s\"", name);
				ret = -EINVAL;
				goto error;
			}
		}

		if (!container_cls) {
			integer_decl = (void *) ctx_decl_scope_lookup_alias(
				ctx->current_scope, "int", -1, true);
			if (!integer_decl) {
				BT_LOGE_STR("Cannot find implicit `int` field class alias for enumeration field class.");
				ret = -EINVAL;
				goto error;
			}
		} else {
			ret = visit_field_class_declarator(ctx, container_cls,
				&qdummy_id, NULL, (void *) &integer_decl,
				NULL);
			if (ret) {
				BT_ASSERT(!integer_decl);
				ret = -EINVAL;
				goto error;
			}
		}

		BT_ASSERT(integer_decl);

		if (integer_decl->base.base.type != CTF_FIELD_CLASS_TYPE_INT) {
			BT_LOGE("Container field class for enumeration field class is not an integer field class: "
				"fc-type=%d", integer_decl->base.base.type);
			ret = -EINVAL;
			goto error;
		}

		*enum_decl = ctf_field_class_enum_create();
		BT_ASSERT(*enum_decl);
		(*enum_decl)->base.base.base.alignment =
			integer_decl->base.base.alignment;
		ctf_field_class_int_copy_content((void *) *enum_decl,
				(void *) integer_decl);
		last_value.is_signed = (*enum_decl)->base.is_signed;

		bt_list_for_each_entry(iter, enumerator_list, siblings) {
			ret = visit_enum_decl_entry(ctx, iter, *enum_decl,
				&last_value);
			if (ret) {
				_BT_LOGE_NODE(iter,
					"Cannot visit enumeration field class entry: "
					"ret=%d", ret);
				goto error;
			}
		}

		if (name) {
			ret = ctx_decl_scope_register_enum(ctx->current_scope,
				name, *enum_decl);
			if (ret) {
				BT_LOGE("Cannot register enumeration field class in declaration scope: "
					"ret=%d", ret);
				goto error;
			}
		}
	}

	goto end;

error:
	ctf_field_class_destroy((void *) *enum_decl);
	*enum_decl = NULL;

end:
	ctf_field_class_destroy((void *) integer_decl);
	integer_decl = NULL;
	return ret;
}

static
int visit_field_class_specifier(struct ctx *ctx,
		struct ctf_node *cls_specifier_list,
		struct ctf_field_class **decl)
{
	int ret = 0;
	GString *str = NULL;

	*decl = NULL;
	str = g_string_new("");
	ret = get_class_specifier_list_name(ctx, cls_specifier_list, str);
	if (ret) {
		_BT_LOGE_NODE(cls_specifier_list,
			"Cannot get field class specifier list's name: ret=%d", ret);
		goto error;
	}

	*decl = ctx_decl_scope_lookup_alias(ctx->current_scope, str->str, -1,
		true);
	if (!*decl) {
		_BT_LOGE_NODE(cls_specifier_list,
			"Cannot find field class alias: name=\"%s\"", str->str);
		ret = -EINVAL;
		goto error;
	}

	goto end;

error:
	ctf_field_class_destroy(*decl);
	*decl = NULL;

end:
	if (str) {
		g_string_free(str, TRUE);
	}

	return ret;
}

static
int visit_integer_decl(struct ctx *ctx,
		struct bt_list_head *expressions,
		struct ctf_field_class_int **integer_decl)
{
	int set = 0;
	int ret = 0;
	int signedness = 0;
	struct ctf_node *expression;
	uint64_t alignment = 0, size = 0;
	struct ctf_clock_class *mapped_clock_class = NULL;
	enum ctf_encoding encoding = CTF_ENCODING_NONE;
	bt_field_class_integer_preferred_display_base base =
		BT_FIELD_CLASS_INTEGER_PREFERRED_DISPLAY_BASE_DECIMAL;
	enum ctf_byte_order byte_order = ctx->ctf_tc->default_byte_order;

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
					"integer field class");
				ret = -EPERM;
				goto error;
			}

			signedness = get_boolean(right);
			if (signedness < 0) {
				_BT_LOGE_NODE(right,
					"Invalid boolean value for integer field class's `signed` attribute: "
					"ret=%d", ret);
				ret = -EINVAL;
				goto error;
			}

			_SET(&set, _INTEGER_SIGNED_SET);
		} else if (!strcmp(left->u.unary_expression.u.string,
				"byte_order")) {
			if (_IS_SET(&set, _INTEGER_BYTE_ORDER_SET)) {
				_BT_LOGE_DUP_ATTR(left, "byte_order",
					"integer field class");
				ret = -EPERM;
				goto error;
			}

			byte_order = get_real_byte_order(ctx, right);
			if (byte_order == -1) {
				_BT_LOGE_NODE(right,
					"Invalid `byte_order` attribute in integer field class: "
					"ret=%d", ret);
				ret = -EINVAL;
				goto error;
			}

			_SET(&set, _INTEGER_BYTE_ORDER_SET);
		} else if (!strcmp(left->u.unary_expression.u.string, "size")) {
			if (_IS_SET(&set, _INTEGER_SIZE_SET)) {
				_BT_LOGE_DUP_ATTR(left, "size",
					"integer field class");
				ret = -EPERM;
				goto error;
			}

			if (right->u.unary_expression.type !=
					UNARY_UNSIGNED_CONSTANT) {
				_BT_LOGE_NODE(right,
					"Invalid `size` attribute in integer field class: "
					"expecting unsigned constant integer: "
					"node-type=%d",
					right->u.unary_expression.type);
				ret = -EINVAL;
				goto error;
			}

			size = right->u.unary_expression.u.unsigned_constant;
			if (size == 0) {
				_BT_LOGE_NODE(right,
					"Invalid `size` attribute in integer field class: "
					"expecting positive constant integer: "
					"size=%" PRIu64, size);
				ret = -EINVAL;
				goto error;
			} else if (size > 64) {
				_BT_LOGE_NODE(right,
					"Invalid `size` attribute in integer field class: "
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
					"integer field class");
				ret = -EPERM;
				goto error;
			}

			if (right->u.unary_expression.type !=
					UNARY_UNSIGNED_CONSTANT) {
				_BT_LOGE_NODE(right,
					"Invalid `align` attribute in integer field class: "
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
					"Invalid `align` attribute in integer field class: "
					"expecting power of two: "
					"align=%" PRIu64, alignment);
				ret = -EINVAL;
				goto error;
			}

			_SET(&set, _INTEGER_ALIGN_SET);
		} else if (!strcmp(left->u.unary_expression.u.string, "base")) {
			if (_IS_SET(&set, _INTEGER_BASE_SET)) {
				_BT_LOGE_DUP_ATTR(left, "base",
					"integer field class");
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
					base = BT_FIELD_CLASS_INTEGER_PREFERRED_DISPLAY_BASE_BINARY;
					break;
				case 8:
					base = BT_FIELD_CLASS_INTEGER_PREFERRED_DISPLAY_BASE_OCTAL;
					break;
				case 10:
					base = BT_FIELD_CLASS_INTEGER_PREFERRED_DISPLAY_BASE_DECIMAL;
					break;
				case 16:
					base = BT_FIELD_CLASS_INTEGER_PREFERRED_DISPLAY_BASE_HEXADECIMAL;
					break;
				default:
					_BT_LOGE_NODE(right,
						"Invalid `base` attribute in integer field class: "
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
						"Unexpected unary expression for integer field class's `base` attribute.");
					ret = -EINVAL;
					goto error;
				}

				if (!strcmp(s_right, "decimal") ||
						!strcmp(s_right, "dec") ||
						!strcmp(s_right, "d") ||
						!strcmp(s_right, "i") ||
						!strcmp(s_right, "u")) {
					base = BT_FIELD_CLASS_INTEGER_PREFERRED_DISPLAY_BASE_DECIMAL;
				} else if (!strcmp(s_right, "hexadecimal") ||
						!strcmp(s_right, "hex") ||
						!strcmp(s_right, "x") ||
						!strcmp(s_right, "X") ||
						!strcmp(s_right, "p")) {
					base = BT_FIELD_CLASS_INTEGER_PREFERRED_DISPLAY_BASE_HEXADECIMAL;
				} else if (!strcmp(s_right, "octal") ||
						!strcmp(s_right, "oct") ||
						!strcmp(s_right, "o")) {
					base = BT_FIELD_CLASS_INTEGER_PREFERRED_DISPLAY_BASE_OCTAL;
				} else if (!strcmp(s_right, "binary") ||
						!strcmp(s_right, "b")) {
					base = BT_FIELD_CLASS_INTEGER_PREFERRED_DISPLAY_BASE_BINARY;
				} else {
					_BT_LOGE_NODE(right,
						"Unexpected unary expression for integer field class's `base` attribute: "
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
					"Invalid `base` attribute in integer field class: "
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
					"integer field class");
				ret = -EPERM;
				goto error;
			}

			if (right->u.unary_expression.type != UNARY_STRING) {
				_BT_LOGE_NODE(right,
					"Invalid `encoding` attribute in integer field class: "
					"expecting unary string.");
				ret = -EINVAL;
				goto error;
			}

			s_right = concatenate_unary_strings(
				&expression->u.ctf_expression.right);
			if (!s_right) {
				_BT_LOGE_NODE(right,
					"Unexpected unary expression for integer field class's `encoding` attribute.");
				ret = -EINVAL;
				goto error;
			}

			if (!strcmp(s_right, "UTF8") ||
					!strcmp(s_right, "utf8") ||
					!strcmp(s_right, "utf-8") ||
					!strcmp(s_right, "UTF-8") ||
					!strcmp(s_right, "ASCII") ||
					!strcmp(s_right, "ascii")) {
				encoding = CTF_ENCODING_UTF8;
			} else if (!strcmp(s_right, "none")) {
				encoding = CTF_ENCODING_NONE;
			} else {
				_BT_LOGE_NODE(right,
					"Invalid `encoding` attribute in integer field class: "
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
					"integer field class");
				ret = -EPERM;
				goto error;
			}

			if (right->u.unary_expression.type != UNARY_STRING) {
				_BT_LOGE_NODE(right,
					"Invalid `map` attribute in integer field class: "
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
						"Unexpected unary expression for integer field class's `map` attribute.");
					ret = -EINVAL;
					goto error;
				}

				_BT_LOGE_NODE(right,
					"Invalid `map` attribute in integer field class: "
					"cannot find clock class at this point: name=\"%s\"",
					s_right);
				_SET(&set, _INTEGER_MAP_SET);
				g_free(s_right);
				continue;
			}

			mapped_clock_class =
				ctf_trace_class_borrow_clock_class_by_name(
					ctx->ctf_tc, clock_name);
			if (!mapped_clock_class) {
				_BT_LOGE_NODE(right,
					"Invalid `map` attribute in integer field class: "
					"cannot find clock class at this point: name=\"%s\"",
					clock_name);
				ret = -EINVAL;
				goto error;
			}

			_SET(&set, _INTEGER_MAP_SET);
		} else {
			_BT_LOGW_NODE(left,
				"Unknown attribute in integer field class: "
				"attr-name=\"%s\"",
				left->u.unary_expression.u.string);
		}
	}

	if (!_IS_SET(&set, _INTEGER_SIZE_SET)) {
		BT_LOGE_STR("Missing `size` attribute in integer field class.");
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

	*integer_decl = ctf_field_class_int_create();
	BT_ASSERT(*integer_decl);
	(*integer_decl)->base.base.alignment = alignment;
	(*integer_decl)->base.byte_order = byte_order;
	(*integer_decl)->base.size = size;
	(*integer_decl)->is_signed = (signedness > 0);
	(*integer_decl)->disp_base = base;
	(*integer_decl)->encoding = encoding;
	(*integer_decl)->mapped_clock_class = mapped_clock_class;
	return 0;

error:
	ctf_field_class_destroy((void *) *integer_decl);
	*integer_decl = NULL;
	return ret;
}

static
int visit_floating_point_number_decl(struct ctx *ctx,
		struct bt_list_head *expressions,
		struct ctf_field_class_float **float_decl)
{
	int set = 0;
	int ret = 0;
	struct ctf_node *expression;
	uint64_t alignment = 1, exp_dig = 0, mant_dig = 0;
	enum ctf_byte_order byte_order = ctx->ctf_tc->default_byte_order;

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
					"floating point number field class");
				ret = -EPERM;
				goto error;
			}

			byte_order = get_real_byte_order(ctx, right);
			if (byte_order == -1) {
				_BT_LOGE_NODE(right,
					"Invalid `byte_order` attribute in floating point number field class: "
					"ret=%d", ret);
				ret = -EINVAL;
				goto error;
			}

			_SET(&set, _FLOAT_BYTE_ORDER_SET);
		} else if (!strcmp(left->u.unary_expression.u.string,
				"exp_dig")) {
			if (_IS_SET(&set, _FLOAT_EXP_DIG_SET)) {
				_BT_LOGE_DUP_ATTR(left, "exp_dig",
					"floating point number field class");
				ret = -EPERM;
				goto error;
			}

			if (right->u.unary_expression.type !=
					UNARY_UNSIGNED_CONSTANT) {
				_BT_LOGE_NODE(right,
					"Invalid `exp_dig` attribute in floating point number field class: "
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
					"floating point number field class");
				ret = -EPERM;
				goto error;
			}

			if (right->u.unary_expression.type !=
					UNARY_UNSIGNED_CONSTANT) {
				_BT_LOGE_NODE(right,
					"Invalid `mant_dig` attribute in floating point number field class: "
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
					"floating point number field class");
				ret = -EPERM;
				goto error;
			}

			if (right->u.unary_expression.type !=
					UNARY_UNSIGNED_CONSTANT) {
				_BT_LOGE_NODE(right,
					"Invalid `align` attribute in floating point number field class: "
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
					"Invalid `align` attribute in floating point number field class: "
					"expecting power of two: "
					"align=%" PRIu64, alignment);
				ret = -EINVAL;
				goto error;
			}

			_SET(&set, _FLOAT_ALIGN_SET);
		} else {
			_BT_LOGW_NODE(left,
				"Unknown attribute in floating point number field class: "
				"attr-name=\"%s\"",
				left->u.unary_expression.u.string);
		}
	}

	if (!_IS_SET(&set, _FLOAT_MANT_DIG_SET)) {
		BT_LOGE_STR("Missing `mant_dig` attribute in floating point number field class.");
		ret = -EPERM;
		goto error;
	}

	if (!_IS_SET(&set, _FLOAT_EXP_DIG_SET)) {
		BT_LOGE_STR("Missing `exp_dig` attribute in floating point number field class.");
		ret = -EPERM;
		goto error;
	}

	if (mant_dig != 24 && mant_dig != 53) {
		BT_LOGE_STR("`mant_dig` attribute: expecting 24 or 53.");
		ret = -EPERM;
		goto error;
	}

	if (mant_dig == 24 && exp_dig != 8) {
		BT_LOGE_STR("`exp_dig` attribute: expecting 8 because `mant_dig` is 24.");
		ret = -EPERM;
		goto error;
	}

	if (mant_dig == 53 && exp_dig != 11) {
		BT_LOGE_STR("`exp_dig` attribute: expecting 11 because `mant_dig` is 53.");
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

	*float_decl = ctf_field_class_float_create();
	BT_ASSERT(*float_decl);
	(*float_decl)->base.base.alignment = alignment;
	(*float_decl)->base.byte_order = byte_order;
	(*float_decl)->base.size = mant_dig + exp_dig;
	return 0;

error:
	ctf_field_class_destroy((void *) *float_decl);
	*float_decl = NULL;
	return ret;
}

static
int visit_string_decl(struct ctx *ctx,
		struct bt_list_head *expressions,
		struct ctf_field_class_string **string_decl)
{
	int set = 0;
	int ret = 0;
	struct ctf_node *expression;
	enum ctf_encoding encoding = CTF_ENCODING_UTF8;

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
					"string field class");
				ret = -EPERM;
				goto error;
			}

			if (right->u.unary_expression.type != UNARY_STRING) {
				_BT_LOGE_NODE(right,
					"Invalid `encoding` attribute in string field class: "
					"expecting unary string.");
				ret = -EINVAL;
				goto error;
			}

			s_right = concatenate_unary_strings(
				&expression->u.ctf_expression.right);
			if (!s_right) {
				_BT_LOGE_NODE(right,
					"Unexpected unary expression for string field class's `encoding` attribute.");
				ret = -EINVAL;
				goto error;
			}

			if (!strcmp(s_right, "UTF8") ||
					!strcmp(s_right, "utf8") ||
					!strcmp(s_right, "utf-8") ||
					!strcmp(s_right, "UTF-8") ||
					!strcmp(s_right, "ASCII") ||
					!strcmp(s_right, "ascii")) {
				encoding = CTF_ENCODING_UTF8;
			} else if (!strcmp(s_right, "none")) {
				encoding = CTF_ENCODING_NONE;
			} else {
				_BT_LOGE_NODE(right,
					"Invalid `encoding` attribute in string field class: "
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
				"Unknown attribute in string field class: "
				"attr-name=\"%s\"",
				left->u.unary_expression.u.string);
		}
	}

	*string_decl = ctf_field_class_string_create();
	BT_ASSERT(*string_decl);
	(*string_decl)->encoding = encoding;
	return 0;

error:
	ctf_field_class_destroy((void *) *string_decl);
	*string_decl = NULL;
	return ret;
}

static
int visit_field_class_specifier_list(struct ctx *ctx,
		struct ctf_node *ts_list, struct ctf_field_class **decl)
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

	first = _BT_LIST_FIRST_ENTRY(&ts_list->u.field_class_specifier_list.head,
		struct ctf_node, siblings);
	if (first->type != NODE_TYPE_SPECIFIER) {
		_BT_LOGE_NODE(first,
			"Unexpected node type: node-type=%d", first->type);
		ret = -EINVAL;
		goto error;
	}

	node = first->u.field_class_specifier.node;

	switch (first->u.field_class_specifier.type) {
	case TYPESPEC_INTEGER:
		ret = visit_integer_decl(ctx, &node->u.integer.expressions,
			(void *) decl);
		if (ret) {
			BT_ASSERT(!*decl);
			goto error;
		}
		break;
	case TYPESPEC_FLOATING_POINT:
		ret = visit_floating_point_number_decl(ctx,
			&node->u.floating_point.expressions, (void *) decl);
		if (ret) {
			BT_ASSERT(!*decl);
			goto error;
		}
		break;
	case TYPESPEC_STRING:
		ret = visit_string_decl(ctx,
			&node->u.string.expressions, (void *) decl);
		if (ret) {
			BT_ASSERT(!*decl);
			goto error;
		}
		break;
	case TYPESPEC_STRUCT:
		ret = visit_struct_decl(ctx, node->u._struct.name,
			&node->u._struct.declaration_list,
			node->u._struct.has_body,
			&node->u._struct.min_align, (void *) decl);
		if (ret) {
			BT_ASSERT(!*decl);
			goto error;
		}
		break;
	case TYPESPEC_VARIANT:
		ret = visit_variant_decl(ctx, node->u.variant.name,
			node->u.variant.choice,
			&node->u.variant.declaration_list,
			node->u.variant.has_body, (void *) decl);
		if (ret) {
			BT_ASSERT(!*decl);
			goto error;
		}
		break;
	case TYPESPEC_ENUM:
		ret = visit_enum_decl(ctx, node->u._enum.enum_id,
			node->u._enum.container_field_class,
			&node->u._enum.enumerator_list,
			node->u._enum.has_body, (void *) decl);
		if (ret) {
			BT_ASSERT(!*decl);
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
		ret = visit_field_class_specifier(ctx, ts_list, decl);
		if (ret) {
			_BT_LOGE_NODE(first,
				"Cannot visit field class specifier: ret=%d",
				ret);
			BT_ASSERT(!*decl);
			goto error;
		}
		break;
	default:
		_BT_LOGE_NODE(first,
			"Unexpected field class specifier type: node-type=%d",
			first->u.field_class_specifier.type);
		ret = -EINVAL;
		goto error;
	}

	BT_ASSERT(*decl);
	return 0;

error:
	ctf_field_class_destroy((void *) *decl);
	*decl = NULL;
	return ret;
}

static
int visit_event_decl_entry(struct ctx *ctx, struct ctf_node *node,
		struct ctf_event_class *event_class, uint64_t *stream_id,
		int *set)
{
	int ret = 0;
	char *left = NULL;

	switch (node->type) {
	case NODE_TYPEDEF:
		ret = visit_field_class_def(ctx, node->u.field_class_def.field_class_specifier_list,
			&node->u.field_class_def.field_class_declarators);
		if (ret) {
			_BT_LOGE_NODE(node,
				"Cannot add field class found in event class.");
			goto error;
		}
		break;
	case NODE_TYPEALIAS:
		ret = visit_field_class_alias(ctx, node->u.field_class_alias.target,
			node->u.field_class_alias.alias);
		if (ret) {
			_BT_LOGE_NODE(node,
				"Cannot add field class alias found in event class.");
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
				_BT_LOGE_DUP_ATTR(node, "name", "event class");
				ret = -EPERM;
				goto error;
			}

			_SET(set, _EVENT_NAME_SET);
		} else if (!strcmp(left, "id")) {
			int64_t id = -1;

			if (_IS_SET(set, _EVENT_ID_SET)) {
				_BT_LOGE_DUP_ATTR(node, "id", "event class");
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

			event_class->id = id;
			_SET(set, _EVENT_ID_SET);
		} else if (!strcmp(left, "stream_id")) {
			if (_IS_SET(set, _EVENT_STREAM_ID_SET)) {
				_BT_LOGE_DUP_ATTR(node, "stream_id",
					"event class");
				ret = -EPERM;
				goto error;
			}

			ret = get_unary_unsigned(&node->u.ctf_expression.right,
				stream_id);

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

			ret = visit_field_class_specifier_list(ctx,
				_BT_LIST_FIRST_ENTRY(
					&node->u.ctf_expression.right,
					struct ctf_node, siblings),
				&event_class->spec_context_fc);
			if (ret) {
				_BT_LOGE_NODE(node,
					"Cannot create event class's context field class.");
				goto error;
			}

			BT_ASSERT(event_class->spec_context_fc);
			_SET(set, _EVENT_CONTEXT_SET);
		} else if (!strcmp(left, "fields")) {
			if (_IS_SET(set, _EVENT_FIELDS_SET)) {
				_BT_LOGE_NODE(node,
					"Duplicate `fields` entry in event class.");
				ret = -EPERM;
				goto error;
			}

			ret = visit_field_class_specifier_list(ctx,
				_BT_LIST_FIRST_ENTRY(
					&node->u.ctf_expression.right,
					struct ctf_node, siblings),
				&event_class->payload_fc);
			if (ret) {
				_BT_LOGE_NODE(node,
					"Cannot create event class's payload field class.");
				goto error;
			}

			BT_ASSERT(event_class->payload_fc);
			_SET(set, _EVENT_FIELDS_SET);
		} else if (!strcmp(left, "loglevel")) {
			uint64_t loglevel_value;
			bt_event_class_log_level log_level = -1;

			if (_IS_SET(set, _EVENT_LOG_LEVEL_SET)) {
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

			if (log_level != -1) {
				event_class->log_level = log_level;
			}

			_SET(set, _EVENT_LOG_LEVEL_SET);
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
				g_string_assign(event_class->emf_uri,
					right);
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

	goto end;

error:
	if (left) {
		g_free(left);
	}

end:
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
int visit_event_decl(struct ctx *ctx, struct ctf_node *node)
{
	int ret = 0;
	int set = 0;
	struct ctf_node *iter;
	uint64_t stream_id = 0;
	char *event_name = NULL;
	struct ctf_event_class *event_class = NULL;
	struct ctf_stream_class *stream_class = NULL;
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

	event_class = ctf_event_class_create();
	BT_ASSERT(event_class);
	g_string_assign(event_class->name, event_name);
	_TRY_PUSH_SCOPE_OR_GOTO_ERROR();
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
		/*
		 * Allow missing stream_id if there is only a single
		 * stream class.
		 */
		switch (ctx->ctf_tc->stream_classes->len) {
		case 0:
			/* Create implicit stream class if there's none */
			stream_id = 0;
			stream_class = ctf_stream_class_create();
			BT_ASSERT(stream_class);
			stream_class->id = stream_id;
			g_ptr_array_add(ctx->ctf_tc->stream_classes,
				stream_class);
			break;
		case 1:
			/* Single stream class: get its ID */
			stream_class = ctx->ctf_tc->stream_classes->pdata[0];
			stream_id = stream_class->id;
			break;
		default:
			_BT_LOGE_NODE(node,
				"Missing `stream_id` attribute in event class.");
			ret = -EPERM;
			goto error;
		}
	}

	/* We have the stream ID now; get the stream class if found */
	if (!stream_class) {
		stream_class = ctf_trace_class_borrow_stream_class_by_id(
			ctx->ctf_tc, stream_id);
		if (!stream_class) {
			_BT_LOGE_NODE(node,
				"Cannot find stream class at this point: "
				"id=%" PRId64, stream_id);
			ret = -EINVAL;
			goto error;
		}
	}

	BT_ASSERT(stream_class);

	if (!_IS_SET(&set, _EVENT_ID_SET)) {
		/* Allow only one event without ID per stream */
		if (stream_class->event_classes->len != 0) {
			_BT_LOGE_NODE(node,
				"Missing `id` attribute in event class.");
			ret = -EPERM;
			goto error;
		}

		/* Automatic ID */
		event_class->id = 0;
	}

	if (ctf_stream_class_borrow_event_class_by_id(stream_class,
			event_class->id)) {
		_BT_LOGE_NODE(node,
			"Duplicate event class (same ID) in the same stream class: "
			"id=%" PRId64, event_class->id);
		ret = -EEXIST;
		goto error;
	}

	ctf_stream_class_append_event_class(stream_class, event_class);
	event_class = NULL;
	goto end;

error:
	ctf_event_class_destroy(event_class);
	event_class = NULL;

	if (ret >= 0) {
		ret = -1;
	}

end:
	if (pop_scope) {
		ctx_pop_scope(ctx);
	}

	if (event_name) {
		g_free(event_name);
	}

	return ret;
}

static
int auto_map_field_to_trace_clock_class(struct ctx *ctx,
		struct ctf_field_class *fc)
{
	struct ctf_clock_class *clock_class_to_map_to = NULL;
	struct ctf_field_class_int *int_fc = (void *) fc;
	int ret = 0;
	uint64_t clock_class_count;

	if (!fc) {
		goto end;
	}

	if (fc->type != CTF_FIELD_CLASS_TYPE_INT &&
			fc->type != CTF_FIELD_CLASS_TYPE_ENUM) {
		goto end;
	}

	if (int_fc->mapped_clock_class) {
		/* Already mapped */
		goto end;
	}

	clock_class_count = ctx->ctf_tc->clock_classes->len;

	switch (clock_class_count) {
	case 0:
		/*
		 * No clock class exists in the trace at this point. Create an
		 * implicit one at 1 GHz, named `default`, and use this clock
		 * class.
		 */
		clock_class_to_map_to = ctf_clock_class_create();
		BT_ASSERT(clock_class_to_map_to);
		clock_class_to_map_to->frequency = UINT64_C(1000000000);
		g_string_assign(clock_class_to_map_to->name, "default");
		BT_ASSERT(ret == 0);
		g_ptr_array_add(ctx->ctf_tc->clock_classes,
			clock_class_to_map_to);
		break;
	case 1:
		/*
		 * Only one clock class exists in the trace at this point: use
		 * this one.
		 */
		clock_class_to_map_to = ctx->ctf_tc->clock_classes->pdata[0];
		break;
	default:
		/*
		 * Timestamp field not mapped to a clock class and there's more
		 * than one clock class in the trace: this is an error.
		 */
		BT_LOGE_STR("Timestamp field found with no mapped clock class, "
			"but there's more than one clock class in the trace at this point.");
		ret = -1;
		goto end;
	}

	BT_ASSERT(clock_class_to_map_to);
	int_fc->mapped_clock_class = clock_class_to_map_to;

end:
	return ret;
}

static
int auto_map_fields_to_trace_clock_class(struct ctx *ctx,
		struct ctf_field_class *root_fc, const char *field_name)
{
	int ret = 0;
	uint64_t i, count;
	struct ctf_field_class_struct *struct_fc = (void *) root_fc;
	struct ctf_field_class_variant *var_fc = (void *) root_fc;

	if (!root_fc) {
		goto end;
	}

	if (root_fc->type != CTF_FIELD_CLASS_TYPE_STRUCT &&
			root_fc->type != CTF_FIELD_CLASS_TYPE_VARIANT) {
		goto end;
	}

	if (root_fc->type == CTF_FIELD_CLASS_TYPE_STRUCT) {
		count = struct_fc->members->len;
	} else {
		count = var_fc->options->len;
	}

	for (i = 0; i < count; i++) {
		struct ctf_named_field_class *named_fc = NULL;

		if (root_fc->type == CTF_FIELD_CLASS_TYPE_STRUCT) {
			named_fc = ctf_field_class_struct_borrow_member_by_index(
				struct_fc, i);
		} else if (root_fc->type == CTF_FIELD_CLASS_TYPE_VARIANT) {
			named_fc = ctf_field_class_variant_borrow_option_by_index(
				var_fc, i);
		}

		if (strcmp(named_fc->name->str, field_name) == 0) {
			ret = auto_map_field_to_trace_clock_class(ctx,
				named_fc->fc);
			if (ret) {
				BT_LOGE("Cannot automatically map field to trace's clock class: "
					"field-name=\"%s\"", field_name);
				goto end;
			}
		}

		ret = auto_map_fields_to_trace_clock_class(ctx, named_fc->fc,
			field_name);
		if (ret) {
			BT_LOGE("Cannot automatically map structure or variant field class's fields to trace's clock class: "
				"field-name=\"%s\", root-field-name=\"%s\"",
				field_name, named_fc->name->str);
			goto end;
		}
	}

end:
	return ret;
}

static
int visit_stream_decl_entry(struct ctx *ctx, struct ctf_node *node,
		struct ctf_stream_class *stream_class, int *set)
{
	int ret = 0;
	char *left = NULL;

	switch (node->type) {
	case NODE_TYPEDEF:
		ret = visit_field_class_def(ctx, node->u.field_class_def.field_class_specifier_list,
			&node->u.field_class_def.field_class_declarators);
		if (ret) {
			_BT_LOGE_NODE(node,
				"Cannot add field class found in stream class.");
			goto error;
		}
		break;
	case NODE_TYPEALIAS:
		ret = visit_field_class_alias(ctx, node->u.field_class_alias.target,
			node->u.field_class_alias.alias);
		if (ret) {
			_BT_LOGE_NODE(node,
				"Cannot add field class alias found in stream class.");
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

			if (ctf_trace_class_borrow_stream_class_by_id(
					ctx->ctf_tc, id)) {
				_BT_LOGE_NODE(node,
					"Duplicate stream class (same ID): id=%" PRId64,
					id);
				ret = -EEXIST;
				goto error;
			}

			stream_class->id = id;
			_SET(set, _STREAM_ID_SET);
		} else if (!strcmp(left, "event.header")) {
			if (_IS_SET(set, _STREAM_EVENT_HEADER_SET)) {
				_BT_LOGE_NODE(node,
					"Duplicate `event.header` entry in stream class.");
				ret = -EPERM;
				goto error;
			}

			ret = visit_field_class_specifier_list(ctx,
				_BT_LIST_FIRST_ENTRY(
					&node->u.ctf_expression.right,
					struct ctf_node, siblings),
				&stream_class->event_header_fc);
			if (ret) {
				_BT_LOGE_NODE(node,
					"Cannot create stream class's event header field class.");
				goto error;
			}

			BT_ASSERT(stream_class->event_header_fc);
			ret = auto_map_fields_to_trace_clock_class(ctx,
				stream_class->event_header_fc, "timestamp");
			if (ret) {
				_BT_LOGE_NODE(node,
					"Cannot automatically map specific event header field class fields named `timestamp` to trace's clock class.");
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

			ret = visit_field_class_specifier_list(ctx,
				_BT_LIST_FIRST_ENTRY(
					&node->u.ctf_expression.right,
					struct ctf_node, siblings),
				&stream_class->event_common_context_fc);
			if (ret) {
				_BT_LOGE_NODE(node,
					"Cannot create stream class's event context field class.");
				goto error;
			}

			BT_ASSERT(stream_class->event_common_context_fc);
			_SET(set, _STREAM_EVENT_CONTEXT_SET);
		} else if (!strcmp(left, "packet.context")) {
			if (_IS_SET(set, _STREAM_PACKET_CONTEXT_SET)) {
				_BT_LOGE_NODE(node,
					"Duplicate `packet.context` entry in stream class.");
				ret = -EPERM;
				goto error;
			}

			ret = visit_field_class_specifier_list(ctx,
				_BT_LIST_FIRST_ENTRY(
					&node->u.ctf_expression.right,
					struct ctf_node, siblings),
				&stream_class->packet_context_fc);
			if (ret) {
				_BT_LOGE_NODE(node,
					"Cannot create stream class's packet context field class.");
				goto error;
			}

			BT_ASSERT(stream_class->packet_context_fc);
			ret = auto_map_fields_to_trace_clock_class(ctx,
				stream_class->packet_context_fc,
				"timestamp_begin");
			if (ret) {
				_BT_LOGE_NODE(node,
					"Cannot automatically map specific packet context field class fields named `timestamp_begin` to trace's clock class.");
				goto error;
			}

			ret = auto_map_fields_to_trace_clock_class(ctx,
				stream_class->packet_context_fc,
				"timestamp_end");
			if (ret) {
				_BT_LOGE_NODE(node,
					"Cannot automatically map specific packet context field class fields named `timestamp_end` to trace's clock class.");
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
	return ret;
}

static
int visit_stream_decl(struct ctx *ctx, struct ctf_node *node)
{
	int set = 0;
	int ret = 0;
	struct ctf_node *iter;
	struct ctf_stream_class *stream_class = NULL;
	struct bt_list_head *decl_list = &node->u.stream.declaration_list;

	if (node->visited) {
		goto end;
	}

	node->visited = TRUE;
	stream_class = ctf_stream_class_create();
	BT_ASSERT(stream_class);
	_TRY_PUSH_SCOPE_OR_GOTO_ERROR();

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
		/* Check that packet header has `stream_id` field */
		struct ctf_named_field_class *named_fc = NULL;

		if (!ctx->ctf_tc->packet_header_fc) {
			_BT_LOGE_NODE(node,
				"Stream class has a `id` attribute, "
				"but trace has no packet header field class.");
			goto error;
		}

		named_fc = ctf_field_class_struct_borrow_member_by_name(
			(void *) ctx->ctf_tc->packet_header_fc, "stream_id");
		if (!named_fc) {
			_BT_LOGE_NODE(node,
				"Stream class has a `id` attribute, "
				"but trace's packet header field class has no `stream_id` field.");
			goto error;
		}

		if (named_fc->fc->type != CTF_FIELD_CLASS_TYPE_INT &&
				named_fc->fc->type != CTF_FIELD_CLASS_TYPE_ENUM) {
			_BT_LOGE_NODE(node,
				"Stream class has a `id` attribute, "
				"but trace's packet header field class's `stream_id` field is not an integer field class.");
			goto error;
		}
	} else {
		/* Allow only _one_ ID-less stream */
		if (ctx->ctf_tc->stream_classes->len != 0) {
			_BT_LOGE_NODE(node,
				"Missing `id` attribute in stream class as there's more than one stream class in the trace.");
			ret = -EPERM;
			goto error;
		}

		/* Automatic ID: 0 */
		stream_class->id = 0;
	}

	/*
	 * Make sure that this stream class's ID is currently unique in
	 * the trace.
	 */
	if (ctf_trace_class_borrow_stream_class_by_id(ctx->ctf_tc,
			stream_class->id)) {
		_BT_LOGE_NODE(node,
			"Duplicate stream class (same ID): id=%" PRId64,
			stream_class->id);
		ret = -EINVAL;
		goto error;
	}

	g_ptr_array_add(ctx->ctf_tc->stream_classes, stream_class);
	stream_class = NULL;
	goto end;

error:
	ctf_stream_class_destroy(stream_class);
	stream_class = NULL;

end:
	return ret;
}

static
int visit_trace_decl_entry(struct ctx *ctx, struct ctf_node *node, int *set)
{
	int ret = 0;
	char *left = NULL;
	uint64_t val;

	switch (node->type) {
	case NODE_TYPEDEF:
		ret = visit_field_class_def(ctx, node->u.field_class_def.field_class_specifier_list,
			&node->u.field_class_def.field_class_declarators);
		if (ret) {
			_BT_LOGE_NODE(node,
				"Cannot add field class found in trace (`trace` block).");
			goto error;
		}
		break;
	case NODE_TYPEALIAS:
		ret = visit_field_class_alias(ctx, node->u.field_class_alias.target,
			node->u.field_class_alias.alias);
		if (ret) {
			_BT_LOGE_NODE(node,
				"Cannot add field class alias found in trace (`trace` block).");
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
				&val);
			if (ret) {
				_BT_LOGE_NODE(node,
					"Unexpected unary expression for trace's `major` attribute.");
				ret = -EINVAL;
				goto error;
			}

			if (val != 1) {
				_BT_LOGE_NODE(node,
					"Invalid trace's `minor` attribute: expecting 1.");
				goto error;
			}

			ctx->ctf_tc->major = val;
			_SET(set, _TRACE_MAJOR_SET);
		} else if (!strcmp(left, "minor")) {
			if (_IS_SET(set, _TRACE_MINOR_SET)) {
				_BT_LOGE_DUP_ATTR(node, "minor", "trace");
				ret = -EPERM;
				goto error;
			}

			ret = get_unary_unsigned(&node->u.ctf_expression.right,
				&val);
			if (ret) {
				_BT_LOGE_NODE(node,
					"Unexpected unary expression for trace's `minor` attribute.");
				ret = -EINVAL;
				goto error;
			}

			if (val != 8) {
				_BT_LOGE_NODE(node,
					"Invalid trace's `minor` attribute: expecting 8.");
				goto error;
			}

			ctx->ctf_tc->minor = val;
			_SET(set, _TRACE_MINOR_SET);
		} else if (!strcmp(left, "uuid")) {
			if (_IS_SET(set, _TRACE_UUID_SET)) {
				_BT_LOGE_DUP_ATTR(node, "uuid", "trace");
				ret = -EPERM;
				goto error;
			}

			ret = get_unary_uuid(&node->u.ctf_expression.right,
				ctx->ctf_tc->uuid);
			if (ret) {
				_BT_LOGE_NODE(node,
					"Invalid trace's `uuid` attribute.");
				goto error;
			}

			ctx->ctf_tc->is_uuid_set = true;
			_SET(set, _TRACE_UUID_SET);
		} else if (!strcmp(left, "byte_order")) {
			/* Default byte order is already known at this stage */
			if (_IS_SET(set, _TRACE_BYTE_ORDER_SET)) {
				_BT_LOGE_DUP_ATTR(node, "byte_order",
					"trace");
				ret = -EPERM;
				goto error;
			}

			BT_ASSERT(ctx->ctf_tc->default_byte_order != -1);
			_SET(set, _TRACE_BYTE_ORDER_SET);
		} else if (!strcmp(left, "packet.header")) {
			if (_IS_SET(set, _TRACE_PACKET_HEADER_SET)) {
				_BT_LOGE_NODE(node,
					"Duplicate `packet.header` entry in trace.");
				ret = -EPERM;
				goto error;
			}

			ret = visit_field_class_specifier_list(ctx,
				_BT_LIST_FIRST_ENTRY(
					&node->u.ctf_expression.right,
					struct ctf_node, siblings),
				&ctx->ctf_tc->packet_header_fc);
			if (ret) {
				_BT_LOGE_NODE(node,
					"Cannot create trace's packet header field class.");
				goto error;
			}

			BT_ASSERT(ctx->ctf_tc->packet_header_fc);
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

	_TRY_PUSH_SCOPE_OR_GOTO_ERROR();

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

	ctx->is_trace_visited = true;

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
					ctx->is_lttng = true;
				}
			}

			ctf_trace_class_append_env_entry(ctx->ctf_tc,
				left, CTF_TRACE_CLASS_ENV_ENTRY_TYPE_STR,
				right, 0);
			g_free(right);
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

			ctf_trace_class_append_env_entry(ctx->ctf_tc,
				left, CTF_TRACE_CLASS_ENV_ENTRY_TYPE_INT,
				NULL, v);
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
				enum ctf_byte_order bo;

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
				if (bo == -1) {
					_BT_LOGE_NODE(node,
						"Invalid `byte_order` attribute in trace (`trace` block): "
						"expecting `le`, `be`, or `network`.");
					ret = -EINVAL;
					goto error;
				} else if (bo == CTF_BYTE_ORDER_DEFAULT) {
					_BT_LOGE_NODE(node,
						"Invalid `byte_order` attribute in trace (`trace` block): "
						"cannot be set to `native` here.");
					ret = -EPERM;
					goto error;
				}

				ctx->ctf_tc->default_byte_order = bo;
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
	struct ctf_clock_class *clock, int *set, int64_t *offset_seconds,
	uint64_t *offset_cycles)
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

		g_string_assign(clock->name, right);
		g_free(right);
		_SET(set, _CLOCK_NAME_SET);
	} else if (!strcmp(left, "uuid")) {
		uint8_t uuid[BABELTRACE_UUID_LEN];

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

		clock->has_uuid = true;
		memcpy(&clock->uuid[0], uuid, 16);
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

		g_string_assign(clock->description, right);
		g_free(right);
		_SET(set, _CLOCK_DESCRIPTION_SET);
	} else if (!strcmp(left, "freq")) {
		uint64_t freq = UINT64_C(-1);

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

		if (freq == UINT64_C(-1) || freq == 0) {
			_BT_LOGE_NODE(entry_node,
				"Invalid clock class frequency: freq=%" PRIu64,
				freq);
			ret = -EINVAL;
			goto error;
		}

		clock->frequency = freq;
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

		clock->precision = precision;
		_SET(set, _CLOCK_PRECISION_SET);
	} else if (!strcmp(left, "offset_s")) {
		if (_IS_SET(set, _CLOCK_OFFSET_S_SET)) {
			_BT_LOGE_DUP_ATTR(entry_node, "offset_s",
				"clock class");
			ret = -EPERM;
			goto error;
		}

		ret = get_unary_signed(
			&entry_node->u.ctf_expression.right, offset_seconds);
		if (ret) {
			_BT_LOGE_NODE(entry_node,
				"Unexpected unary expression for clock class's `offset_s` attribute.");
			ret = -EINVAL;
			goto error;
		}

		_SET(set, _CLOCK_OFFSET_S_SET);
	} else if (!strcmp(left, "offset")) {
		if (_IS_SET(set, _CLOCK_OFFSET_SET)) {
			_BT_LOGE_DUP_ATTR(entry_node, "offset", "clock class");
			ret = -EPERM;
			goto error;
		}

		ret = get_unary_unsigned(
			&entry_node->u.ctf_expression.right, offset_cycles);
		if (ret) {
			_BT_LOGE_NODE(entry_node,
				"Unexpected unary expression for clock class's `offset` attribute.");
			ret = -EINVAL;
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

		clock->is_absolute = ret;
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

static inline
uint64_t cycles_from_ns(uint64_t frequency, uint64_t ns)
{
	uint64_t cycles;

	/* 1GHz */
	if (frequency == UINT64_C(1000000000)) {
		cycles = ns;
	} else {
		cycles = (uint64_t) (((double) ns * (double) frequency) / 1e9);
	}

	return cycles;
}

static
void calibrate_clock_class_offsets(int64_t *offset_seconds,
		uint64_t *offset_cycles, uint64_t freq)
{
	if (*offset_cycles >= freq) {
		const uint64_t s_in_offset_cycles = *offset_cycles / freq;

		*offset_seconds += (int64_t) s_in_offset_cycles;
		*offset_cycles -= (s_in_offset_cycles * freq);
	}
}

static
void apply_clock_class_offset(struct ctx *ctx,
		struct ctf_clock_class *clock)
{
	uint64_t freq;
	int64_t offset_s_to_apply = ctx->decoder_config.clock_class_offset_s;
	uint64_t offset_ns_to_apply;
	int64_t cur_offset_s;
	uint64_t cur_offset_cycles;

	if (ctx->decoder_config.clock_class_offset_s == 0 &&
			ctx->decoder_config.clock_class_offset_ns == 0) {
		goto end;
	}

	/* Transfer nanoseconds to seconds as much as possible */
	if (ctx->decoder_config.clock_class_offset_ns < 0) {
		const int64_t abs_ns = -ctx->decoder_config.clock_class_offset_ns;
		const int64_t abs_extra_s = abs_ns / INT64_C(1000000000) + 1;
		const int64_t extra_s = -abs_extra_s;
		const int64_t offset_ns = ctx->decoder_config.clock_class_offset_ns -
			(extra_s * INT64_C(1000000000));

		BT_ASSERT(offset_ns > 0);
		offset_ns_to_apply = (uint64_t) offset_ns;
		offset_s_to_apply += extra_s;
	} else {
		const int64_t extra_s = ctx->decoder_config.clock_class_offset_ns /
			INT64_C(1000000000);
		const int64_t offset_ns = ctx->decoder_config.clock_class_offset_ns -
			(extra_s * INT64_C(1000000000));

		BT_ASSERT(offset_ns >= 0);
		offset_ns_to_apply = (uint64_t) offset_ns;
		offset_s_to_apply += extra_s;
	}

	freq = clock->frequency;
	cur_offset_s = clock->offset_seconds;
	cur_offset_cycles = clock->offset_cycles;

	/* Apply offsets */
	cur_offset_s += offset_s_to_apply;
	cur_offset_cycles += cycles_from_ns(freq, offset_ns_to_apply);

	/*
	 * Recalibrate offsets because the part in cycles can be greater
	 * than the frequency at this point.
	 */
	calibrate_clock_class_offsets(&cur_offset_s, &cur_offset_cycles, freq);

	/* Set final offsets */
	clock->offset_seconds = cur_offset_s;
	clock->offset_cycles = cur_offset_cycles;

end:
	return;
}

static
int visit_clock_decl(struct ctx *ctx, struct ctf_node *clock_node)
{
	int ret = 0;
	int set = 0;
	struct ctf_clock_class *clock;
	struct ctf_node *entry_node;
	struct bt_list_head *decl_list = &clock_node->u.clock.declaration_list;
	const char *clock_class_name;
	int64_t offset_seconds = 0;
	uint64_t offset_cycles = 0;
	uint64_t freq;

	if (clock_node->visited) {
		return 0;
	}

	clock_node->visited = TRUE;

	/* CTF 1.8's default frequency for a clock class is 1 GHz */
	clock = ctf_clock_class_create();
	if (!clock) {
		_BT_LOGE_NODE(clock_node,
			"Cannot create default clock class.");
		ret = -ENOMEM;
		goto end;
	}

	bt_list_for_each_entry(entry_node, decl_list, siblings) {
		ret = visit_clock_decl_entry(ctx, entry_node, clock, &set,
			&offset_seconds, &offset_cycles);
		if (ret) {
			_BT_LOGE_NODE(entry_node,
				"Cannot visit clock class's entry: ret=%d",
				ret);
			goto end;
		}
	}

	if (!_IS_SET(&set, _CLOCK_NAME_SET)) {
		_BT_LOGE_NODE(clock_node,
			"Missing `name` attribute in clock class.");
		ret = -EPERM;
		goto end;
	}

	clock_class_name = clock->name->str;
	BT_ASSERT(clock_class_name);
	if (ctx->is_lttng && strcmp(clock_class_name, "monotonic") == 0) {
		/*
		 * Old versions of LTTng forgot to set its clock class
		 * as absolute, even if it is. This is important because
		 * it's a condition to be able to sort messages
		 * from different sources.
		 */
		clock->is_absolute = true;
	}

	/*
	 * Adjust offsets so that the part in cycles is less than the
	 * frequency (move to the part in seconds).
	 */
	freq = clock->frequency;
	calibrate_clock_class_offsets(&offset_seconds, &offset_cycles, freq);
	BT_ASSERT(offset_cycles < clock->frequency);
	clock->offset_seconds = offset_seconds;
	clock->offset_cycles = offset_cycles;
	apply_clock_class_offset(ctx, clock);
	g_ptr_array_add(ctx->ctf_tc->clock_classes, clock);
	clock = NULL;

end:
	if (clock) {
		ctf_clock_class_destroy(clock);
	}

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
		ret = visit_field_class_def(ctx,
			root_decl_node->u.field_class_def.field_class_specifier_list,
			&root_decl_node->u.field_class_def.field_class_declarators);
		if (ret) {
			_BT_LOGE_NODE(root_decl_node,
				"Cannot add field class found in root scope.");
			goto end;
		}
		break;
	case NODE_TYPEALIAS:
		ret = visit_field_class_alias(ctx, root_decl_node->u.field_class_alias.target,
			root_decl_node->u.field_class_alias.alias);
		if (ret) {
			_BT_LOGE_NODE(root_decl_node,
				"Cannot add field class alias found in root scope.");
			goto end;
		}
		break;
	case NODE_TYPE_SPECIFIER_LIST:
	{
		struct ctf_field_class *decl = NULL;

		/*
		 * Just add the field class specifier to the root
		 * declaration scope. Put local reference.
		 */
		ret = visit_field_class_specifier_list(ctx, root_decl_node, &decl);
		if (ret) {
			_BT_LOGE_NODE(root_decl_node,
				"Cannot visit root scope's field class: "
				"ret=%d", ret);
			BT_ASSERT(!decl);
			goto end;
		}

		ctf_field_class_destroy(decl);
		decl = NULL;
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

BT_HIDDEN
struct ctf_visitor_generate_ir *ctf_visitor_generate_ir_create(
		bt_self_component_source *self_comp,
		const struct ctf_metadata_decoder_config *decoder_config)
{
	struct ctx *ctx = NULL;

	/* Create visitor's context */
	ctx = ctx_create(self_comp, decoder_config);
	if (!ctx) {
		BT_LOGE_STR("Cannot create visitor's context.");
		goto error;
	}

	goto end;

error:
	ctx_destroy(ctx);
	ctx = NULL;

end:
	return (void *) ctx;
}

BT_HIDDEN
void ctf_visitor_generate_ir_destroy(struct ctf_visitor_generate_ir *visitor)
{
	ctx_destroy((void *) visitor);
}

BT_HIDDEN
bt_trace_class *ctf_visitor_generate_ir_get_ir_trace_class(
		struct ctf_visitor_generate_ir *visitor)
{
	struct ctx *ctx = (void *) visitor;

	BT_ASSERT(ctx);

	if (ctx->trace_class) {
		bt_trace_class_get_ref(ctx->trace_class);
	}

	return ctx->trace_class;
}

BT_HIDDEN
struct ctf_trace_class *ctf_visitor_generate_ir_borrow_ctf_trace_class(
		struct ctf_visitor_generate_ir *visitor)
{
	struct ctx *ctx = (void *) visitor;

	BT_ASSERT(ctx);
	BT_ASSERT(ctx->ctf_tc);
	return ctx->ctf_tc;
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
		bool got_trace_decl = false;

		/*
		 * The first thing we need is the native byte order of
		 * the trace block, because early class aliases can have
		 * a `byte_order` attribute set to `native`. If we don't
		 * have the native byte order yet, and we don't have any
		 * trace block yet, then fail with EINCOMPLETE.
		 */
		if (ctx->ctf_tc->default_byte_order == -1) {
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

				got_trace_decl = true;
			}

			if (!got_trace_decl) {
				BT_LOGD_STR("Incomplete AST: need trace (`trace` block).");
				ret = -EINCOMPLETE;
				goto end;
			}
		}

		BT_ASSERT(ctx->ctf_tc->default_byte_order == CTF_BYTE_ORDER_LITTLE ||
			ctx->ctf_tc->default_byte_order == CTF_BYTE_ORDER_BIG);
		BT_ASSERT(ctx->current_scope &&
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

		BT_ASSERT(ctx->current_scope &&
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

		BT_ASSERT(ctx->current_scope &&
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

		BT_ASSERT(ctx->current_scope &&
			ctx->current_scope->parent_scope == NULL);

		/* Callsite blocks are not supported */
		bt_list_for_each_entry(iter, &node->u.root.callsite, siblings) {
			_BT_LOGW_NODE(iter,
				"\"callsite\" blocks are not supported as of this version.");
		}

		BT_ASSERT(ctx->current_scope &&
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

		BT_ASSERT(ctx->current_scope &&
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

		BT_ASSERT(ctx->current_scope &&
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

		BT_ASSERT(ctx->current_scope &&
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

	/* Update default clock classes */
	ret = ctf_trace_class_update_default_clock_classes(ctx->ctf_tc);
	if (ret) {
		ret = -EINVAL;
		goto end;
	}

	/* Update trace class meanings */
	ret = ctf_trace_class_update_meanings(ctx->ctf_tc);
	if (ret) {
		ret = -EINVAL;
		goto end;
	}

	/* Update text arrays and sequences */
	ret = ctf_trace_class_update_text_array_sequence(ctx->ctf_tc);
	if (ret) {
		ret = -EINVAL;
		goto end;
	}

	/* Resolve sequence lengths and variant tags */
	ret = ctf_trace_class_resolve_field_classes(ctx->ctf_tc);
	if (ret) {
		ret = -EINVAL;
		goto end;
	}

	if (ctx->trace_class) {
		/*
		 * Update "in IR" for field classes.
		 *
		 * If we have no IR trace class, then we'll have no way
		 * to create IR fields anyway, so we leave all the
		 * `in_ir` members false.
		 */
		ret = ctf_trace_class_update_in_ir(ctx->ctf_tc);
		if (ret) {
			ret = -EINVAL;
			goto end;
		}
	}

	/* Update saved value indexes */
	ret = ctf_trace_class_update_value_storing_indexes(ctx->ctf_tc);
	if (ret) {
		ret = -EINVAL;
		goto end;
	}

	/* Validate what we have so far */
	ret = ctf_trace_class_validate(ctx->ctf_tc);
	if (ret) {
		ret = -EINVAL;
		goto end;
	}

	/*
	 * If there are fields which are not related to the CTF format
	 * itself in the packet header and in event header field
	 * classes, warn about it because they are never translated.
	 */
	ctf_trace_class_warn_meaningless_header_fields(ctx->ctf_tc);

	if (ctx->trace_class) {
		/* Copy new CTF metadata -> new IR metadata */
		ret = ctf_trace_class_translate(ctx->trace_class, ctx->ctf_tc);
		if (ret) {
			ret = -EINVAL;
			goto end;
		}
	}

end:
	return ret;
}
