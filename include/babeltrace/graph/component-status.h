#ifndef BABELTRACE_GRAPH_COMPONENT_STATUS_H
#define BABELTRACE_GRAPH_COMPONENT_STATUS_H

/*
 * BabelTrace - Babeltrace Component Interface
 *
 * Copyright 2015 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 *
 * Author: Jérémie Galarneau <jeremie.galarneau@efficios.com>
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

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Status code. Errors are always negative.
 */
enum bt_component_status {
	/** No error, okay. */
	BT_COMPONENT_STATUS_OK				= 0,
	/** No more work to be done by this component. **/
	BT_COMPONENT_STATUS_END				= 1,
	/**
	 * Component can't process a notification at this time
	 * (e.g. would block), try again later.
	 */
	BT_COMPONENT_STATUS_AGAIN			= 11,
	/** Refuse port connection. */
	BT_COMPONENT_STATUS_REFUSE_PORT_CONNECTION	= 111,
	/** General error. */
	BT_COMPONENT_STATUS_ERROR			= -1,
	/** Unsupported component feature. */
	BT_COMPONENT_STATUS_UNSUPPORTED			= -2,
	/** Invalid arguments. */
	BT_COMPONENT_STATUS_INVALID			= -22,
	/** Memory allocation failure. */
	BT_COMPONENT_STATUS_NOMEM			= -12,
	/** Element not found. */
	BT_COMPONENT_STATUS_NOT_FOUND			= -19,
};

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_GRAPH_COMPONENT_STATUS_H */
