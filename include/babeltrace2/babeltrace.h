#ifndef BABELTRACE2_BABELTRACE_H
#define BABELTRACE2_BABELTRACE_H

/*
 * Copyright (c) 2010-2019 EfficiOS Inc. and Linux Foundation
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

/*
 * Tell the specific headers that they are included from this header.
 *
 * Do NOT define `__BT_IN_BABELTRACE_H` in user code.
 */
#ifndef __BT_IN_BABELTRACE_H
# define __BT_IN_BABELTRACE_H
#endif

/* Internal: needed by some of the following included headers */
#include <babeltrace2/func-status.h>

/* Internal: needed by some of the following included headers */
#ifdef __cplusplus
# define __BT_UPCAST(_type, _p)		static_cast<_type *>(static_cast<void *>(_p))
# define __BT_UPCAST_CONST(_type, _p)	static_cast<const _type *>(static_cast<const void *>(_p))
#else
# define __BT_UPCAST(_type, _p)		((_type *) (_p))
# define __BT_UPCAST_CONST(_type, _p)	((const _type *) (_p))
#endif

#include <babeltrace2/error-reporting.h>
#include <babeltrace2/graph/component-class-dev.h>
#include <babeltrace2/graph/component-class.h>
#include <babeltrace2/graph/component-descriptor-set.h>
#include <babeltrace2/graph/component.h>
#include <babeltrace2/graph/connection.h>
#include <babeltrace2/graph/graph.h>
#include <babeltrace2/graph/interrupter.h>
#include <babeltrace2/graph/message-iterator.h>
#include <babeltrace2/graph/message.h>
#include <babeltrace2/graph/port.h>
#include <babeltrace2/graph/private-query-executor.h>
#include <babeltrace2/graph/query-executor.h>
#include <babeltrace2/graph/self-component-class.h>
#include <babeltrace2/graph/self-component-port.h>
#include <babeltrace2/graph/self-component.h>
#include <babeltrace2/graph/self-message-iterator.h>
#include <babeltrace2/integer-range-set.h>
#include <babeltrace2/logging.h>
#include <babeltrace2/plugin/plugin-dev.h>
#include <babeltrace2/plugin/plugin-loading.h>
#include <babeltrace2/trace-ir/clock-class.h>
#include <babeltrace2/trace-ir/clock-snapshot.h>
#include <babeltrace2/trace-ir/event-class.h>
#include <babeltrace2/trace-ir/event.h>
#include <babeltrace2/trace-ir/field-class.h>
#include <babeltrace2/trace-ir/field-path.h>
#include <babeltrace2/trace-ir/field.h>
#include <babeltrace2/trace-ir/packet.h>
#include <babeltrace2/trace-ir/stream-class.h>
#include <babeltrace2/trace-ir/stream.h>
#include <babeltrace2/trace-ir/trace-class.h>
#include <babeltrace2/trace-ir/trace.h>
#include <babeltrace2/types.h>
#include <babeltrace2/util.h>
#include <babeltrace2/value.h>
#include <babeltrace2/version.h>

/* Cancel private definitions */
#undef __BT_FUNC_STATUS_AGAIN
#undef __BT_FUNC_STATUS_END
#undef __BT_FUNC_STATUS_ERROR
#undef __BT_FUNC_STATUS_INTERRUPTED
#undef __BT_FUNC_STATUS_UNKNOWN_OBJECT
#undef __BT_FUNC_STATUS_MEMORY_ERROR
#undef __BT_FUNC_STATUS_NOT_FOUND
#undef __BT_FUNC_STATUS_OK
#undef __BT_FUNC_STATUS_OVERFLOW_ERROR
#undef __BT_FUNC_STATUS_USER_ERROR
#undef __BT_IN_BABELTRACE_H
#undef __BT_UPCAST
#undef __BT_UPCAST_CONST
#undef __BT_LOGGING_LEVEL_TRACE
#undef __BT_LOGGING_LEVEL_DEBUG
#undef __BT_LOGGING_LEVEL_INFO
#undef __BT_LOGGING_LEVEL_WARNING
#undef __BT_LOGGING_LEVEL_ERROR
#undef __BT_LOGGING_LEVEL_FATAL
#undef __BT_LOGGING_LEVEL_NONE

#endif /* BABELTRACE2_BABELTRACE_H */
