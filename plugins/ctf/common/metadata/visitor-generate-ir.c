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

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <assert.h>
#include <glib.h>
#include <inttypes.h>
#include <errno.h>
#include <babeltrace/compat/uuid-internal.h>
#include <babeltrace/endian-internal.h>
#include <babeltrace/ref.h>
#include <babeltrace/ctf-ir/trace.h>
#include <babeltrace/ctf-ir/stream-class.h>
#include <babeltrace/ctf-ir/event.h>
#include <babeltrace/ctf-ir/event-class.h>
#include <babeltrace/ctf-ir/field-types.h>
#include <babeltrace/ctf-ir/field-types-internal.h>
#include <babeltrace/ctf-ir/clock-class.h>

#include "scanner.h"
#include "parser.h"
#include "ast.h"

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

#define _BT_CTF_FIELD_TYPE_INIT(_name)	struct bt_ctf_field_type *_name = NULL;

/* Error printing wrappers */
#define _PERROR(_fmt, ...)					\
	do {							\
		if (!ctx->efd) break;				\
		fprintf(ctx->efd, "[error] %s: " _fmt "\n",	\
			__func__, __VA_ARGS__);			\
	} while (0)

#define _PWARNING(_fmt, ...)					\
	do {							\
		if (!ctx->efd) break;				\
		fprintf(ctx->efd, "[warning] %s: " _fmt "\n",	\
			__func__, __VA_ARGS__);			\
	} while (0)

#define _FPERROR(_stream, _fmt, ...)				\
	do {							\
		if (!_stream) break;				\
		fprintf(_stream, "[error] %s: " _fmt "\n",	\
			__func__, __VA_ARGS__);			\
	} while (0)

#define _FPWARNING(_stream, _fmt, ...)				\
	do {							\
		if (!_stream) break;				\
		fprintf(_stream, "[warning] %s: " _fmt "\n",	\
			__func__, __VA_ARGS__);			\
	} while (0)

#define _PERROR_DUP_ATTR(_attr, _entity)			\
	do {							\
		if (!ctx->efd) break;				\
		fprintf(ctx->efd,				\
			"[error] %s: duplicate attribute \""	\
			_attr "\" in " _entity "\n", __func__);	\
	} while (0)

/*
 * Declaration scope of a visitor context. This represents a TSDL
 * lexical scope, so that aliases and named structures, variants,
 * and enumerations may be registered and looked up hierarchically.
 */
struct ctx_decl_scope {
	/*
	 * Alias name to field type.
	 *
	 * GQuark -> struct bt_ctf_field_type *
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
	struct bt_ctf_trace *trace;

	/* Error stream to use during visit */
	FILE *efd;

	/* Current declaration scope (top of the stack) */
	struct ctx_decl_scope *current_scope;

	/* 1 if trace declaration is visited */
	int is_trace_visited;

	/* Offset (ns) to apply to clock classes on creation */
	uint64_t clock_class_offset_ns;

	/* Trace attributes */
	enum bt_ctf_byte_order trace_bo;
	uint64_t trace_major;
	uint64_t trace_minor;
	unsigned char trace_uuid[BABELTRACE_UUID_LEN];

	/*
	 * Stream IDs to stream classes.
	 *
	 * int64_t -> struct bt_ctf_stream_class *
	 */
	GHashTable *stream_classes;
};

/*
 * Visitor (public).
 */
struct ctf_visitor_generate_ir { };

static
const char *loglevel_str [] = {
	[ LOGLEVEL_EMERG ] = "TRACE_EMERG",
	[ LOGLEVEL_ALERT ] = "TRACE_ALERT",
	[ LOGLEVEL_CRIT ] = "TRACE_CRIT",
	[ LOGLEVEL_ERR ] = "TRACE_ERR",
	[ LOGLEVEL_WARNING ] = "TRACE_WARNING",
	[ LOGLEVEL_NOTICE ] = "TRACE_NOTICE",
	[ LOGLEVEL_INFO ] = "TRACE_INFO",
	[ LOGLEVEL_DEBUG_SYSTEM ] = "TRACE_DEBUG_SYSTEM",
	[ LOGLEVEL_DEBUG_PROGRAM ] = "TRACE_DEBUG_PROGRAM",
	[ LOGLEVEL_DEBUG_PROCESS ] = "TRACE_DEBUG_PROCESS",
	[ LOGLEVEL_DEBUG_MODULE ] = "TRACE_DEBUG_MODULE",
	[ LOGLEVEL_DEBUG_UNIT ] = "TRACE_DEBUG_UNIT",
	[ LOGLEVEL_DEBUG_FUNCTION ] = "TRACE_DEBUG_FUNCTION",
	[ LOGLEVEL_DEBUG_LINE ] = "TRACE_DEBUG_LINE",
	[ LOGLEVEL_DEBUG ] = "TRACE_DEBUG",
};

static
const char *print_loglevel(int64_t value)
{
	if (value < 0) {
		return NULL;
	}
	if (value >= _NR_LOGLEVELS) {
		return "<<UNKNOWN>>";
	}
	return loglevel_str[value];
}

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
		goto end;
	}

	scope->decl_map = g_hash_table_new_full(g_direct_hash, g_direct_equal,
		NULL, (GDestroyNotify) bt_ctf_field_type_put);
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
struct bt_ctf_field_type *ctx_decl_scope_lookup_prefix_alias(
	struct ctx_decl_scope *scope, char prefix,
	const char *name, int levels)
{
	GQuark qname = 0;
	int cur_levels = 0;
	_BT_CTF_FIELD_TYPE_INIT(decl);
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
			(gconstpointer) (unsigned long) qname);
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
struct bt_ctf_field_type *ctx_decl_scope_lookup_alias(
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
struct bt_ctf_field_type *ctx_decl_scope_lookup_enum(
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
struct bt_ctf_field_type *ctx_decl_scope_lookup_struct(
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
struct bt_ctf_field_type *ctx_decl_scope_lookup_variant(
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
	char prefix, const char *name, struct bt_ctf_field_type *decl)
{
	int ret = 0;
	GQuark qname = 0;
	_BT_CTF_FIELD_TYPE_INIT(edecl);

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
		(gpointer) (unsigned long) qname, decl);

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
	const char *name, struct bt_ctf_field_type *decl)
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
	const char *name, struct bt_ctf_field_type *decl)
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
	const char *name, struct bt_ctf_field_type *decl)
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
	const char *name, struct bt_ctf_field_type *decl)
{
	return ctx_decl_scope_register_prefix_alias(scope, _PREFIX_VARIANT,
		name, decl);
}

/**
 * Creates a new visitor context.
 *
 * @param trace	Associated trace
 * @param efd	Error stream
 * @returns	New visitor context, or NULL on error
 */
static
struct ctx *ctx_create(struct bt_ctf_trace *trace, FILE *efd,
		uint64_t clock_class_offset_ns)
{
	struct ctx *ctx = NULL;
	struct ctx_decl_scope *scope = NULL;

	ctx = g_new0(struct ctx, 1);
	if (!ctx) {
		goto error;
	}

	/* Root declaration scope */
	scope = ctx_decl_scope_create(NULL);
	if (!scope) {
		goto error;
	}

	ctx->stream_classes = g_hash_table_new_full(g_direct_hash,
		g_direct_equal, NULL, (GDestroyNotify) bt_put);
	if (!ctx->stream_classes) {
		goto error;
	}

	ctx->trace = trace;
	ctx->efd = efd;
	ctx->current_scope = scope;
	ctx->trace_bo = BT_CTF_BYTE_ORDER_NATIVE;
	ctx->clock_class_offset_ns = clock_class_offset_ns;
	return ctx;

error:
	g_free(ctx);
	ctx_decl_scope_destroy(scope);
	return NULL;
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
	g_hash_table_destroy(ctx->stream_classes);
	g_free(ctx);

end:
	return;
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
	struct bt_ctf_field_type **decl);

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

	bt_list_for_each_entry(node, head, siblings) {
		int uexpr_type = node->u.unary_expression.type;
		int uexpr_link = node->u.unary_expression.link;
		int cond = node->type != NODE_UNARY_EXPRESSION ||
			uexpr_type != UNARY_UNSIGNED_CONSTANT ||
			uexpr_link != UNARY_LINK_UNKNOWN || i != 0;
		if (cond) {
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
			(uexpr_type != UNARY_UNSIGNED_CONSTANT) ||
			(uexpr_type != UNARY_UNSIGNED_CONSTANT &&
				uexpr_type != UNARY_SIGNED_CONSTANT) ||
			uexpr_link != UNARY_LINK_UNKNOWN || i != 0;
		if (cond) {
			ret = -EINVAL;
			goto end;
		}

		switch (node->u.unary_expression.type) {
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
			goto end;
		}
	}

end:
	return ret;
}

static
int get_boolean(FILE *efd, struct ctf_node *unary_expr)
{
	int ret = 0;

	if (unary_expr->type != NODE_UNARY_EXPRESSION) {
		_FPERROR(efd, "%s", "expecting unary expression");
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
			_FPERROR(efd, "unexpected string \"%s\"", str);
			ret = -EINVAL;
			goto end;
		}
		break;
	}
	default:
		_FPERROR(efd, "%s", "unexpected unary expression type");
		ret = -EINVAL;
		goto end;
	}

end:
	return ret;
}

static
enum bt_ctf_byte_order byte_order_from_unary_expr(FILE *efd,
	struct ctf_node *unary_expr)
{
	const char *str;
	enum bt_ctf_byte_order bo = BT_CTF_BYTE_ORDER_UNKNOWN;

	if (unary_expr->u.unary_expression.type != UNARY_STRING) {
		_FPERROR(efd, "%s",
			"\"byte_order\" attribute: expecting string");
		goto end;
	}

	str = unary_expr->u.unary_expression.u.string;

	if (!strcmp(str, "be") || !strcmp(str, "network")) {
		bo = BT_CTF_BYTE_ORDER_BIG_ENDIAN;
	} else if (!strcmp(str, "le")) {
		bo = BT_CTF_BYTE_ORDER_LITTLE_ENDIAN;
	} else if (!strcmp(str, "native")) {
		bo = BT_CTF_BYTE_ORDER_NATIVE;
	} else {
		_FPERROR(efd, "unexpected \"byte_order\" attribute value \"%s\"; should be \"be\", \"le\", \"network\", or \"native\"",
			str);
		goto end;
	}

end:
	return bo;
}

static
enum bt_ctf_byte_order get_real_byte_order(struct ctx *ctx,
	struct ctf_node *uexpr)
{
	enum bt_ctf_byte_order bo = byte_order_from_unary_expr(ctx->efd, uexpr);

	if (bo == BT_CTF_BYTE_ORDER_NATIVE) {
		bo = bt_ctf_trace_get_byte_order(ctx->trace);
	}

	return bo;
}

static
int is_align_valid(uint64_t align)
{
	return (align != 0) && !(align & (align - 1));
}

