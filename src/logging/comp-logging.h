/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2019 Philippe Proulx <pproulx@efficios.com>
 */

#ifndef BABELTRACE_LOGGING_COMP_LOGGING_H
#define BABELTRACE_LOGGING_COMP_LOGGING_H

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
	BT_LOG_WRITE_PRINTF_CUR_LVL((enum bt_log_level) (_lvl),		\
		(enum bt_log_level) (BT_LOG_OUTPUT_LEVEL), BT_LOG_TAG,	\
		_BT_COMP_LOG_COMP_PREFIX _fmt,				\
		(_self_comp) ?						\
			bt_component_get_name(				\
				bt_self_component_as_component(_self_comp)) : \
			_BT_COMP_LOG_COMP_NA_STR,			\
		##__VA_ARGS__)

/* Logs with level `_lvl` for self component class `_self_comp_class` */
#define BT_COMP_CLASS_LOG(_lvl, _self_comp_class, _fmt, ...)		\
	BT_LOG_WRITE_PRINTF_CUR_LVL((enum bt_log_level) (_lvl),		\
		(enum bt_log_level) (BT_LOG_OUTPUT_LEVEL), BT_LOG_TAG,	\
		_BT_COMP_LOG_COMP_PREFIX _fmt,				\
		bt_component_class_get_name(				\
			bt_self_component_class_as_component_class(	\
				_self_comp_class)), ##__VA_ARGS__)

#define BT_COMP_LOG_CUR_LVL(_lvl, _cur_lvl, _self_comp, _fmt, ...)	\
	BT_LOG_WRITE_PRINTF_CUR_LVL((enum bt_log_level) (_lvl),		\
		(enum bt_log_level) (_cur_lvl), BT_LOG_TAG,		\
		_BT_COMP_LOG_COMP_PREFIX _fmt,				\
		(_self_comp) ?						\
			bt_component_get_name(				\
				bt_self_component_as_component(_self_comp)) : \
			_BT_COMP_LOG_COMP_NA_STR,			\
		##__VA_ARGS__)

#define BT_COMP_LOG_ERRNO(_lvl, _self_comp, _msg, _fmt, ...)		\
	BT_LOG_WRITE_ERRNO_PRINTF_CUR_LVL((enum bt_log_level) (_lvl),	\
		(enum bt_log_level) (BT_LOG_OUTPUT_LEVEL),		\
		BT_LOG_TAG, _msg,					\
		_BT_COMP_LOG_COMP_PREFIX _fmt,				\
		(_self_comp) ?						\
			bt_component_get_name(				\
				bt_self_component_as_component(_self_comp)) : \
			_BT_COMP_LOG_COMP_NA_STR,			\
		##__VA_ARGS__)

#define BT_COMP_LOG_ERRNO_CUR_LVL(_lvl, _cur_lvl, _self_comp, _msg, _fmt, ...) \
	BT_LOG_WRITE_ERRNO_PRINTF_CUR_LVL((enum bt_log_level) (_lvl),	\
		(enum bt_log_level) (_cur_lvl), BT_LOG_TAG, _msg,	\
		_BT_COMP_LOG_COMP_PREFIX _fmt,				\
		(_self_comp) ?						\
			bt_component_get_name(				\
				bt_self_component_as_component(_self_comp)) : \
			_BT_COMP_LOG_COMP_NA_STR,			\
		##__VA_ARGS__)

#define BT_COMP_LOG_MEM(_lvl, _self_comp, _data_ptr, _data_sz, _fmt, ...) \
	BT_LOG_WRITE_MEM_PRINTF_CUR_LVL((enum bt_log_level) (_lvl),	\
		(enum bt_log_level) (BT_LOG_OUTPUT_LEVEL), BT_LOG_TAG,	\
		(_data_ptr), (_data_sz),				\
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
#define BT_COMP_LOG_APPEND_CAUSE(_lvl, _self_comp, _fmt, ...)			\
	do {									\
		BT_COMP_LOG(_lvl, _self_comp, _fmt, ##__VA_ARGS__);		\
		(void) BT_CURRENT_THREAD_ERROR_APPEND_CAUSE_FROM_COMPONENT( 	\
			_self_comp, _fmt, ##__VA_ARGS__);			\
	} while (0)

/* Logs error and appends error cause from component context. */
#define BT_COMP_LOGE_APPEND_CAUSE(_self_comp, _fmt, ...)			\
	BT_COMP_LOG_APPEND_CAUSE(BT_LOG_ERROR, _self_comp, _fmt, ##__VA_ARGS__)

/*
 * Logs and appends error cause from component context - the errno edition.
 */
#define BT_COMP_LOG_APPEND_CAUSE_ERRNO(_lvl, _self_comp, _msg, _fmt, ...)		\
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
	BT_COMP_LOG_APPEND_CAUSE_ERRNO(BT_LOG_ERROR, _self_comp, _msg, _fmt, ##__VA_ARGS__)

/* Logs error from component class context. */
#define BT_COMP_CLASS_LOGE(_self_comp_class, _fmt, ...)					\
	BT_COMP_CLASS_LOG(BT_LOG_ERROR,_self_comp_class, _fmt, ##__VA_ARGS__)

/* Logs error and errno string from component class context. */
#define BT_COMP_CLASS_LOG_ERRNO(_lvl, _self_comp_class, _msg, _fmt, ...)		\
	BT_LOG_WRITE_ERRNO_PRINTF_CUR_LVL((enum bt_log_level) (_lvl),			\
		(enum bt_log_level) (BT_LOG_OUTPUT_LEVEL), BT_LOG_TAG, _msg,		\
		_BT_COMP_LOG_COMP_PREFIX _fmt,						\
		bt_component_class_get_name(						\
			bt_self_component_class_as_component_class(_self_comp_class))  	\
		##__VA_ARGS__)

/* Logs and appends error cause from component class context. */
#define BT_COMP_CLASS_LOG_APPEND_CAUSE(_lvl, _self_comp_class, _fmt, ...)		\
	do {										\
		BT_COMP_CLASS_LOG(_lvl, _self_comp_class, _fmt, ##__VA_ARGS__);		\
		(void) BT_CURRENT_THREAD_ERROR_APPEND_CAUSE_FROM_COMPONENT_CLASS(	\
			_self_comp_class, _fmt, ##__VA_ARGS__);				\
	} while (0)

/* Logs error and appends error cause from component class context. */
#define BT_COMP_CLASS_LOGE_APPEND_CAUSE(_self_comp_class, _fmt, ...)				\
	BT_COMP_CLASS_LOG_APPEND_CAUSE(BT_LOG_ERROR, _self_comp_class, _fmt, ##__VA_ARGS__)

/*
 * Logs and appends error cause from component class context - the errno
 * edition.
 */
#define BT_COMP_CLASS_LOG_APPEND_CAUSE_ERRNO(_lvl, _self_comp_class, _msg, _fmt, ...)	\
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
	BT_COMP_CLASS_LOG_APPEND_CAUSE_ERRNO(BT_LOG_ERROR, _self_comp_class, _msg, _fmt,	\
		##__VA_ARGS__)

/*
 * Logs error from component or component class context, depending on whichever
 * is set.
 */
#define BT_COMP_OR_COMP_CLASS_LOG(_lvl, _self_comp, _self_comp_class, _fmt, ...)		\
	do {											\
		/* Only one of `_self_comp` and `_self_comp_class` must be set. */		\
		BT_ASSERT((!!(_self_comp) != (!!_self_comp_class)));				\
		if (_self_comp) {								\
			BT_COMP_LOG(_lvl, _self_comp, _fmt, ##__VA_ARGS__);			\
		} else {									\
			BT_COMP_CLASS_LOG(_lvl, _self_comp_class, _fmt, ##__VA_ARGS__);		\
		}										\
	} while (0)

#define BT_COMP_OR_COMP_CLASS_LOGE(_self_comp, _self_comp_class, _fmt, ...)			\
	BT_COMP_OR_COMP_CLASS_LOG(BT_LOG_ERROR,_self_comp, _self_comp_class, _fmt, ##__VA_ARGS__)
#define BT_COMP_OR_COMP_CLASS_LOGW(_self_comp, _self_comp_class, _fmt, ...)			\
	BT_COMP_OR_COMP_CLASS_LOG(BT_LOG_WARNING,_self_comp, _self_comp_class, _fmt, ##__VA_ARGS__)
#define BT_COMP_OR_COMP_CLASS_LOGI(_self_comp, _self_comp_class, _fmt, ...)			\
	BT_COMP_OR_COMP_CLASS_LOG(BT_LOG_INFO,_self_comp, _self_comp_class, _fmt, ##__VA_ARGS__)
#define BT_COMP_OR_COMP_CLASS_LOGD(_self_comp, _self_comp_class, _fmt, ...)			\
	BT_COMP_OR_COMP_CLASS_LOG(BT_LOG_DEBUG,_self_comp, _self_comp_class, _fmt, ##__VA_ARGS__)

/*
 * Logs error with errno string from component or component class context,
 * depending on whichever is set.
 */
#define BT_COMP_OR_COMP_CLASS_LOG_ERRNO(_lvl, _self_comp, _self_comp_class, _msg, _fmt, ...)	\
	do {											\
		/* Only one of `_self_comp` and `_self_comp_class` must be set. */		\
		BT_ASSERT((!!(_self_comp) != (!!_self_comp_class)));				\
		if (_self_comp) {								\
			BT_COMP_LOG_ERRNO(_lvl, _self_comp, _msg, _fmt, ##__VA_ARGS__);	    	\
		} else {									\
			BT_COMP_CLASS_LOG_ERRNO(_lvl, _self_comp_class, _msg, _fmt, ##__VA_ARGS__);	\
		}										\
	} while (0)

#define BT_COMP_OR_COMP_CLASS_LOGW_ERRNO(_self_comp, _self_comp_class, _msg, _fmt, ...)		\
	BT_COMP_OR_COMP_CLASS_LOG_ERRNO(BT_LOG_WARNING, _self_comp, _self_comp_class, _msg, _fmt, ##__VA_ARGS__)

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

/*
 * Logs error and appends error cause from message iterator context.
 *
 * There is no BT_SELF_MSG_LOGE yet, so use BT_COMP_LOGE for now.
 */
#define BT_MSG_ITER_LOGE_APPEND_CAUSE(_self_msg_iter, _fmt, ...)					\
	do {												\
		BT_COMP_LOG(BT_LOG_ERROR, bt_self_message_iterator_borrow_component(_self_msg_iter),	\
			_fmt, ##__VA_ARGS__);								\
		(void) BT_CURRENT_THREAD_ERROR_APPEND_CAUSE_FROM_MESSAGE_ITERATOR( 			\
			_self_msg_iter, _fmt, ##__VA_ARGS__);						\
	} while (0)

#endif /* BABELTRACE_LOGGING_COMP_LOGGING_H */
