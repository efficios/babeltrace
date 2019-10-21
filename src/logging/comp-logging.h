#ifndef BABELTRACE_LOGGING_COMP_LOGGING_H
#define BABELTRACE_LOGGING_COMP_LOGGING_H

/*
 * Copyright 2019 Philippe Proulx <pproulx@efficios.com>
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

#ifndef BT_LOG_TAG
# error Please define a tag with BT_LOG_TAG before including this file.
#endif

#include <stdlib.h>
#include <babeltrace2/babeltrace.h>
#include "logging/log.h"

#define _BT_COMP_LOG_COMP_PREFIX	"[%s] "
#define _BT_COMP_LOG_COMP_NA_STR	"N/A"

/* Logs with level `_lvl` for self component `_self_comp` */
#define BT_COMP_LOG(_lvl, _self_comp, _fmt, ...)			\
	BT_LOG_WRITE((_lvl), BT_LOG_TAG, _BT_COMP_LOG_COMP_PREFIX _fmt,	\
		(_self_comp) ?						\
			bt_component_get_name(				\
				bt_self_component_as_component(_self_comp)) : \
			_BT_COMP_LOG_COMP_NA_STR,			\
		##__VA_ARGS__)

/* Logs with level `_lvl` for self component class `_self_comp_class` */
#define BT_COMP_CLASS_LOG(_lvl, _self_comp_class, _fmt, ...)		\
	BT_LOG_WRITE((_lvl), BT_LOG_TAG, _BT_COMP_LOG_COMP_PREFIX _fmt,	\
		bt_component_class_get_name(				\
			bt_self_component_class_as_component_class(	\
				_self_comp_class)), ##__VA_ARGS__)

#define BT_COMP_LOG_CUR_LVL(_lvl, _cur_lvl, _self_comp, _fmt, ...)	\
	BT_LOG_WRITE_CUR_LVL((_lvl), (_cur_lvl), BT_LOG_TAG,		\
		_BT_COMP_LOG_COMP_PREFIX _fmt,				\
		(_self_comp) ?						\
			bt_component_get_name(				\
				bt_self_component_as_component(_self_comp)) : \
			_BT_COMP_LOG_COMP_NA_STR,			\
		##__VA_ARGS__)

#define BT_COMP_LOG_ERRNO(_lvl, _self_comp, _msg, _fmt, ...)		\
	BT_LOG_WRITE_ERRNO((_lvl), BT_LOG_TAG, _msg,			\
		_BT_COMP_LOG_COMP_PREFIX _fmt,				\
		(_self_comp) ?						\
			bt_component_get_name(				\
				bt_self_component_as_component(_self_comp)) : \
			_BT_COMP_LOG_COMP_NA_STR,			\
		##__VA_ARGS__)

#define BT_COMP_LOG_ERRNO_CUR_LVL(_lvl, _cur_lvl, _self_comp, _msg, _fmt, ...) \
	BT_LOG_WRITE_ERRNO_CUR_LVL((_lvl), (_cur_lvl), BT_LOG_TAG, _msg, \
		_BT_COMP_LOG_COMP_PREFIX _fmt,				\
		(_self_comp) ?						\
			bt_component_get_name(				\
				bt_self_component_as_component(_self_comp)) : \
			_BT_COMP_LOG_COMP_NA_STR,			\
		##__VA_ARGS__)

#define BT_COMP_LOG_MEM(_lvl, _self_comp, _data_ptr, _data_sz, _fmt, ...) \
	BT_LOG_WRITE_MEM((_lvl), BT_LOG_TAG, (_data_ptr), (_data_sz),	\
		_BT_COMP_LOG_COMP_PREFIX _fmt,				\
		(_self_comp) ?						\
			bt_component_get_name(				\
				bt_self_component_as_component(_self_comp)) : \
			_BT_COMP_LOG_COMP_NA_STR,			\
		##__VA_ARGS__)

/* Specific per-level logging macros; they use `BT_COMP_LOG_SELF_COMP` */
#define BT_COMP_LOGF(_fmt, ...) \
	BT_COMP_LOG(BT_LOG_FATAL, (BT_COMP_LOG_SELF_COMP), _fmt, ##__VA_ARGS__)
#define BT_COMP_LOGE(_fmt, ...) \
	BT_COMP_LOG(BT_LOG_ERROR, (BT_COMP_LOG_SELF_COMP), _fmt, ##__VA_ARGS__)
#define BT_COMP_LOGW(_fmt, ...) \
	BT_COMP_LOG(BT_LOG_WARNING, (BT_COMP_LOG_SELF_COMP), _fmt, ##__VA_ARGS__)
#define BT_COMP_LOGI(_fmt, ...) \
	BT_COMP_LOG(BT_LOG_INFO, (BT_COMP_LOG_SELF_COMP), _fmt, ##__VA_ARGS__)
#define BT_COMP_LOGD(_fmt, ...) \
	BT_COMP_LOG(BT_LOG_DEBUG, (BT_COMP_LOG_SELF_COMP), _fmt, ##__VA_ARGS__)
#define BT_COMP_LOGT(_fmt, ...) \
	BT_COMP_LOG(BT_LOG_TRACE, (BT_COMP_LOG_SELF_COMP), _fmt, ##__VA_ARGS__)
#define BT_COMP_LOGF_STR(_str) \
	BT_COMP_LOG(BT_LOG_FATAL, (BT_COMP_LOG_SELF_COMP), "%s", (_str))
#define BT_COMP_LOGE_STR(_str) \
	BT_COMP_LOG(BT_LOG_ERROR, (BT_COMP_LOG_SELF_COMP), "%s", (_str))
#define BT_COMP_LOGW_STR(_str) \
	BT_COMP_LOG(BT_LOG_WARNING, (BT_COMP_LOG_SELF_COMP), "%s", (_str))
#define BT_COMP_LOGI_STR(_str) \
	BT_COMP_LOG(BT_LOG_INFO, (BT_COMP_LOG_SELF_COMP), "%s", (_str))
#define BT_COMP_LOGD_STR(_str) \
	BT_COMP_LOG(BT_LOG_DEBUG, (BT_COMP_LOG_SELF_COMP), "%s", (_str))
#define BT_COMP_LOGT_STR(_str) \
	BT_COMP_LOG(BT_LOG_TRACE, (BT_COMP_LOG_SELF_COMP), "%s", (_str))
#define BT_COMP_LOGF_ERRNO(_msg, _fmt, ...) \
	BT_COMP_LOG_ERRNO(BT_LOG_FATAL, (BT_COMP_LOG_SELF_COMP), _msg, _fmt, ##__VA_ARGS__)
#define BT_COMP_LOGE_ERRNO(_msg, _fmt, ...) \
	BT_COMP_LOG_ERRNO(BT_LOG_ERROR, (BT_COMP_LOG_SELF_COMP), _msg, _fmt, ##__VA_ARGS__)
#define BT_COMP_LOGW_ERRNO(_msg, _fmt, ...) \
	BT_COMP_LOG_ERRNO(BT_LOG_WARNING, (BT_COMP_LOG_SELF_COMP), _msg, _fmt, ##__VA_ARGS__)
#define BT_COMP_LOGI_ERRNO(_msg, _fmt, ...) \
	BT_COMP_LOG_ERRNO(BT_LOG_INFO, (BT_COMP_LOG_SELF_COMP), _msg, _fmt, ##__VA_ARGS__)
#define BT_COMP_LOGD_ERRNO(_msg, _fmt, ...) \
	BT_COMP_LOG_ERRNO(BT_LOG_DEBUG, (BT_COMP_LOG_SELF_COMP), _msg, _fmt, ##__VA_ARGS__)
#define BT_COMP_LOGT_ERRNO(_msg, _fmt, ...) \
	BT_COMP_LOG_ERRNO(BT_LOG_TRACE, (BT_COMP_LOG_SELF_COMP), _msg, _fmt, ##__VA_ARGS__)
#define BT_COMP_LOGF_MEM(_data_ptr, _data_sz, _fmt, ...) \
	BT_COMP_LOG_MEM(BT_LOG_FATAL, (BT_COMP_LOG_SELF_COMP), (_data_ptr), (_data_sz), _fmt, ##__VA_ARGS__)
#define BT_COMP_LOGE_MEM(_data_ptr, _data_sz, _fmt, ...) \
	BT_COMP_LOG_MEM(BT_LOG_ERROR, (BT_COMP_LOG_SELF_COMP), (_data_ptr), (_data_sz), _fmt, ##__VA_ARGS__)
#define BT_COMP_LOGW_MEM(_data_ptr, _data_sz, _fmt, ...) \
	BT_COMP_LOG_MEM(BT_LOG_WARNING, (BT_COMP_LOG_SELF_COMP), (_data_ptr), (_data_sz), _fmt, ##__VA_ARGS__)
#define BT_COMP_LOGI_MEM(_data_ptr, _data_sz, _fmt, ...) \
	BT_COMP_LOG_MEM(BT_LOG_INFO, (BT_COMP_LOG_SELF_COMP), (_data_ptr), (_data_sz), _fmt, ##__VA_ARGS__)
#define BT_COMP_LOGD_MEM(_data_ptr, _data_sz, _fmt, ...) \
	BT_COMP_LOG_MEM(BT_LOG_DEBUG, (BT_COMP_LOG_SELF_COMP), (_data_ptr), (_data_sz), _fmt, ##__VA_ARGS__)
#define BT_COMP_LOGT_MEM(_data_ptr, _data_sz, _fmt, ...) \
	BT_COMP_LOG_MEM(BT_LOG_TRACE, (BT_COMP_LOG_SELF_COMP), (_data_ptr), (_data_sz), _fmt, ##__VA_ARGS__)

#define BT_COMP_LOG_SUPPORTED

/* Logs and appends error cause from component context. */
#define BT_COMP_LOG_APPEND_CAUSE(_self_comp, _lvl, _fmt, ...)			\
	do {									\
		BT_COMP_LOG(_lvl, _self_comp, _fmt, ##__VA_ARGS__);		\
		(void) BT_CURRENT_THREAD_ERROR_APPEND_CAUSE_FROM_COMPONENT( 	\
			_self_comp, _fmt, ##__VA_ARGS__);			\
	} while (0)

/* Logs error and appends error cause from component context. */
#define BT_COMP_LOGE_APPEND_CAUSE(_self_comp, _fmt, ...)			\
	BT_COMP_LOG_APPEND_CAUSE(_self_comp, BT_LOG_ERROR, _fmt, ##__VA_ARGS__)

/*
 * Logs and appends error cause from component context - the errno edition.
 */
#define BT_COMP_LOG_APPEND_CAUSE_ERRNO(_self_comp, _lvl, _msg, _fmt, ...)		\
	do {										\
		const char *error_str = g_strerror(errno);				\
		BT_COMP_LOG(_lvl, _self_comp, _msg ": %s" _fmt, error_str,		\
			##__VA_ARGS__);							\
		(void) BT_CURRENT_THREAD_ERROR_APPEND_CAUSE_FROM_COMPONENT(		\
			_self_comp, _msg ": %s" _fmt, error_str, ##__VA_ARGS__);	\
	} while (0)

/*
 * Logs error and appends error cause from component context - the errno
 * edition.
 */
#define BT_COMP_LOGE_APPEND_CAUSE_ERRNO(_self_comp, _msg, _fmt, ...)				\
	BT_COMP_LOG_APPEND_CAUSE_ERRNO(_self_comp, BT_LOG_ERROR, _msg, _fmt, ##__VA_ARGS__)

/* Logs and appends error cause from component class context. */
#define BT_COMP_CLASS_LOG_APPEND_CAUSE(_self_comp_class, _lvl, _fmt, ...)		\
	do {										\
		BT_COMP_CLASS_LOG(_lvl, _self_comp_class, _fmt, ##__VA_ARGS__);		\
		(void) BT_CURRENT_THREAD_ERROR_APPEND_CAUSE_FROM_COMPONENT_CLASS(	\
			_self_comp_class, _fmt, ##__VA_ARGS__);				\
	} while (0)

/* Logs error and appends error cause from component class context. */
#define BT_COMP_CLASS_LOGE_APPEND_CAUSE(_self_comp_class, _fmt, ...)				\
	BT_COMP_CLASS_LOG_APPEND_CAUSE(_self_comp_class, BT_LOG_ERROR, _fmt, ##__VA_ARGS__)

/*
 * Logs and appends error cause from component class context - the errno
 * edition.
 */
#define BT_COMP_CLASS_LOG_APPEND_CAUSE_ERRNO(_self_comp_class, _lvl, _msg, _fmt, ...)	\
	do {										\
		const char *error_str = g_strerror(errno);				\
		BT_COMP_CLASS_LOG(_lvl, _self_comp_class, _msg ": %s" _fmt, error_str,	\
			##__VA_ARGS__);							\
		(void) BT_CURRENT_THREAD_ERROR_APPEND_CAUSE_FROM_COMPONENT_CLASS(	\
			_self_comp_class, _msg ": %s" _fmt, error_str, ##__VA_ARGS__);	\
	} while (0)

/*
 * Logs error and appends error cause from component class context - the errno
 * edition.
 */
#define BT_COMP_CLASS_LOGE_APPEND_CAUSE_ERRNO(_self_comp_class, _msg, _fmt, ...)		\
	BT_COMP_CLASS_LOG_APPEND_CAUSE_ERRNO(_self_comp_class, BT_LOG_ERROR, _msg, _fmt,	\
		##__VA_ARGS__)

/*
 * Logs error and appends error cause from component or component class context,
 * depending on whichever is set.
 */
#define BT_COMP_OR_COMP_CLASS_LOGE_APPEND_CAUSE(_self_comp, _self_comp_class, _fmt, ...)	\
	do {											\
		/* Only one of `_self_comp` and `_self_comp_class` must be set. */		\
		BT_ASSERT((!!(_self_comp) != (!!_self_comp_class)));				\
		if (_self_comp) {								\
			BT_COMP_LOGE_APPEND_CAUSE(_self_comp, _fmt, ##__VA_ARGS__);		\
		} else {									\
			BT_COMP_CLASS_LOGE_APPEND_CAUSE(_self_comp_class, _fmt, ##__VA_ARGS__);	\
		}										\
	} while (0)

#endif /* BABELTRACE_LOGGING_COMP_LOGGING_H */
