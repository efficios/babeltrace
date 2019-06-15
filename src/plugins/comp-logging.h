#ifndef BABELTRACE_PLUGINS_COMP_LOGGING_H
#define BABELTRACE_PLUGINS_COMP_LOGGING_H

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

#ifndef BT_LOG_OUTPUT_LEVEL
# error Please define a log level expression with BT_LOG_OUTPUT_LEVEL before including this file.
#endif

#ifndef BT_COMP_LOG_SELF_COMP
# error Please define a self component address expression with BT_COMP_LOG_SELF_COMP before including this file.
#endif

#include <stdlib.h>
#include <babeltrace2/graph/self-component.h>
#include <babeltrace2/graph/component-const.h>
#include <babeltrace2/graph/component-class-const.h>
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
	BT_COMP_LOG(BT_LOG_WARN, (BT_COMP_LOG_SELF_COMP), _fmt, ##__VA_ARGS__)
#define BT_COMP_LOGI(_fmt, ...) \
	BT_COMP_LOG(BT_LOG_INFO, (BT_COMP_LOG_SELF_COMP), _fmt, ##__VA_ARGS__)
#define BT_COMP_LOGD(_fmt, ...) \
	BT_COMP_LOG(BT_LOG_DEBUG, (BT_COMP_LOG_SELF_COMP), _fmt, ##__VA_ARGS__)
#define BT_COMP_LOGV(_fmt, ...) \
	BT_COMP_LOG(BT_LOG_VERBOSE, (BT_COMP_LOG_SELF_COMP), _fmt, ##__VA_ARGS__)
#define BT_COMP_LOGF_STR(_str) \
	BT_COMP_LOG(BT_LOG_FATAL, (BT_COMP_LOG_SELF_COMP), "%s", (_str))
#define BT_COMP_LOGE_STR(_str) \
	BT_COMP_LOG(BT_LOG_ERROR, (BT_COMP_LOG_SELF_COMP), "%s", (_str))
#define BT_COMP_LOGW_STR(_str) \
	BT_COMP_LOG(BT_LOG_WARN, (BT_COMP_LOG_SELF_COMP), "%s", (_str))
#define BT_COMP_LOGI_STR(_str) \
	BT_COMP_LOG(BT_LOG_INFO, (BT_COMP_LOG_SELF_COMP), "%s", (_str))
#define BT_COMP_LOGD_STR(_str) \
	BT_COMP_LOG(BT_LOG_DEBUG, (BT_COMP_LOG_SELF_COMP), "%s", (_str))
#define BT_COMP_LOGV_STR(_str) \
	BT_COMP_LOG(BT_LOG_VERBOSE, (BT_COMP_LOG_SELF_COMP), "%s", (_str))
#define BT_COMP_LOGF_ERRNO(_msg, _fmt, ...) \
	BT_COMP_LOG_ERRNO(BT_LOG_FATAL, (BT_COMP_LOG_SELF_COMP), _msg, _fmt, ##__VA_ARGS__)
#define BT_COMP_LOGE_ERRNO(_msg, _fmt, ...) \
	BT_COMP_LOG_ERRNO(BT_LOG_ERROR, (BT_COMP_LOG_SELF_COMP), _msg, _fmt, ##__VA_ARGS__)
#define BT_COMP_LOGW_ERRNO(_msg, _fmt, ...) \
	BT_COMP_LOG_ERRNO(BT_LOG_WARN, (BT_COMP_LOG_SELF_COMP), _msg, _fmt, ##__VA_ARGS__)
#define BT_COMP_LOGI_ERRNO(_msg, _fmt, ...) \
	BT_COMP_LOG_ERRNO(BT_LOG_INFO, (BT_COMP_LOG_SELF_COMP), _msg, _fmt, ##__VA_ARGS__)
#define BT_COMP_LOGD_ERRNO(_msg, _fmt, ...) \
	BT_COMP_LOG_ERRNO(BT_LOG_DEBUG, (BT_COMP_LOG_SELF_COMP), _msg, _fmt, ##__VA_ARGS__)
#define BT_COMP_LOGV_ERRNO(_msg, _fmt, ...) \
	BT_COMP_LOG_ERRNO(BT_LOG_VERBOSE, (BT_COMP_LOG_SELF_COMP), _msg, _fmt, ##__VA_ARGS__)
#define BT_COMP_LOGF_MEM(_data_ptr, _data_sz, _fmt, ...) \
	BT_COMP_LOG_MEM(BT_LOG_FATAL, (BT_COMP_LOG_SELF_COMP), (_data_ptr), (_data_sz), _fmt, ##__VA_ARGS__)
#define BT_COMP_LOGE_MEM(_data_ptr, _data_sz, _fmt, ...) \
	BT_COMP_LOG_MEM(BT_LOG_ERROR, (BT_COMP_LOG_SELF_COMP), (_data_ptr), (_data_sz), _fmt, ##__VA_ARGS__)
#define BT_COMP_LOGW_MEM(_data_ptr, _data_sz, _fmt, ...) \
	BT_COMP_LOG_MEM(BT_LOG_WARN, (BT_COMP_LOG_SELF_COMP), (_data_ptr), (_data_sz), _fmt, ##__VA_ARGS__)
#define BT_COMP_LOGI_MEM(_data_ptr, _data_sz, _fmt, ...) \
	BT_COMP_LOG_MEM(BT_LOG_INFO, (BT_COMP_LOG_SELF_COMP), (_data_ptr), (_data_sz), _fmt, ##__VA_ARGS__)
#define BT_COMP_LOGD_MEM(_data_ptr, _data_sz, _fmt, ...) \
	BT_COMP_LOG_MEM(BT_LOG_DEBUG, (BT_COMP_LOG_SELF_COMP), (_data_ptr), (_data_sz), _fmt, ##__VA_ARGS__)
#define BT_COMP_LOGV_MEM(_data_ptr, _data_sz, _fmt, ...) \
	BT_COMP_LOG_MEM(BT_LOG_VERBOSE, (BT_COMP_LOG_SELF_COMP), (_data_ptr), (_data_sz), _fmt, ##__VA_ARGS__)

#define BT_COMP_LOG_SUPPORTED

#endif /* BABELTRACE_PLUGINS_COMP_LOGGING_H */