static
int get_type_specifier_name(struct ctx *ctx, struct ctf_node *type_specifier,
	GString *str)
{
	int ret = 0;

	if (type_specifier->type != NODE_TYPE_SPECIFIER) {
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
			_PERROR("%s", "unexpected empty structure name");
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
			_PERROR("%s", "unexpected empty variant name");
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
			_PERROR("%s", "unexpected empty enum name");
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
		_PERROR("%s", "unknown specifier");
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
	int ret;
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
	struct bt_ctf_field_type **field_decl,
	struct bt_ctf_field_type *nested_decl)
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
			ret = -EINVAL;
			goto error;
		}

		/* TODO: GCC bitfields not supported yet */
		if (node_type_declarator->u.type_declarator.bitfield_len !=
				NULL) {
			_PERROR("%s", "GCC bitfields are not supported as of this version");
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
			_BT_CTF_FIELD_TYPE_INIT(nested_decl_copy);

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
				_PERROR("cannot find typealias \"%s\"",
					g_quark_to_string(qalias));
				ret = -EINVAL;
				goto error;
			}

			/* Make a copy of it */
			nested_decl_copy = bt_ctf_field_type_copy(nested_decl);
			BT_PUT(nested_decl);
			if (!nested_decl_copy) {
				_PERROR("%s", "cannot copy nested declaration");
				ret = -EINVAL;
				goto error;
			}

			BT_MOVE(nested_decl, nested_decl_copy);

			/* Force integer's base to 16 since it's a pointer */
			if (bt_ctf_field_type_is_integer(nested_decl)) {
				bt_ctf_field_type_integer_set_base(nested_decl,
					BT_CTF_INTEGER_BASE_HEXADECIMAL);
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

			*field_name = g_quark_from_string(id);
		} else {
			*field_name = 0;
		}

		BT_MOVE(*field_decl, nested_decl);
		goto end;
	} else {
		struct ctf_node *first;
		_BT_CTF_FIELD_TYPE_INIT(decl);
		_BT_CTF_FIELD_TYPE_INIT(outer_field_decl);
		struct bt_list_head *length =
			&node_type_declarator->
				u.type_declarator.u.nested.length;

		/* Create array/sequence, pass nested_decl as child */
		if (bt_list_empty(length)) {
			_PERROR("%s",
				"expecting length field reference or value");
			ret = -EINVAL;
			goto error;
		}

		first = _BT_LIST_FIRST_ENTRY(length, struct ctf_node, siblings);
		if (first->type != NODE_UNARY_EXPRESSION) {
			ret = -EINVAL;
			goto error;
		}

		switch (first->u.unary_expression.type) {
		case UNARY_UNSIGNED_CONSTANT:
		{
			size_t len;
			_BT_CTF_FIELD_TYPE_INIT(array_decl);

			len = first->u.unary_expression.u.unsigned_constant;
			array_decl = bt_ctf_field_type_array_create(nested_decl,
				len);
			BT_PUT(nested_decl);
			if (!array_decl) {
				_PERROR("%s",
					"cannot create array declaration");
				ret = -ENOMEM;
				goto error;
			}

			BT_MOVE(decl, array_decl);
			break;
		}
		case UNARY_STRING:
		{
			/* Lookup unsigned integer definition, create seq. */
			_BT_CTF_FIELD_TYPE_INIT(seq_decl);
			char *length_name = concatenate_unary_strings(length);

			if (!length_name) {
				ret = -EINVAL;
				goto error;
			}

			seq_decl = bt_ctf_field_type_sequence_create(
				nested_decl, length_name);
			g_free(length_name);
			BT_PUT(nested_decl);
			if (!seq_decl) {
				_PERROR("%s",
					"cannot create sequence declaration");
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
	struct bt_ctf_field_type *struct_decl,
	struct ctf_node *type_specifier_list,
	struct bt_list_head *type_declarators)
{
	int ret = 0;
	struct ctf_node *iter;
	_BT_CTF_FIELD_TYPE_INIT(field_decl);

	bt_list_for_each_entry(iter, type_declarators, siblings) {
		field_decl = NULL;
		GQuark qfield_name;
		const char *field_name;
		_BT_CTF_FIELD_TYPE_INIT(efield_decl);

		ret = visit_type_declarator(ctx, type_specifier_list,
			&qfield_name, iter, &field_decl, NULL);
		if (ret) {
			assert(!field_decl);
			_PERROR("%s", "unable to find structure field declaration type");
			goto error;
		}

		assert(field_decl);
		field_name = g_quark_to_string(qfield_name);

		/* Check if field with same name already exists */
		efield_decl =
			bt_ctf_field_type_structure_get_field_type_by_name(
				struct_decl, field_name);
		if (efield_decl) {
			BT_PUT(efield_decl);
			_PERROR("duplicate field \"%s\" in structure",
				field_name);
			ret = -EINVAL;
			goto error;
		}

		/* Add field to structure */
		ret = bt_ctf_field_type_structure_add_field(struct_decl,
			field_decl, field_name);
		BT_PUT(field_decl);
		if (ret) {
			_PERROR("cannot add field \"%s\" to structure",
				g_quark_to_string(qfield_name));
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
	struct bt_ctf_field_type *variant_decl,
	struct ctf_node *type_specifier_list,
	struct bt_list_head *type_declarators)
{
	int ret = 0;
	struct ctf_node *iter;
	_BT_CTF_FIELD_TYPE_INIT(field_decl);

	bt_list_for_each_entry(iter, type_declarators, siblings) {
		field_decl = NULL;
		GQuark qfield_name;
		const char *field_name;
		_BT_CTF_FIELD_TYPE_INIT(efield_decl);

		ret = visit_type_declarator(ctx, type_specifier_list,
			&qfield_name, iter, &field_decl, NULL);
		if (ret) {
			assert(!field_decl);
			_PERROR("%s",
				"unable to find variant field declaration type");
			goto error;
		}

		assert(field_decl);
		field_name = g_quark_to_string(qfield_name);

		/* Check if field with same name already exists */
		efield_decl =
			bt_ctf_field_type_variant_get_field_type_by_name(
				variant_decl, field_name);
		if (efield_decl) {
			BT_PUT(efield_decl);
			_PERROR("duplicate field \"%s\" in variant",
				field_name);
			ret = -EINVAL;
			goto error;
		}

		/* Add field to structure */
		ret = bt_ctf_field_type_variant_add_field(variant_decl,
			field_decl, field_name);
		BT_PUT(field_decl);
		if (ret) {
			_PERROR("cannot add field \"%s\" to variant",
				g_quark_to_string(qfield_name));
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
	_BT_CTF_FIELD_TYPE_INIT(type_decl);

	bt_list_for_each_entry(iter, type_declarators, siblings) {
		ret = visit_type_declarator(ctx, type_specifier_list,
			&qidentifier, iter, &type_decl, NULL);
		if (ret) {
			_PERROR("%s", "problem creating type declaration");
			ret = -EINVAL;
			goto end;
		}

		/* Do not allow typedef and typealias of untagged variants */
		if (bt_ctf_field_type_is_variant(type_decl)) {
			if (bt_ctf_field_type_variant_get_tag_name(type_decl)) {
				_PERROR("%s", "typedef of untagged variant is not allowed");
				ret = -EPERM;
				goto end;
			}
		}

		ret = ctx_decl_scope_register_alias(ctx->current_scope,
			g_quark_to_string(qidentifier), type_decl);
		if (ret) {
			_PERROR("cannot register typedef \"%s\"",
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
	_BT_CTF_FIELD_TYPE_INIT(type_decl);

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
		_PERROR("%s", "problem creating type declaration");
		goto end;
	}

	/* Do not allow typedef and typealias of untagged variants */
	if (bt_ctf_field_type_is_variant(type_decl)) {
		if (bt_ctf_field_type_variant_get_tag_name(type_decl)) {
			_PERROR("%s",
				"typealias of untagged variant is not allowed");
			ret = -EPERM;
			goto end;
		}
	}

	/*
	 * The semantic validator does not check whether the target is
	 * abstract or not (if it has an identifier). Check it here.
	 */
	if (qdummy_field_name != 0) {
		_PERROR("%s", "expecting empty identifier");
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
		_PERROR("cannot register typealias \"%s\"",
			g_quark_to_string(qalias));
		goto end;
	}

end:
	BT_PUT(type_decl);

	return ret;
}

static
int visit_struct_decl_entry(struct ctx *ctx, struct ctf_node *entry_node,
	struct bt_ctf_field_type *struct_decl)
{
	int ret = 0;

	switch (entry_node->type) {
	case NODE_TYPEDEF:
		ret = visit_typedef(ctx,
			entry_node->u._typedef.type_specifier_list,
			&entry_node->u._typedef.type_declarators);
		if (ret) {
			_PERROR("%s",
				"cannot add typedef in \"struct\" declaration");
			goto end;
		}
		break;
	case NODE_TYPEALIAS:
		ret = visit_typealias(ctx, entry_node->u.typealias.target,
			entry_node->u.typealias.alias);
		if (ret) {
			_PERROR("%s",
				"cannot add typealias in \"struct\" declaration");
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
		_PERROR("unexpected node type: %d", (int) entry_node->type);
		ret = -EINVAL;
		goto end;
	}

end:
	return ret;
}

static
int visit_variant_decl_entry(struct ctx *ctx, struct ctf_node *entry_node,
	struct bt_ctf_field_type *variant_decl)
{
	int ret = 0;

	switch (entry_node->type) {
	case NODE_TYPEDEF:
		ret = visit_typedef(ctx,
			entry_node->u._typedef.type_specifier_list,
			&entry_node->u._typedef.type_declarators);
		if (ret) {
			_PERROR("%s",
				"cannot add typedef in \"variant\" declaration");
			goto end;
		}
		break;
	case NODE_TYPEALIAS:
		ret = visit_typealias(ctx, entry_node->u.typealias.target,
			entry_node->u.typealias.alias);
		if (ret) {
			_PERROR("%s",
				"cannot add typealias in \"variant\" declaration");
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
		_PERROR("unexpected node type: %d", (int) entry_node->type);
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
	struct bt_ctf_field_type **struct_decl)
{
	int ret = 0;

	*struct_decl = NULL;

	/* For named struct (without body), lookup in declaration scope */
	if (!has_body) {
		_BT_CTF_FIELD_TYPE_INIT(struct_decl_copy);

		if (!name) {
			ret = -EPERM;
			goto error;
		}

		*struct_decl = ctx_decl_scope_lookup_struct(ctx->current_scope,
			name, -1);
		if (!*struct_decl) {
			_PERROR("cannot find \"struct %s\"", name);
			ret = -EINVAL;
			goto error;
		}

		/* Make a copy of it */
		struct_decl_copy = bt_ctf_field_type_copy(*struct_decl);
		if (!struct_decl_copy) {
			_PERROR("%s",
				"cannot create copy of structure declaration");
			ret = -EINVAL;
			goto error;
		}

		BT_MOVE(*struct_decl, struct_decl_copy);
	} else {
		struct ctf_node *entry_node;
		uint64_t min_align_value = 0;

		if (name) {
			_BT_CTF_FIELD_TYPE_INIT(estruct_decl);

			estruct_decl = ctx_decl_scope_lookup_struct(
				ctx->current_scope, name, 1);
			if (estruct_decl) {
				BT_PUT(estruct_decl);
				_PERROR("\"struct %s\" already declared in local scope",
					name);
				ret = -EINVAL;
				goto error;
			}
		}

		if (!bt_list_empty(min_align)) {
			ret = get_unary_unsigned(min_align, &min_align_value);
			if (ret) {
				_PERROR("%s", "unexpected unary expression for structure declaration's \"align\" attribute");
				goto error;
			}
		}

		*struct_decl = bt_ctf_field_type_structure_create();
		if (!*struct_decl) {
			_PERROR("%s", "cannot create structure declaration");
			ret = -ENOMEM;
			goto error;
		}

		if (min_align_value != 0) {
			ret = bt_ctf_field_type_set_alignment(*struct_decl,
					min_align_value);
			if (ret) {
				_PERROR("%s", "failed to set structure's minimal alignment");
				goto error;
			}
		}

		ret = ctx_push_scope(ctx);
		if (ret) {
			_PERROR("%s", "cannot push scope");
			goto error;
		}

		bt_list_for_each_entry(entry_node, decl_list, siblings) {
			ret = visit_struct_decl_entry(ctx, entry_node,
				*struct_decl);
			if (ret) {
				ctx_pop_scope(ctx);
				goto error;
			}
		}

		ctx_pop_scope(ctx);

		if (name) {
			ret = ctx_decl_scope_register_struct(ctx->current_scope,
				name, *struct_decl);
			if (ret) {
				_PERROR("cannot register \"struct %s\" in declaration scope",
					name);
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
	int has_body, struct bt_ctf_field_type **variant_decl)
{
	int ret = 0;
	_BT_CTF_FIELD_TYPE_INIT(untagged_variant_decl);

	*variant_decl = NULL;

	/* For named variant (without body), lookup in declaration scope */
	if (!has_body) {
		_BT_CTF_FIELD_TYPE_INIT(variant_decl_copy);

		if (!name) {
			ret = -EPERM;
			goto error;
		}

		untagged_variant_decl =
			ctx_decl_scope_lookup_variant(ctx->current_scope,
				name, -1);
		if (!untagged_variant_decl) {
			_PERROR("cannot find \"variant %s\"", name);
			ret = -EINVAL;
			goto error;
		}

		/* Make a copy of it */
		variant_decl_copy = bt_ctf_field_type_copy(
			untagged_variant_decl);
		if (!variant_decl_copy) {
			_PERROR("%s",
				"cannot create copy of structure declaration");
			ret = -EINVAL;
			goto error;
		}

		BT_MOVE(untagged_variant_decl, variant_decl_copy);
	} else {
		struct ctf_node *entry_node;

		if (name) {
			struct bt_ctf_field_type *evariant_decl =
				ctx_decl_scope_lookup_struct(ctx->current_scope,
					name, 1);

			if (evariant_decl) {
				BT_PUT(evariant_decl);
				_PERROR("\"variant %s\" already declared in local scope",
					name);
				ret = -EINVAL;
				goto error;
			}
		}

		untagged_variant_decl = bt_ctf_field_type_variant_create(NULL,
			NULL);
		if (!untagged_variant_decl) {
			_PERROR("%s", "cannot create variant declaration");
			ret = -ENOMEM;
			goto error;
		}

		ret = ctx_push_scope(ctx);
		if (ret) {
			_PERROR("%s", "cannot push scope");
			goto error;
		}

		bt_list_for_each_entry(entry_node, decl_list, siblings) {
			ret = visit_variant_decl_entry(ctx, entry_node,
				untagged_variant_decl);
			if (ret) {
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
				_PERROR("cannot register \"variant %s\" in declaration scope",
					name);
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
		ret = bt_ctf_field_type_variant_set_tag_name(
			untagged_variant_decl, tag);
		if (ret) {
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
	struct bt_ctf_field_type *enum_decl, int64_t *last, int is_signed)
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
			_PERROR("wrong unary expression for enumeration label \"%s\"",
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
			_PERROR("invalid enumeration entry: \"%s\"",
				label);
			ret = -EINVAL;
			goto error;
		}

		if (nr_vals > 1) {
			_PERROR("invalid enumeration entry: \"%s\"",
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
		ret = bt_ctf_field_type_enumeration_add_mapping_unsigned(enum_decl,
			label, (uint64_t) start, (uint64_t) end);
	}
	if (ret) {
		_PERROR("cannot add mapping to enumeration for label \"%s\"",
			label);
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
	struct bt_ctf_field_type **enum_decl)
{
	int ret = 0;
	GQuark qdummy_id;
	_BT_CTF_FIELD_TYPE_INIT(integer_decl);

	*enum_decl = NULL;

	/* For named enum (without body), lookup in declaration scope */
	if (!has_body) {
		_BT_CTF_FIELD_TYPE_INIT(enum_decl_copy);

		if (!name) {
			ret = -EPERM;
			goto error;
		}

		*enum_decl = ctx_decl_scope_lookup_enum(ctx->current_scope,
			name, -1);
		if (!*enum_decl) {
			_PERROR("cannot find \"enum %s\"", name);
			ret = -EINVAL;
			goto error;
		}

		/* Make a copy of it */
		enum_decl_copy = bt_ctf_field_type_copy(*enum_decl);
		if (!enum_decl_copy) {
			_PERROR("%s",
				"cannot create copy of enumeration declaration");
			ret = -EINVAL;
			goto error;
		}

		BT_PUT(*enum_decl);
		BT_MOVE(*enum_decl, enum_decl_copy);
	} else {
		struct ctf_node *iter;
		int64_t last_value = 0;

		if (name) {
			_BT_CTF_FIELD_TYPE_INIT(eenum_decl);

			eenum_decl = ctx_decl_scope_lookup_enum(
				ctx->current_scope, name, 1);
			if (eenum_decl) {
				BT_PUT(eenum_decl);
				_PERROR("\"enum %s\" already declared in local scope",
					name);
				ret = -EINVAL;
				goto error;
			}
		}

		if (!container_type) {
			integer_decl = ctx_decl_scope_lookup_alias(
				ctx->current_scope, "int", -1);
			if (!integer_decl) {
				_PERROR("%s", "cannot find \"int\" type for enumeration");
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

		if (!bt_ctf_field_type_is_integer(integer_decl)) {
			_PERROR("%s", "container type for enumeration is not an integer");
			ret = -EINVAL;
			goto error;
		}

		*enum_decl = bt_ctf_field_type_enumeration_create(integer_decl);
		if (!*enum_decl) {
			_PERROR("%s", "cannot create enumeration declaration");
			ret = -ENOMEM;
			goto error;
		}

		bt_list_for_each_entry(iter, enumerator_list, siblings) {
			ret = visit_enum_decl_entry(ctx, iter, *enum_decl,
				&last_value,
				bt_ctf_field_type_integer_get_signed(integer_decl));
			if (ret) {
				goto error;
			}
		}

		if (name) {
			ret = ctx_decl_scope_register_enum(ctx->current_scope,
				name, *enum_decl);
			if (ret) {
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
	struct bt_ctf_field_type **decl)
{
	int ret = 0;
	GString *str = NULL;
	_BT_CTF_FIELD_TYPE_INIT(decl_copy);

	*decl = NULL;
	str = g_string_new("");
	ret = get_type_specifier_list_name(ctx, type_specifier_list, str);
	if (ret) {
		goto error;
	}

	*decl = ctx_decl_scope_lookup_alias(ctx->current_scope, str->str, -1);
	if (!*decl) {
		_PERROR("cannot find type alias \"%s\"", str->str);
		ret = -EINVAL;
		goto error;
	}

	/* Make a copy of the type declaration */
	decl_copy = bt_ctf_field_type_copy(*decl);
	if (!decl_copy) {
		_PERROR("%s", "cannot create copy of type declaration");
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
	struct bt_ctf_field_type **integer_decl)
{
	int set = 0;
	int ret = 0;
	int signedness = 0;
	struct ctf_node *expression;
	uint64_t alignment = 0, size = 0;
	struct bt_ctf_clock_class *mapped_clock = NULL;
	enum bt_ctf_string_encoding encoding = BT_CTF_STRING_ENCODING_NONE;
	enum bt_ctf_integer_base base = BT_CTF_INTEGER_BASE_DECIMAL;
	enum bt_ctf_byte_order byte_order =
		bt_ctf_trace_get_byte_order(ctx->trace);

	*integer_decl = NULL;

	bt_list_for_each_entry(expression, expressions, siblings) {
		struct ctf_node *left, *right;

		left = _BT_LIST_FIRST_ENTRY(&expression->u.ctf_expression.left,
			struct ctf_node, siblings);
		right = _BT_LIST_FIRST_ENTRY(
			&expression->u.ctf_expression.right, struct ctf_node,
			siblings);

		if (left->u.unary_expression.type != UNARY_STRING) {
			ret = -EINVAL;
			goto error;
		}

		if (!strcmp(left->u.unary_expression.u.string, "signed")) {
			if (_IS_SET(&set, _INTEGER_SIGNED_SET)) {
				_PERROR_DUP_ATTR("signed",
					"integer declaration");
				ret = -EPERM;
				goto error;
			}

			signedness = get_boolean(ctx->efd, right);
			if (signedness < 0) {
				ret = -EINVAL;
				goto error;
			}

			_SET(&set, _INTEGER_SIGNED_SET);
		} else if (!strcmp(left->u.unary_expression.u.string,
				"byte_order")) {
			if (_IS_SET(&set, _INTEGER_BYTE_ORDER_SET)) {
				_PERROR_DUP_ATTR("byte_order",
					"integer declaration");
				ret = -EPERM;
				goto error;
			}

			byte_order = get_real_byte_order(ctx, right);
			if (byte_order == BT_CTF_BYTE_ORDER_UNKNOWN) {
				_PERROR("%s", "invalid \"byte_order\" attribute in integer declaration");
				ret = -EINVAL;
				goto error;
			}

			_SET(&set, _INTEGER_BYTE_ORDER_SET);
		} else if (!strcmp(left->u.unary_expression.u.string, "size")) {
			if (_IS_SET(&set, _INTEGER_SIZE_SET)) {
				_PERROR_DUP_ATTR("size",
					"integer declaration");
				ret = -EPERM;
				goto error;
			}

			if (right->u.unary_expression.type !=
					UNARY_UNSIGNED_CONSTANT) {
				_PERROR("%s", "invalid \"size\" attribute in integer declaration: expecting unsigned constant");
				ret = -EINVAL;
				goto error;
			}

			size = right->u.unary_expression.u.unsigned_constant;
			if (size == 0) {
				_PERROR("%s", "invalid \"size\" attribute in integer declaration: expecting positive constant");
				ret = -EINVAL;
				goto error;
			} else if (size > 64) {
				_PERROR("%s", "invalid \"size\" attribute in integer declaration: integers over 64-bit are not supported as of this version");
				ret = -EINVAL;
				goto error;
			}

			_SET(&set, _INTEGER_SIZE_SET);
		} else if (!strcmp(left->u.unary_expression.u.string,
				"align")) {
			if (_IS_SET(&set, _INTEGER_ALIGN_SET)) {
				_PERROR_DUP_ATTR("align",
					"integer declaration");
				ret = -EPERM;
				goto error;
			}

			if (right->u.unary_expression.type !=
					UNARY_UNSIGNED_CONSTANT) {
				_PERROR("%s", "invalid \"align\" attribute in integer declaration: expecting unsigned constant");
				ret = -EINVAL;
				goto error;
			}

			alignment =
				right->u.unary_expression.u.unsigned_constant;
			if (!is_align_valid(alignment)) {
				_PERROR("%s", "invalid \"align\" attribute in integer declaration: expecting power of two");
				ret = -EINVAL;
				goto error;
			}

			_SET(&set, _INTEGER_ALIGN_SET);
		} else if (!strcmp(left->u.unary_expression.u.string, "base")) {
			if (_IS_SET(&set, _INTEGER_BASE_SET)) {
				_PERROR_DUP_ATTR("base", "integer declaration");
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
					base = BT_CTF_INTEGER_BASE_BINARY;
					break;
				case 8:
					base = BT_CTF_INTEGER_BASE_OCTAL;
					break;
				case 10:
					base = BT_CTF_INTEGER_BASE_DECIMAL;
					break;
				case 16:
					base = BT_CTF_INTEGER_BASE_HEXADECIMAL;
					break;
				default:
					_PERROR("invalid \"base\" attribute in integer declaration: %" PRIu64,
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
					_PERROR("%s", "unexpected unary expression for integer declaration's \"base\" attribute");
					ret = -EINVAL;
					goto error;
				}

				if (!strcmp(s_right, "decimal") ||
						!strcmp(s_right, "dec") ||
						!strcmp(s_right, "d") ||
						!strcmp(s_right, "i") ||
						!strcmp(s_right, "u")) {
					base = BT_CTF_INTEGER_BASE_DECIMAL;
				} else if (!strcmp(s_right, "hexadecimal") ||
						!strcmp(s_right, "hex") ||
						!strcmp(s_right, "x") ||
						!strcmp(s_right, "X") ||
						!strcmp(s_right, "p")) {
					base = BT_CTF_INTEGER_BASE_HEXADECIMAL;
				} else if (!strcmp(s_right, "octal") ||
						!strcmp(s_right, "oct") ||
						!strcmp(s_right, "o")) {
					base = BT_CTF_INTEGER_BASE_OCTAL;
				} else if (!strcmp(s_right, "binary") ||
						!strcmp(s_right, "b")) {
					base = BT_CTF_INTEGER_BASE_BINARY;
				} else {
					_PERROR("unexpected unary expression for integer declaration's \"base\" attribute: \"%s\"",
						s_right);
					g_free(s_right);
					ret = -EINVAL;
					goto error;
				}

				g_free(s_right);
				break;
			}
			default:
				_PERROR("%s", "invalid \"base\" attribute in integer declaration: expecting unsigned constant or unary string");
				ret = -EINVAL;
				goto error;
			}

			_SET(&set, _INTEGER_BASE_SET);
		} else if (!strcmp(left->u.unary_expression.u.string,
				"encoding")) {
			char *s_right;

			if (_IS_SET(&set, _INTEGER_ENCODING_SET)) {
				_PERROR_DUP_ATTR("encoding",
					"integer declaration");
				ret = -EPERM;
				goto error;
			}

			if (right->u.unary_expression.type != UNARY_STRING) {
				_PERROR("%s", "invalid \"encoding\" attribute in integer declaration: expecting unary string");
				ret = -EINVAL;
				goto error;
			}

			s_right = concatenate_unary_strings(
				&expression->u.ctf_expression.right);
			if (!s_right) {
				_PERROR("%s", "unexpected unary expression for integer declaration's \"encoding\" attribute");
				ret = -EINVAL;
				goto error;
			}

			if (!strcmp(s_right, "UTF8") ||
					!strcmp(s_right, "utf8") ||
					!strcmp(s_right, "utf-8") ||
					!strcmp(s_right, "UTF-8")) {
				encoding = BT_CTF_STRING_ENCODING_UTF8;
			} else if (!strcmp(s_right, "ASCII") ||
					!strcmp(s_right, "ascii")) {
				encoding = BT_CTF_STRING_ENCODING_ASCII;
			} else if (!strcmp(s_right, "none")) {
				encoding = BT_CTF_STRING_ENCODING_NONE;
			} else {
				_PERROR("invalid \"encoding\" attribute in integer declaration: unknown encoding \"%s\"",
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
				_PERROR_DUP_ATTR("map", "integer declaration");
				ret = -EPERM;
				goto error;
			}

			if (right->u.unary_expression.type != UNARY_STRING) {
				_PERROR("%s", "invalid \"map\" attribute in integer declaration: expecting unary string");
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
					_PERROR("%s", "unexpected unary expression for integer declaration's \"map\" attribute");
					ret = -EINVAL;
					goto error;
				}

				_PWARNING("invalid \"map\" attribute in integer declaration: unknown clock class: \"%s\"",
					s_right);
				_SET(&set, _INTEGER_MAP_SET);
				g_free(s_right);
				continue;
			}

			mapped_clock = bt_ctf_trace_get_clock_class_by_name(
				ctx->trace, clock_name);
			if (!mapped_clock) {
				_PERROR("invalid \"map\" attribute in integer declaration: cannot find clock class \"%s\"",
					clock_name);
				ret = -EINVAL;
				goto error;
			}

			_SET(&set, _INTEGER_MAP_SET);
		} else {
			_PWARNING("unknown attribute \"%s\" in integer declaration",
				left->u.unary_expression.u.string);
		}
	}

	if (!_IS_SET(&set, _INTEGER_SIZE_SET)) {
		_PERROR("%s",
			"missing \"size\" attribute in integer declaration");
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

	*integer_decl = bt_ctf_field_type_integer_create((unsigned int) size);
	if (!*integer_decl) {
		_PERROR("%s", "cannot create integer declaration");
		ret = -ENOMEM;
		goto error;
	}

	ret = bt_ctf_field_type_integer_set_signed(*integer_decl, signedness);
	ret |= bt_ctf_field_type_integer_set_base(*integer_decl, base);
	ret |= bt_ctf_field_type_integer_set_encoding(*integer_decl, encoding);
	ret |= bt_ctf_field_type_set_alignment(*integer_decl,
		(unsigned int) alignment);
	ret |= bt_ctf_field_type_set_byte_order(*integer_decl, byte_order);

	if (mapped_clock) {
		/* Move clock */
		ret |= bt_ctf_field_type_integer_set_mapped_clock_class(
			*integer_decl, mapped_clock);
		bt_put(mapped_clock);
		mapped_clock = NULL;
	}

	if (ret) {
		_PERROR("%s", "cannot configure integer declaration");
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
	struct bt_ctf_field_type **float_decl)
{
	int set = 0;
	int ret = 0;
	struct ctf_node *expression;
	uint64_t alignment = 1, exp_dig = 0, mant_dig = 0;
	enum bt_ctf_byte_order byte_order =
		bt_ctf_trace_get_byte_order(ctx->trace);

	*float_decl = NULL;

	bt_list_for_each_entry(expression, expressions, siblings) {
		struct ctf_node *left, *right;

		left = _BT_LIST_FIRST_ENTRY(&expression->u.ctf_expression.left,
			struct ctf_node, siblings);
		right = _BT_LIST_FIRST_ENTRY(
			&expression->u.ctf_expression.right, struct ctf_node,
			siblings);

		if (left->u.unary_expression.type != UNARY_STRING) {
			ret = -EINVAL;
			goto error;
		}

		if (!strcmp(left->u.unary_expression.u.string, "byte_order")) {
			if (_IS_SET(&set, _FLOAT_BYTE_ORDER_SET)) {
				_PERROR_DUP_ATTR("byte_order",
					"floating point number declaration");
				ret = -EPERM;
				goto error;
			}

			byte_order = get_real_byte_order(ctx, right);
			if (byte_order == BT_CTF_BYTE_ORDER_UNKNOWN) {
				_PERROR("%s", "invalid \"byte_order\" attribute in floating point number declaration");
				ret = -EINVAL;
				goto error;
			}

			_SET(&set, _FLOAT_BYTE_ORDER_SET);
		} else if (!strcmp(left->u.unary_expression.u.string,
				"exp_dig")) {
			if (_IS_SET(&set, _FLOAT_EXP_DIG_SET)) {
				_PERROR_DUP_ATTR("exp_dig",
					"floating point number declaration");
				ret = -EPERM;
				goto error;
			}

			if (right->u.unary_expression.type !=
					UNARY_UNSIGNED_CONSTANT) {
				_PERROR("%s", "invalid \"exp_dig\" attribute in floating point number declaration: expecting unsigned constant");
				ret = -EINVAL;
				goto error;
			}

			exp_dig = right->u.unary_expression.u.unsigned_constant;
			_SET(&set, _FLOAT_EXP_DIG_SET);
		} else if (!strcmp(left->u.unary_expression.u.string,
				"mant_dig")) {
			if (_IS_SET(&set, _FLOAT_MANT_DIG_SET)) {
				_PERROR_DUP_ATTR("mant_dig",
					"floating point number declaration");
				ret = -EPERM;
				goto error;
			}

			if (right->u.unary_expression.type !=
					UNARY_UNSIGNED_CONSTANT) {
				_PERROR("%s", "invalid \"mant_dig\" attribute in floating point number declaration: expecting unsigned constant");
				ret = -EINVAL;
				goto error;
			}

			mant_dig = right->u.unary_expression.u.
				unsigned_constant;
			_SET(&set, _FLOAT_MANT_DIG_SET);
		} else if (!strcmp(left->u.unary_expression.u.string,
				"align")) {
			if (_IS_SET(&set, _FLOAT_ALIGN_SET)) {
				_PERROR_DUP_ATTR("align",
					"floating point number declaration");
				ret = -EPERM;
				goto error;
			}

			if (right->u.unary_expression.type !=
					UNARY_UNSIGNED_CONSTANT) {
				_PERROR("%s", "invalid \"align\" attribute in floating point number declaration: expecting unsigned constant");
				ret = -EINVAL;
				goto error;
			}

			alignment = right->u.unary_expression.u.
				unsigned_constant;

			if (!is_align_valid(alignment)) {
				_PERROR("%s", "invalid \"align\" attribute in floating point number declaration: expecting power of two");
				ret = -EINVAL;
				goto error;
			}

			_SET(&set, _FLOAT_ALIGN_SET);
		} else {
			_PWARNING("unknown attribute \"%s\" in floating point number declaration",
				left->u.unary_expression.u.string);
		}
	}

	if (!_IS_SET(&set, _FLOAT_MANT_DIG_SET)) {
		_PERROR("%s", "missing \"mant_dig\" attribute in floating point number declaration");
		ret = -EPERM;
		goto error;
	}

	if (!_IS_SET(&set, _FLOAT_EXP_DIG_SET)) {
		_PERROR("%s", "missing \"exp_dig\" attribute in floating point number declaration");
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

	*float_decl = bt_ctf_field_type_floating_point_create();
	if (!*float_decl) {
		_PERROR("%s",
			"cannot create floating point number declaration");
		ret = -ENOMEM;
		goto error;
	}

	ret = bt_ctf_field_type_floating_point_set_exponent_digits(
		*float_decl, exp_dig);
	ret |= bt_ctf_field_type_floating_point_set_mantissa_digits(
		*float_decl, mant_dig);
	ret |= bt_ctf_field_type_set_byte_order(*float_decl, byte_order);
	ret |= bt_ctf_field_type_set_alignment(*float_decl, alignment);
	if (ret) {
		_PERROR("%s",
			"cannot configure floating point number declaration");
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
	struct bt_ctf_field_type **string_decl)
{
	int set = 0;
	int ret = 0;
	struct ctf_node *expression;
	enum bt_ctf_string_encoding encoding = BT_CTF_STRING_ENCODING_UTF8;

	*string_decl = NULL;

	bt_list_for_each_entry(expression, expressions, siblings) {
		struct ctf_node *left, *right;

		left = _BT_LIST_FIRST_ENTRY(&expression->u.ctf_expression.left,
			struct ctf_node, siblings);
		right = _BT_LIST_FIRST_ENTRY(
			&expression->u.ctf_expression.right, struct ctf_node,
			siblings);

		if (left->u.unary_expression.type != UNARY_STRING) {
			ret = -EINVAL;
			goto error;
		}

		if (!strcmp(left->u.unary_expression.u.string, "encoding")) {
			char *s_right;

			if (_IS_SET(&set, _STRING_ENCODING_SET)) {
				_PERROR_DUP_ATTR("encoding",
					"string declaration");
				ret = -EPERM;
				goto error;
			}

			if (right->u.unary_expression.type != UNARY_STRING) {
				_PERROR("%s", "invalid \"encoding\" attribute in string declaration: expecting unary string");
				ret = -EINVAL;
				goto error;
			}

			s_right = concatenate_unary_strings(
				&expression->u.ctf_expression.right);
			if (!s_right) {
				_PERROR("%s", "unexpected unary expression for string declaration's \"encoding\" attribute");
				ret = -EINVAL;
				goto error;
			}

			if (!strcmp(s_right, "UTF8") ||
					!strcmp(s_right, "utf8") ||
					!strcmp(s_right, "utf-8") ||
					!strcmp(s_right, "UTF-8")) {
				encoding = BT_CTF_STRING_ENCODING_UTF8;
			} else if (!strcmp(s_right, "ASCII") ||
					!strcmp(s_right, "ascii")) {
				encoding = BT_CTF_STRING_ENCODING_ASCII;
			} else if (!strcmp(s_right, "none")) {
				encoding = BT_CTF_STRING_ENCODING_NONE;
			} else {
				_PERROR("invalid \"encoding\" attribute in string declaration: unknown encoding \"%s\"",
					s_right);
				g_free(s_right);
				ret = -EINVAL;
				goto error;
			}

			g_free(s_right);
			_SET(&set, _STRING_ENCODING_SET);
		} else {
			_PWARNING("unknown attribute \"%s\" in string declaration",
				left->u.unary_expression.u.string);
		}
	}

	*string_decl = bt_ctf_field_type_string_create();
	if (!*string_decl) {
		_PERROR("%s", "cannot create string declaration");
		ret = -ENOMEM;
		goto error;
	}

	ret = bt_ctf_field_type_string_set_encoding(*string_decl, encoding);
	if (ret) {
		_PERROR("%s", "cannot configure string declaration");
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
	struct bt_ctf_field_type **decl)
{
	int ret = 0;
	struct ctf_node *first, *node;

	*decl = NULL;

	if (ts_list->type != NODE_TYPE_SPECIFIER_LIST) {
		ret = -EINVAL;
		goto error;
	}

	first = _BT_LIST_FIRST_ENTRY(&ts_list->u.type_specifier_list.head,
		struct ctf_node, siblings);
	if (first->type != NODE_TYPE_SPECIFIER) {
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
			assert(!*decl);
			goto error;
		}
		break;
	default:
		_PERROR("unexpected node type: %d",
			(int) first->u.type_specifier.type);
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
	struct bt_ctf_event_class *event_class, int64_t *stream_id,
	int *set)
{
	int ret = 0;
	char *left = NULL;
	_BT_CTF_FIELD_TYPE_INIT(decl);

	switch (node->type) {
	case NODE_TYPEDEF:
		ret = visit_typedef(ctx, node->u._typedef.type_specifier_list,
			&node->u._typedef.type_declarators);
		if (ret) {
			_PERROR("%s",
				"cannot add typedef in \"event\" declaration");
			goto error;
		}
		break;
	case NODE_TYPEALIAS:
		ret = visit_typealias(ctx, node->u.typealias.target,
			node->u.typealias.alias);
		if (ret) {
			_PERROR("%s", "cannot add typealias in \"event\" declaration");
			goto error;
		}
		break;
	case NODE_CTF_EXPRESSION:
	{
		left = concatenate_unary_strings(&node->u.ctf_expression.left);
		if (!left) {
			ret = -EINVAL;
			goto error;
		}

		if (!strcmp(left, "name")) {
			/* This is already known at this stage */
			if (_IS_SET(set, _EVENT_NAME_SET)) {
				_PERROR_DUP_ATTR("name", "event declaration");
				ret = -EPERM;
				goto error;
			}

			_SET(set, _EVENT_NAME_SET);
		} else if (!strcmp(left, "id")) {
			int64_t id;

			if (_IS_SET(set, _EVENT_ID_SET)) {
				_PERROR_DUP_ATTR("id", "event declaration");
				ret = -EPERM;
				goto error;
			}

			ret = get_unary_unsigned(&node->u.ctf_expression.right,
				(uint64_t *) &id);
			if (ret || id < 0) {
				_PERROR("%s", "unexpected unary expression for event declaration's \"id\" attribute");
				ret = -EINVAL;
				goto error;
			}

			ret = bt_ctf_event_class_set_id(event_class, id);
			if (ret) {
				_PERROR("%s",
					"cannot set event declaration's ID");
				goto error;
			}

			_SET(set, _EVENT_ID_SET);
		} else if (!strcmp(left, "stream_id")) {
			if (_IS_SET(set, _EVENT_STREAM_ID_SET)) {
				_PERROR_DUP_ATTR("stream_id",
					"event declaration");
				ret = -EPERM;
				goto error;
			}

			ret = get_unary_unsigned(&node->u.ctf_expression.right,
				(uint64_t *) stream_id);
			if (ret || *stream_id < 0) {
				_PERROR("%s", "unexpected unary expression for event declaration's \"stream_id\" attribute");
				ret = -EINVAL;
				goto error;
			}

			_SET(set, _EVENT_STREAM_ID_SET);
		} else if (!strcmp(left, "context")) {
			if (_IS_SET(set, _EVENT_CONTEXT_SET)) {
				_PERROR("%s", "duplicate \"context\" entry in event declaration");
				ret = -EPERM;
				goto error;
			}

			ret = visit_type_specifier_list(ctx,
				_BT_LIST_FIRST_ENTRY(
					&node->u.ctf_expression.right,
					struct ctf_node, siblings),
				&decl);
			if (ret) {
				_PERROR("%s", "cannot create event context declaration");
				goto error;
			}

			assert(decl);
			ret = bt_ctf_event_class_set_context_type(
				event_class, decl);
			BT_PUT(decl);
			if (ret) {
				_PERROR("%s", "cannot set event's context declaration");
				goto error;
			}

			_SET(set, _EVENT_CONTEXT_SET);
		} else if (!strcmp(left, "fields")) {
			if (_IS_SET(set, _EVENT_FIELDS_SET)) {
				_PERROR("%s", "duplicate \"fields\" entry in event declaration");
				ret = -EPERM;
				goto error;
			}

			ret = visit_type_specifier_list(ctx,
				_BT_LIST_FIRST_ENTRY(
					&node->u.ctf_expression.right,
					struct ctf_node, siblings),
				&decl);
			if (ret) {
				_PERROR("%s", "cannot create event payload declaration");
				goto error;
			}

			assert(decl);
			ret = bt_ctf_event_class_set_payload_type(
				event_class, decl);
			BT_PUT(decl);
			if (ret) {
				_PERROR("%s", "cannot set event's payload declaration");
				goto error;
			}

			_SET(set, _EVENT_FIELDS_SET);
		} else if (!strcmp(left, "loglevel")) {
			uint64_t loglevel_value;
			const char *loglevel_str;
			struct bt_value *value_obj, *str_obj;

			if (_IS_SET(set, _EVENT_LOGLEVEL_SET)) {
				_PERROR_DUP_ATTR("loglevel",
					"event declaration");
				ret = -EPERM;
				goto error;
			}

			ret = get_unary_unsigned(&node->u.ctf_expression.right,
				&loglevel_value);
			if (ret) {
				_PERROR("%s", "unexpected unary expression for event declaration's \"loglevel\" attribute");
				ret = -EINVAL;
				goto error;
			}
			value_obj = bt_value_integer_create_init(loglevel_value);
			if (!value_obj) {
				_PERROR("%s", "cannot allocate memory for loglevel value object");
				ret = -ENOMEM;
				goto error;
			}
			if (bt_ctf_event_class_set_attribute(event_class,
				"loglevel", value_obj) != BT_VALUE_STATUS_OK) {
				_PERROR("%s", "cannot set loglevel value");
				ret = -EINVAL;
				bt_put(value_obj);
				goto error;
			}
			loglevel_str = print_loglevel(loglevel_value);
			if (loglevel_str) {
				str_obj = bt_value_string_create_init(loglevel_str);
				if (bt_ctf_event_class_set_attribute(event_class,
						"loglevel_string", str_obj) != BT_VALUE_STATUS_OK) {
					_PERROR("%s", "cannot set loglevel string");
					ret = -EINVAL;
					bt_put(str_obj);
					goto error;
				}
				bt_put(str_obj);
			}
			bt_put(value_obj);
			_SET(set, _EVENT_LOGLEVEL_SET);
		} else if (!strcmp(left, "model.emf.uri")) {
			char *right;

			if (_IS_SET(set, _EVENT_MODEL_EMF_URI_SET)) {
				_PERROR_DUP_ATTR("model.emf.uri",
					"event declaration");
				ret = -EPERM;
				goto error;
			}

			right = concatenate_unary_strings(
				&node->u.ctf_expression.right);
			if (!right) {
				_PERROR("%s", "unexpected unary expression for event declaration's \"model.emf.uri\" attribute");
				ret = -EINVAL;
				goto error;
			}

			// TODO: FIXME: set model EMF URI here

			g_free(right);
			_SET(set, _EVENT_MODEL_EMF_URI_SET);
		} else {
			_PWARNING("unknown attribute \"%s\" in event declaration",
				left);
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
			goto error;
		}

		if (!strcmp(left, "name")) {
			name = concatenate_unary_strings(
				&iter->u.ctf_expression.right);
			if (!name) {
				_PERROR("%s", "unexpected unary expression for event declaration's \"name\" attribute");
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
	struct bt_ctf_event_class *event_class)
{
	int ret = 0;

	/* Context type. */
	ret = bt_ctf_event_class_set_context_type(event_class, NULL);
	if (ret) {
		_PERROR("%s", "cannot set initial NULL event context");
		goto end;
	}

	/* Event payload. */
	ret = bt_ctf_event_class_set_payload_type(event_class, NULL);
	if (ret) {
		_PERROR("%s", "cannot set initial NULL event payload");
		goto end;
	}
end:
	return ret;
}

static
int reset_stream_decl_types(struct ctx *ctx,
	struct bt_ctf_stream_class *stream_class)
{
	int ret = 0;

	/* Packet context. */
	ret = bt_ctf_stream_class_set_packet_context_type(stream_class, NULL);
	if (ret) {
		_PERROR("%s", "cannot set initial empty packet context");
		goto end;
	}

	/* Event header. */
	ret = bt_ctf_stream_class_set_event_header_type(stream_class, NULL);
	if (ret) {
		_PERROR("%s", "cannot set initial empty event header");
		goto end;
	}

	/* Event context. */
	ret = bt_ctf_stream_class_set_event_context_type(stream_class, NULL);
	if (ret) {
		_PERROR("%s", "cannot set initial empty stream event context");
		goto end;
	}
end:
	return ret;
}

static
struct bt_ctf_stream_class *create_reset_stream_class(struct ctx *ctx)
{
	int ret;
	struct bt_ctf_stream_class *stream_class;

	stream_class = bt_ctf_stream_class_create(NULL);
	if (!stream_class) {
		_PERROR("%s", "cannot create stream class");
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
	struct bt_ctf_event_class *event_class = NULL;
	struct bt_ctf_event_class *eevent_class;
	struct bt_ctf_stream_class *stream_class = NULL;
	struct bt_list_head *decl_list = &node->u.event.declaration_list;
	bool pop_scope = false;

	if (node->visited) {
		goto end;
	}

	node->visited = TRUE;
	event_name = get_event_decl_name(ctx, node);
	if (!event_name) {
		_PERROR("%s",
			"missing \"name\" attribute in event declaration");
		ret = -EPERM;
		goto error;
	}

	event_class = bt_ctf_event_class_create(event_name);

	/*
	 * Unset context and fields to override the default ones.
	 */
	ret = reset_event_decl_types(ctx, event_class);
	if (ret) {
		goto error;
	}

	ret = ctx_push_scope(ctx);
	if (ret) {
		_PERROR("%s", "cannot push scope");
		goto error;
	}

	pop_scope = true;

	bt_list_for_each_entry(iter, decl_list, siblings) {
		ret = visit_event_decl_entry(ctx, iter, event_class,
			&stream_id, &set);
		if (ret) {
			goto error;
		}
	}

	if (!_IS_SET(&set, _EVENT_STREAM_ID_SET)) {
		GList *keys = NULL;
		struct bt_ctf_stream_class *new_stream_class;
		size_t stream_class_count =
			g_hash_table_size(ctx->stream_classes) +
			bt_ctf_trace_get_stream_class_count(ctx->trace);

		/*
		 * Allow missing stream_id if there is only a single
		 * stream class.
		 */
		switch (stream_class_count) {
		case 0:
			/* Create implicit stream class if there's none */
			new_stream_class = create_reset_stream_class(ctx);
			if (!new_stream_class) {
				ret = -EINVAL;
				goto error;
			}

			ret = bt_ctf_stream_class_set_id(new_stream_class, 0);
			if (ret) {
				_PERROR("%s", "cannot set stream class's ID");
				BT_PUT(new_stream_class);
				goto error;
			}

			stream_id = 0;

			/* Move reference to visitor's context */
			g_hash_table_insert(ctx->stream_classes,
				(gpointer) stream_id, new_stream_class);
			new_stream_class = NULL;
			break;
		case 1:
			/* Single stream class: get its ID */
			if (g_hash_table_size(ctx->stream_classes) == 1) {
				keys = g_hash_table_get_keys(ctx->stream_classes);
				stream_id = (int64_t) keys->data;
				g_list_free(keys);
			} else {
				assert(bt_ctf_trace_get_stream_class_count(
					ctx->trace) == 1);
				stream_class = bt_ctf_trace_get_stream_class(
					ctx->trace, 0);
				assert(stream_class);
				stream_id = bt_ctf_stream_class_get_id(
					stream_class);
				BT_PUT(stream_class);
			}
			break;
		default:
			_PERROR("%s", "missing \"stream_id\" attribute in event declaration");
			ret = -EPERM;
			goto error;
		}
	}

	assert(stream_id >= 0);

	/* We have the stream ID now; get the stream class if found */
	stream_class = g_hash_table_lookup(ctx->stream_classes,
		(gpointer) stream_id);
	bt_get(stream_class);
	if (!stream_class) {
		stream_class = bt_ctf_trace_get_stream_class_by_id(ctx->trace,
			stream_id);
		if (!stream_class) {
			_PERROR("cannot find stream class with ID %" PRId64,
				stream_id);
			ret = -EINVAL;
			goto error;
		}
	}

	assert(stream_class);

	if (!_IS_SET(&set, _EVENT_ID_SET)) {
		/* Allow only one event without ID per stream */
		if (bt_ctf_stream_class_get_event_class_count(stream_class) !=
				0) {
			_PERROR("%s",
				"missing \"id\" field in event declaration");
			ret = -EPERM;
			goto error;
		}

		/* Automatic ID */
		ret = bt_ctf_event_class_set_id(event_class, 0);
		if (ret) {
			_PERROR("%s", "cannot set event's ID");
			goto error;
		}
	}

	event_id = bt_ctf_event_class_get_id(event_class);
	if (event_id < 0) {
		_PERROR("%s", "cannot get event's ID");
		ret = -EINVAL;
		goto error;
	}

	eevent_class = bt_ctf_stream_class_get_event_class_by_id(stream_class,
		event_id);
	if (eevent_class) {
		BT_PUT(eevent_class);
		_PERROR("%s", "duplicate event with ID %" PRId64 " in same stream");
		ret = -EEXIST;
		goto error;
	}

	eevent_class = bt_ctf_stream_class_get_event_class_by_name(stream_class,
		event_name);
	if (eevent_class) {
		BT_PUT(eevent_class);
		_PERROR("%s",
			"duplicate event with name \"%s\" in same stream");
		ret = -EEXIST;
		goto error;
	}

	ret = bt_ctf_stream_class_add_event_class(stream_class, event_class);
	BT_PUT(event_class);
	if (ret) {
		_PERROR("%s", "cannot add event class to stream class");
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
int visit_stream_decl_entry(struct ctx *ctx, struct ctf_node *node,
	struct bt_ctf_stream_class *stream_class, int *set)
{
	int ret = 0;
	char *left = NULL;
	_BT_CTF_FIELD_TYPE_INIT(decl);

	switch (node->type) {
	case NODE_TYPEDEF:
		ret = visit_typedef(ctx, node->u._typedef.type_specifier_list,
			&node->u._typedef.type_declarators);
		if (ret) {
			_PERROR("%s",
				"cannot add typedef in \"stream\" declaration");
			goto error;
		}
		break;
	case NODE_TYPEALIAS:
		ret = visit_typealias(ctx, node->u.typealias.target,
			node->u.typealias.alias);
		if (ret) {
			_PERROR("%s", "cannot add typealias in \"stream\" declaration");
			goto error;
		}
		break;
	case NODE_CTF_EXPRESSION:
	{
		left = concatenate_unary_strings(&node->u.ctf_expression.left);
		if (!left) {
			ret = -EINVAL;
			goto error;
		}

		if (!strcmp(left, "id")) {
			int64_t id;
			gpointer ptr;

			if (_IS_SET(set, _STREAM_ID_SET)) {
				_PERROR_DUP_ATTR("id", "stream declaration");
				ret = -EPERM;
				goto error;
			}

			ret = get_unary_unsigned(&node->u.ctf_expression.right,
				(uint64_t *) &id);
			if (ret || id < 0) {
				_PERROR("%s", "unexpected unary expression for stream declaration's \"id\" attribute");
				ret = -EINVAL;
				goto error;
			}

			ptr = g_hash_table_lookup(ctx->stream_classes,
				(gpointer) id);
			if (ptr) {
				_PERROR("duplicate stream with ID %" PRId64,
					id);
				ret = -EEXIST;
				goto error;
			}

			ret = bt_ctf_stream_class_set_id(stream_class, id);
			if (ret) {
				_PERROR("%s",
					"cannot set stream declaration's ID");
				goto error;
			}

			_SET(set, _STREAM_ID_SET);
		} else if (!strcmp(left, "event.header")) {
			if (_IS_SET(set, _STREAM_EVENT_HEADER_SET)) {
				_PERROR("%s", "duplicate \"event.header\" entry in stream declaration");
				ret = -EPERM;
				goto error;
			}

			ret = visit_type_specifier_list(ctx,
				_BT_LIST_FIRST_ENTRY(
					&node->u.ctf_expression.right,
					struct ctf_node, siblings),
				&decl);
			if (ret) {
				_PERROR("%s", "cannot create event header declaration");
				goto error;
			}

			assert(decl);

			ret = bt_ctf_stream_class_set_event_header_type(
				stream_class, decl);
			BT_PUT(decl);
			if (ret) {
				_PERROR("%s", "cannot set stream's event header declaration");
				goto error;
			}

			_SET(set, _STREAM_EVENT_HEADER_SET);
		} else if (!strcmp(left, "event.context")) {
			if (_IS_SET(set, _STREAM_EVENT_CONTEXT_SET)) {
				_PERROR("%s", "duplicate \"event.context\" entry in stream declaration");
				ret = -EPERM;
				goto error;
			}

			ret = visit_type_specifier_list(ctx,
				_BT_LIST_FIRST_ENTRY(
					&node->u.ctf_expression.right,
					struct ctf_node, siblings),
				&decl);
			if (ret) {
				_PERROR("%s", "cannot create stream event context declaration");
				goto error;
			}

			assert(decl);

			ret = bt_ctf_stream_class_set_event_context_type(
				stream_class, decl);
			BT_PUT(decl);
			if (ret) {
				_PERROR("%s", "cannot set stream's event context declaration");
				goto error;
			}

			_SET(set, _STREAM_EVENT_CONTEXT_SET);
		} else if (!strcmp(left, "packet.context")) {
			if (_IS_SET(set, _STREAM_PACKET_CONTEXT_SET)) {
				_PERROR("%s", "duplicate \"packet.context\" entry in stream declaration");
				ret = -EPERM;
				goto error;
			}

			ret = visit_type_specifier_list(ctx,
				_BT_LIST_FIRST_ENTRY(
					&node->u.ctf_expression.right,
					struct ctf_node, siblings),
				&decl);
			if (ret) {
				_PERROR("%s", "cannot create packet context declaration");
				goto error;
			}

			assert(decl);

			ret = bt_ctf_stream_class_set_packet_context_type(
				stream_class, decl);
			BT_PUT(decl);
			if (ret) {
				_PERROR("%s", "cannot set stream's packet context declaration");
				goto error;
			}

			_SET(set, _STREAM_PACKET_CONTEXT_SET);
		} else {
			_PWARNING("unknown attribute \"%s\" in stream declaration",
				left);
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
	int set = 0;
	int ret = 0;
	struct ctf_node *iter;
	struct bt_ctf_stream_class *stream_class = NULL;
	struct bt_ctf_stream_class *existing_stream_class = NULL;
	struct bt_list_head *decl_list = &node->u.stream.declaration_list;

	if (node->visited) {
		goto end;
	}

	node->visited = TRUE;
	stream_class = create_reset_stream_class(ctx);
	if (!stream_class) {
		ret = -EINVAL;
		goto error;
	}

	ret = ctx_push_scope(ctx);
	if (ret) {
		_PERROR("%s", "cannot push scope");
		goto error;
	}

	bt_list_for_each_entry(iter, decl_list, siblings) {
		ret = visit_stream_decl_entry(ctx, iter, stream_class, &set);
		if (ret) {
			ctx_pop_scope(ctx);
			goto error;
		}
	}

	ctx_pop_scope(ctx);

	if (_IS_SET(&set, _STREAM_ID_SET)) {
		/* Check that packet header has stream_id field */
		_BT_CTF_FIELD_TYPE_INIT(stream_id_decl);
		_BT_CTF_FIELD_TYPE_INIT(packet_header_decl);

		packet_header_decl =
			bt_ctf_trace_get_packet_header_type(ctx->trace);
		if (!packet_header_decl) {
			_PERROR("%s",
				"cannot get trace packet header declaration");
			goto error;
		}

		stream_id_decl =
			bt_ctf_field_type_structure_get_field_type_by_name(
				packet_header_decl, "stream_id");
		BT_PUT(packet_header_decl);
		if (!stream_id_decl) {
			_PERROR("%s", "missing \"stream_id\" field in packet header declaration, but \"id\" attribute is declared for stream");
			goto error;
		}

		if (!bt_ctf_field_type_is_integer(stream_id_decl)) {
			BT_PUT(stream_id_decl);
			_PERROR("%s", "\"stream_id\" field in packet header declaration is not an integer");
			goto error;
		}

		BT_PUT(stream_id_decl);
	} else {
		/* Allow only _one_ ID-less stream */
		if (g_hash_table_size(ctx->stream_classes) != 0) {
			_PERROR("%s",
				"missing \"id\" field in stream declaration");
			ret = -EPERM;
			goto error;
		}

		/* Automatic ID: 0 */
		ret = bt_ctf_stream_class_set_id(stream_class, 0);
	}

	id = bt_ctf_stream_class_get_id(stream_class);
	if (id < 0) {
		_PERROR("wrong stream ID: %" PRId64, id);
		ret = -EINVAL;
		goto error;
	}

	/*
	 * Make sure that this stream class's ID is currently unique in
	 * the trace.
	 */
	if (g_hash_table_lookup(ctx->stream_classes, (gpointer) id)) {
		_PERROR("a stream class with ID: %" PRId64 " already exists in the trace", id);
		ret = -EINVAL;
		goto error;
	}

	existing_stream_class = bt_ctf_trace_get_stream_class_by_id(ctx->trace,
		id);
	if (existing_stream_class) {
		_PERROR("a stream class with ID: %" PRId64 " already exists in the trace", id);
		ret = -EINVAL;
		goto error;
	}

	/* Move reference to visitor's context */
	g_hash_table_insert(ctx->stream_classes, (gpointer) (int64_t) id,
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
	_BT_CTF_FIELD_TYPE_INIT(packet_header_decl);

	switch (node->type) {
	case NODE_TYPEDEF:
		ret = visit_typedef(ctx, node->u._typedef.type_specifier_list,
			&node->u._typedef.type_declarators);
		if (ret) {
			_PERROR("%s",
				"cannot add typedef in \"trace\" declaration");
			goto error;
		}
		break;
	case NODE_TYPEALIAS:
		ret = visit_typealias(ctx, node->u.typealias.target,
			node->u.typealias.alias);
		if (ret) {
			_PERROR("%s",
				"cannot add typealias in \"trace\" declaration");
			goto error;
		}
		break;
	case NODE_CTF_EXPRESSION:
	{
		left = concatenate_unary_strings(&node->u.ctf_expression.left);
		if (!left) {
			ret = -EINVAL;
			goto error;
		}

		if (!strcmp(left, "major")) {
			if (_IS_SET(set, _TRACE_MAJOR_SET)) {
				_PERROR_DUP_ATTR("major", "trace declaration");
				ret = -EPERM;
				goto error;
			}

			ret = get_unary_unsigned(&node->u.ctf_expression.right,
				&ctx->trace_major);
			if (ret) {
				_PERROR("%s", "unexpected unary expression for trace's \"major\" attribute");
				ret = -EINVAL;
				goto error;
			}

			_SET(set, _TRACE_MAJOR_SET);
		} else if (!strcmp(left, "minor")) {
			if (_IS_SET(set, _TRACE_MINOR_SET)) {
				_PERROR_DUP_ATTR("minor", "trace declaration");
				ret = -EPERM;
				goto error;
			}

			ret = get_unary_unsigned(&node->u.ctf_expression.right,
				&ctx->trace_minor);
			if (ret) {
				_PERROR("%s", "unexpected unary expression for trace's \"minor\" attribute");
				ret = -EINVAL;
				goto error;
			}

			_SET(set, _TRACE_MINOR_SET);
		} else if (!strcmp(left, "uuid")) {
			if (_IS_SET(set, _TRACE_UUID_SET)) {
				_PERROR_DUP_ATTR("uuid", "trace declaration");
				ret = -EPERM;
				goto error;
			}

			ret = get_unary_uuid(&node->u.ctf_expression.right,
				ctx->trace_uuid);
			if (ret) {
				_PERROR("%s",
					"invalid trace declaration's UUID");
				goto error;
			}

			_SET(set, _TRACE_UUID_SET);
		} else if (!strcmp(left, "byte_order")) {
			/* Native byte order is already known at this stage */
			if (_IS_SET(set, _TRACE_BYTE_ORDER_SET)) {
				_PERROR_DUP_ATTR("byte_order",
					"trace declaration");
				ret = -EPERM;
				goto error;
			}

			_SET(set, _TRACE_BYTE_ORDER_SET);
		} else if (!strcmp(left, "packet.header")) {
			if (_IS_SET(set, _TRACE_PACKET_HEADER_SET)) {
				_PERROR("%s", "duplicate \"packet.header\" entry in trace declaration");
				ret = -EPERM;
				goto error;
			}

			ret = visit_type_specifier_list(ctx,
				_BT_LIST_FIRST_ENTRY(
					&node->u.ctf_expression.right,
					struct ctf_node, siblings),
				&packet_header_decl);
			if (ret) {
				_PERROR("%s", "cannot create packet header declaration");
				goto error;
			}

			assert(packet_header_decl);
			ret = bt_ctf_trace_set_packet_header_type(ctx->trace,
				packet_header_decl);
			BT_PUT(packet_header_decl);
			if (ret) {
				_PERROR("%s", "cannot set trace declaration's packet header declaration");
				goto error;
			}

			_SET(set, _TRACE_PACKET_HEADER_SET);
		} else {
			_PWARNING("%s", "unknown attribute \"%s\" in trace declaration");
		}

		g_free(left);
		left = NULL;
		break;
	}
	default:
		_PERROR("%s", "unknown expression in trace declaration");
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
		_PERROR("%s", "duplicate \"trace\" block");
		ret = -EEXIST;
		goto error;
	}

	ret = ctx_push_scope(ctx);
	if (ret) {
		_PERROR("%s", "cannot push scope");
		goto error;
	}

	bt_list_for_each_entry(iter, decl_list, siblings) {
		ret = visit_trace_decl_entry(ctx, iter, &set);
		if (ret) {
			ctx_pop_scope(ctx);
			goto error;
		}
	}

	ctx_pop_scope(ctx);

	if (!_IS_SET(&set, _TRACE_MAJOR_SET)) {
		_PERROR("%s",
			"missing \"major\" attribute in trace declaration");
		ret = -EPERM;
		goto error;
	}

	if (!_IS_SET(&set, _TRACE_MINOR_SET)) {
		_PERROR("%s",
			"missing \"minor\" attribute in trace declaration");
		ret = -EPERM;
		goto error;
	}

	if (!_IS_SET(&set, _TRACE_BYTE_ORDER_SET)) {
		_PERROR("%s", "missing \"byte_order\" attribute in trace declaration");
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
			_PERROR("%s", "wrong expression in environment entry");
			ret = -EPERM;
			goto error;
		}

		left = concatenate_unary_strings(
			&entry_node->u.ctf_expression.left);
		if (!left) {
			_PERROR("%s", "cannot get environment entry name");
			ret = -EINVAL;
			goto error;
		}

		if (is_unary_string(right_head)) {
			char *right = concatenate_unary_strings(right_head);

			if (!right) {
				_PERROR("unexpected unary expression for environment entry's value (\"%s\")",
					left);
				ret = -EINVAL;
				goto error;
			}

			printf_verbose("env.%s = \"%s\"\n", left, right);
			ret = bt_ctf_trace_set_environment_field_string(
				ctx->trace, left, right);
			g_free(right);

			if (ret) {
				_PERROR("environment: cannot add entry \"%s\" to trace",
					left);
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
				_PERROR("unexpected unary expression for environment entry's value (\"%s\")",
					left);
				ret = -EINVAL;
				goto error;
			}

			printf_verbose("env.%s = %" PRId64 "\n", left, v);
			ret = bt_ctf_trace_set_environment_field_integer(
				ctx->trace, left, v);
			if (ret) {
				_PERROR("environment: cannot add entry \"%s\" to trace",
					left);
				goto error;
			}
		} else {
			printf_verbose("%s: environment entry \"%s\" has unknown type\n",
				__func__, left);
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
				ret = -EINVAL;
				goto error;
			}

			if (!strcmp(left, "byte_order")) {
				enum bt_ctf_byte_order bo;

				if (_IS_SET(&set, _TRACE_BYTE_ORDER_SET)) {
					_PERROR_DUP_ATTR("byte_order",
						"trace declaration");
					ret = -EPERM;
					goto error;
				}

				_SET(&set, _TRACE_BYTE_ORDER_SET);
				right_node = _BT_LIST_FIRST_ENTRY(
					&node->u.ctf_expression.right,
					struct ctf_node, siblings);
				bo = byte_order_from_unary_expr(ctx->efd,
					right_node);
				if (bo == BT_CTF_BYTE_ORDER_UNKNOWN) {
					_PERROR("%s", "unknown \"byte_order\" attribute in trace declaration");
					ret = -EINVAL;
					goto error;
				} else if (bo == BT_CTF_BYTE_ORDER_NATIVE) {
					_PERROR("%s", "\"byte_order\" attribute cannot be set to \"native\" in trace declaration");
					ret = -EPERM;
					goto error;
				}

				ctx->trace_bo = bo;
				ret = bt_ctf_trace_set_native_byte_order(
					ctx->trace, bo);
				if (ret) {
					_PERROR("cannot set trace's byte order (%d)",
						ret);
					goto error;
				}
			}

			g_free(left);
			left = NULL;
		}
	}

	if (!_IS_SET(&set, _TRACE_BYTE_ORDER_SET)) {
		_PERROR("%s", "missing \"byte_order\" attribute in trace declaration");
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
	struct bt_ctf_clock_class *clock, int *set)
{
	int ret = 0;
	char *left = NULL;

	if (entry_node->type != NODE_CTF_EXPRESSION) {
		ret = -EPERM;
		goto error;
	}

	left = concatenate_unary_strings(&entry_node->u.ctf_expression.left);
	if (!left) {
		ret = -EINVAL;
		goto error;
	}

	if (!strcmp(left, "name")) {
		char *right;

		if (_IS_SET(set, _CLOCK_NAME_SET)) {
			_PERROR_DUP_ATTR("name", "clock class declaration");
			ret = -EPERM;
			goto error;
		}

		right = concatenate_unary_strings(
			&entry_node->u.ctf_expression.right);
		if (!right) {
			_PERROR("%s", "unexpected unary expression for clock class declaration's \"name\" attribute");
			ret = -EINVAL;
			goto error;
		}

		ret = bt_ctf_clock_class_set_name(clock, right);
		if (ret) {
			_PERROR("%s", "cannot set clock class's name");
			g_free(right);
			goto error;
		}

		g_free(right);
		_SET(set, _CLOCK_NAME_SET);
	} else if (!strcmp(left, "uuid")) {
		unsigned char uuid[BABELTRACE_UUID_LEN];

		if (_IS_SET(set, _CLOCK_UUID_SET)) {
			_PERROR_DUP_ATTR("uuid", "clock class declaration");
			ret = -EPERM;
			goto error;
		}

		ret = get_unary_uuid(&entry_node->u.ctf_expression.right, uuid);
		if (ret) {
			_PERROR("%s", "invalid clock class UUID");
			goto error;
		}

		ret = bt_ctf_clock_class_set_uuid(clock, uuid);
		if (ret) {
			_PERROR("%s", "cannot set clock class's UUID");
			goto error;
		}

		_SET(set, _CLOCK_UUID_SET);
	} else if (!strcmp(left, "description")) {
		char *right;

		if (_IS_SET(set, _CLOCK_DESCRIPTION_SET)) {
			_PERROR_DUP_ATTR("description", "clock class declaration");
			ret = -EPERM;
			goto error;
		}

		right = concatenate_unary_strings(
			&entry_node->u.ctf_expression.right);
		if (!right) {
			_PERROR("%s", "unexpected unary expression for clock class's \"description\" attribute");
			ret = -EINVAL;
			goto error;
		}

		ret = bt_ctf_clock_class_set_description(clock, right);
		if (ret) {
			_PERROR("%s", "cannot set clock class's description");
			g_free(right);
			goto error;
		}

		g_free(right);
		_SET(set, _CLOCK_DESCRIPTION_SET);
	} else if (!strcmp(left, "freq")) {
		uint64_t freq;

		if (_IS_SET(set, _CLOCK_FREQ_SET)) {
			_PERROR_DUP_ATTR("freq", "clock class declaration");
			ret = -EPERM;
			goto error;
		}

		ret = get_unary_unsigned(
			&entry_node->u.ctf_expression.right, &freq);
		if (ret) {
			_PERROR("%s", "unexpected unary expression for clock class declaration's \"freq\" attribute");
			ret = -EINVAL;
			goto error;
		}

		ret = bt_ctf_clock_class_set_frequency(clock, freq);
		if (ret) {
			_PERROR("%s", "cannot set clock class's frequency");
			goto error;
		}

		_SET(set, _CLOCK_FREQ_SET);
	} else if (!strcmp(left, "precision")) {
		uint64_t precision;

		if (_IS_SET(set, _CLOCK_PRECISION_SET)) {
			_PERROR_DUP_ATTR("precision", "clock class declaration");
			ret = -EPERM;
			goto error;
		}

		ret = get_unary_unsigned(
			&entry_node->u.ctf_expression.right, &precision);
		if (ret) {
			_PERROR("%s", "unexpected unary expression for clock class declaration's \"precision\" attribute");
			ret = -EINVAL;
			goto error;
		}

		ret = bt_ctf_clock_class_set_precision(clock, precision);
		if (ret) {
			_PERROR("%s", "cannot set clock class's precision");
			goto error;
		}

		_SET(set, _CLOCK_PRECISION_SET);
	} else if (!strcmp(left, "offset_s")) {
		uint64_t offset_s;

		if (_IS_SET(set, _CLOCK_OFFSET_S_SET)) {
			_PERROR_DUP_ATTR("offset_s", "clock class declaration");
			ret = -EPERM;
			goto error;
		}

		ret = get_unary_unsigned(
			&entry_node->u.ctf_expression.right, &offset_s);
		if (ret) {
			_PERROR("%s", "unexpected unary expression for clock class declaration's \"offset_s\" attribute");
			ret = -EINVAL;
			goto error;
		}

		ret = bt_ctf_clock_class_set_offset_s(clock, offset_s);
		if (ret) {
			_PERROR("%s", "cannot set clock class's offset in seconds");
			goto error;
		}

		_SET(set, _CLOCK_OFFSET_S_SET);
	} else if (!strcmp(left, "offset")) {
		uint64_t offset;

		if (_IS_SET(set, _CLOCK_OFFSET_SET)) {
			_PERROR_DUP_ATTR("offset", "clock class declaration");
			ret = -EPERM;
			goto error;
		}

		ret = get_unary_unsigned(
			&entry_node->u.ctf_expression.right, &offset);
		if (ret) {
			_PERROR("%s", "unexpected unary expression for clock class declaration's \"offset\" attribute");
			ret = -EINVAL;
			goto error;
		}

		ret = bt_ctf_clock_class_set_offset_cycles(clock, offset);
		if (ret) {
			_PERROR("%s", "cannot set clock class's offset in cycles");
			goto error;
		}

		_SET(set, _CLOCK_OFFSET_SET);
	} else if (!strcmp(left, "absolute")) {
		struct ctf_node *right;

		if (_IS_SET(set, _CLOCK_ABSOLUTE_SET)) {
			_PERROR_DUP_ATTR("absolute", "clock class declaration");
			ret = -EPERM;
			goto error;
		}

		right = _BT_LIST_FIRST_ENTRY(
			&entry_node->u.ctf_expression.right,
			struct ctf_node, siblings);
		ret = get_boolean(ctx->efd, right);
		if (ret < 0) {
			_PERROR("%s", "unexpected unary expression for clock class declaration's \"absolute\" attribute");
			ret = -EINVAL;
			goto error;
		}

		ret = bt_ctf_clock_class_set_is_absolute(clock, ret);
		if (ret) {
			_PERROR("%s", "cannot set clock class's absolute option");
			goto error;
		}

		_SET(set, _CLOCK_ABSOLUTE_SET);
	} else {
		_PWARNING("unknown attribute \"%s\" in clock class declaration",
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
uint64_t cycles_from_ns(uint64_t frequency, uint64_t ns)
{
	uint64_t cycles;

	/* 1GHz */
	if (frequency == 1000000000ULL) {
		cycles = ns;
	} else {
		cycles = (uint64_t) (((double) ns * (double) frequency) / 1e9);
	}

	return cycles;
}

static
int apply_clock_class_offset(struct ctx *ctx, struct bt_ctf_clock_class *clock)
{
	int ret;
	uint64_t freq;
	int64_t offset_cycles;

	freq = bt_ctf_clock_class_get_frequency(clock);
	if (freq == -1ULL) {
		_PERROR("%s", "cannot get clock class frequency");
		ret = -1;
		goto end;
	}

	ret = bt_ctf_clock_class_get_offset_cycles(clock, &offset_cycles);
	if (ret) {
		_PERROR("%s", "cannot get offset in cycles");
		ret = -1;
		goto end;
	}

	offset_cycles += cycles_from_ns(freq, ctx->clock_class_offset_ns);
	ret = bt_ctf_clock_class_set_offset_cycles(clock, offset_cycles);

end:
	return ret;
}

static
int visit_clock_decl(struct ctx *ctx, struct ctf_node *clock_node)
{
	int ret = 0;
	int set = 0;
	struct bt_ctf_clock_class *clock;
	struct ctf_node *entry_node;
	struct bt_list_head *decl_list = &clock_node->u.clock.declaration_list;

	if (clock_node->visited) {
		return 0;
	}

	clock_node->visited = TRUE;
	clock = bt_ctf_clock_class_create(NULL);
	if (!clock) {
		_PERROR("%s", "cannot create clock");
		ret = -ENOMEM;
		goto error;
	}

	bt_list_for_each_entry(entry_node, decl_list, siblings) {
		ret = visit_clock_decl_entry(ctx, entry_node, clock, &set);
		if (ret) {
			goto error;
		}
	}

	if (!_IS_SET(&set, _CLOCK_NAME_SET)) {
		_PERROR("%s",
			"missing \"name\" attribute in clock class declaration");
		ret = -EPERM;
		goto error;
	}

	if (bt_ctf_trace_get_clock_class_count(ctx->trace) != 0) {
		_PERROR("%s", "only CTF traces with a single clock class declaration are supported as of this version");
		ret = -EINVAL;
		goto error;
	}

	ret = apply_clock_class_offset(ctx, clock);
	if (ret) {
		_PERROR("%s", "cannot apply clock class offset ");
		goto error;
	}

	ret = bt_ctf_trace_add_clock_class(ctx->trace, clock);
	if (ret) {
		_PERROR("%s", "cannot add clock class to trace");
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
			_PERROR("%s", "cannot add typedef in root scope");
			goto end;
		}
		break;
	case NODE_TYPEALIAS:
		ret = visit_typealias(ctx, root_decl_node->u.typealias.target,
			root_decl_node->u.typealias.alias);
		if (ret) {
			_PERROR("%s", "cannot add typealias in root scope");
			goto end;
		}
		break;
	case NODE_TYPE_SPECIFIER_LIST:
	{
		_BT_CTF_FIELD_TYPE_INIT(decl);

		/*
		 * Just add the type specifier to the root
		 * declaration scope. Put local reference.
		 */
		ret = visit_type_specifier_list(ctx, root_decl_node, &decl);
		if (ret) {
			assert(!decl);
			goto end;
		}

		BT_PUT(decl);
		break;
	}
	default:
		ret = -EPERM;
		goto end;
	}

end:
	return ret;
}

static
int move_ctx_stream_classes_to_trace(struct ctx *ctx)
{
	int ret;
	GHashTableIter iter;
	gpointer key, stream_class;

	g_hash_table_iter_init(&iter, ctx->stream_classes);

	while (g_hash_table_iter_next(&iter, &key, &stream_class)) {
		ret = bt_ctf_trace_add_stream_class(ctx->trace,
			stream_class);
		if (ret) {
			int64_t id = bt_ctf_stream_class_get_id(stream_class);
			_PERROR("cannot add stream class %" PRId64 " to trace",
				id);
			goto end;
		}
	}

	g_hash_table_remove_all(ctx->stream_classes);

end:
	return ret;
}

BT_HIDDEN
struct ctf_visitor_generate_ir *ctf_visitor_generate_ir_create(FILE *efd,
		uint64_t clock_class_offset_ns)
{
	int ret;
	struct ctx *ctx = NULL;
	struct bt_ctf_trace *trace;

	trace = bt_ctf_trace_create();
	if (!trace) {
		_FPERROR(efd, "%s", "cannot create trace");
		goto error;
	}

	/* Set packet header to NULL to override the default one */
	ret = bt_ctf_trace_set_packet_header_type(trace, NULL);
	if (ret) {
		_FPERROR(efd, "%s",
			"cannot set initial, empty packet header structure");
		goto error;
	}

	/* Create visitor's context */
	ctx = ctx_create(trace, efd, clock_class_offset_ns);
	if (!ctx) {
		_FPERROR(efd, "%s", "cannot create visitor context");
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
struct bt_ctf_trace *ctf_visitor_generate_ir_get_trace(
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

	printf_verbose("CTF visitor: AST -> CTF IR...\n");

	switch (node->type) {
	case NODE_ROOT:
	{
		struct ctf_node *iter;
		int got_trace_decl = FALSE;
		int found_callsite = FALSE;

		/*
		 * The first thing we need is the native byte order of
		 * the trace block, because early type aliases can have
		 * a `byte_order` attribute set to `native`. If we don't
		 * have the native byte order yet, and we don't have any
		 * trace block yet, then fail with EINCOMPLETE.
		 */
		if (ctx->trace_bo == BT_CTF_BYTE_ORDER_NATIVE) {
			bt_list_for_each_entry(iter, &node->u.root.trace, siblings) {
				if (got_trace_decl) {
					_PERROR("%s", "duplicate trace declaration");
					ret = -1;
					goto end;
				}

				ret = set_trace_byte_order(ctx, iter);
				if (ret) {
					_PERROR("cannot set trace's native byte order (%d)",
						ret);
					goto end;
				}

				got_trace_decl = TRUE;
			}

			if (!got_trace_decl) {
				ret = -EINCOMPLETE;
				goto end;
			}
		}

		assert(ctx->trace_bo == BT_CTF_BYTE_ORDER_LITTLE_ENDIAN ||
			ctx->trace_bo == BT_CTF_BYTE_ORDER_BIG_ENDIAN);
		assert(ctx->current_scope &&
			ctx->current_scope->parent_scope == NULL);

		/*
		 * Visit clocks first since any early integer can be mapped
		 * to one.
		 */
		bt_list_for_each_entry(iter, &node->u.root.clock, siblings) {
			ret = visit_clock_decl(ctx, iter);
			if (ret) {
				_PERROR("error while visiting clock class declaration (%d)",
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
				_PERROR("error while visiting root declaration (%d)",
					ret);
				goto end;
			}
		}

		assert(ctx->current_scope &&
			ctx->current_scope->parent_scope == NULL);

		/* Callsite blocks are not supported */
		bt_list_for_each_entry(iter, &node->u.root.callsite, siblings) {
			found_callsite = TRUE;
			break;
		}

		assert(ctx->current_scope &&
			ctx->current_scope->parent_scope == NULL);

		if (found_callsite) {
			_PWARNING("%s", "\"callsite\" blocks are not supported as of this version");
		}

		/* Environment */
		bt_list_for_each_entry(iter, &node->u.root.env, siblings) {
			ret = visit_env(ctx, iter);
			if (ret) {
				_PERROR("error while visiting environment block (%d)",
					ret);
				goto end;
			}
		}

		assert(ctx->current_scope &&
			ctx->current_scope->parent_scope == NULL);

		/* Trace */
		bt_list_for_each_entry(iter, &node->u.root.trace, siblings) {
			ret = visit_trace_decl(ctx, iter);
			if (ret) {
				_PERROR("%s", "error while visiting trace declaration");
				goto end;
			}
		}

		assert(ctx->current_scope &&
			ctx->current_scope->parent_scope == NULL);

		/* Streams */
		bt_list_for_each_entry(iter, &node->u.root.stream, siblings) {
			ret = visit_stream_decl(ctx, iter);
			if (ret) {
				_PERROR("%s", "error while visiting stream declaration");
				goto end;
			}
		}

		assert(ctx->current_scope &&
			ctx->current_scope->parent_scope == NULL);

		/* Events */
		bt_list_for_each_entry(iter, &node->u.root.event, siblings) {
			ret = visit_event_decl(ctx, iter);
			if (ret) {
				_PERROR("%s", "error while visiting event declaration");
				goto end;
			}
		}

		assert(ctx->current_scope &&
			ctx->current_scope->parent_scope == NULL);
		break;
	}
	default:
		_PERROR("cannot decode node type: %d", (int) node->type);
		ret = -EINVAL;
		goto end;
	}

	/* Move decoded stream classes to trace, if any */
	ret = move_ctx_stream_classes_to_trace(ctx);
	if (ret) {
		_PERROR("%s", "cannot move stream classes to trace");
	}

end:
	return ret;
}
