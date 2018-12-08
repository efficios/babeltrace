#ifndef BABELTRACE_GRAPH_NOTIFICATION_CONST_H
#define BABELTRACE_GRAPH_NOTIFICATION_CONST_H

/*
 * Copyright 2017-2018 Philippe Proulx <pproulx@efficios.com>
 * Copyright 2015 Jérémie Galarneau <jeremie.galarneau@efficios.com>
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

/* For bt_notification */
#include <babeltrace/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Notification types. Unhandled notification types should be ignored.
 */
enum bt_notification_type {
	BT_NOTIFICATION_TYPE_EVENT =			0,
	BT_NOTIFICATION_TYPE_INACTIVITY =		1,
	BT_NOTIFICATION_TYPE_STREAM_BEGINNING =		2,
	BT_NOTIFICATION_TYPE_STREAM_END =		3,
	BT_NOTIFICATION_TYPE_PACKET_BEGINNING =		4,
	BT_NOTIFICATION_TYPE_PACKET_END =		5,
};

/**
 * Get a notification's type.
 *
 * @param notification	Notification instance
 * @returns		One of #bt_notification_type
 */
extern enum bt_notification_type bt_notification_get_type(
		const bt_notification *notification);

extern void bt_notification_get_ref(const bt_notification *notification);

extern void bt_notification_put_ref(const bt_notification *notification);

#define BT_NOTIFICATION_PUT_REF_AND_RESET(_var)		\
	do {						\
		bt_notification_put_ref(_var);		\
		(_var) = NULL;				\
	} while (0)

#define BT_NOTIFICATION_MOVE_REF(_var_dst, _var_src)	\
	do {						\
		bt_notification_put_ref(_var_dst);	\
		(_var_dst) = (_var_src);		\
		(_var_src) = NULL;			\
	} while (0)

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_GRAPH_NOTIFICATION_CONST_H */
