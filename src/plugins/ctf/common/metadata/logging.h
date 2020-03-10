/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2017 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 */

#ifndef CTF_METADATA_LOGGING_H
#define CTF_METADATA_LOGGING_H

#include <babeltrace2/babeltrace.h>
#include "logging/log.h"

/*
 * This global log level is for the generated lexer and parser: we can't
 * use a contextual log level for their "tracing", so they rely on this.
 */
BT_LOG_LEVEL_EXTERN_SYMBOL(ctf_plugin_metadata_log_level);

/*
 * To be used by functions without a context structure to pass all the
 * logging configuration at once.
 */
struct meta_log_config {
	bt_logging_level log_level;

	/* Weak */
	bt_self_component *self_comp;
};

#define _BT_LOGT_LINENO(_lineno, _msg, args...) \
	BT_LOGT("At line %u in metadata stream: " _msg, _lineno, ## args)

#define _BT_LOGW_LINENO(_lineno, _msg, args...) \
	BT_LOGW("At line %u in metadata stream: " _msg, _lineno, ## args)

#define _BT_LOGE_LINENO(_lineno, _msg, args...) \
	BT_LOGE("At line %u in metadata stream: " _msg, _lineno, ## args)

#define _BT_COMP_LOGT_LINENO(_lineno, _msg, args...) \
	BT_COMP_LOGT("At line %u in metadata stream: " _msg, _lineno, ## args)

#define _BT_COMP_LOGW_LINENO(_lineno, _msg, args...) \
	BT_COMP_LOGW("At line %u in metadata stream: " _msg, _lineno, ## args)

#define _BT_COMP_LOGE_LINENO(_lineno, _msg, args...) \
	BT_COMP_LOGE("At line %u in metadata stream: " _msg, _lineno, ## args)

#endif /* CTF_METADATA_LOGGING_H */
