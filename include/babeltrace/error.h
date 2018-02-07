#ifndef BABELTRACE_ERROR_H
#define BABELTRACE_ERROR_H

/*
 * BabelTrace
 *
 * Global error accessors.
 *
 * Copyright 2018 EfficiOS Inc. and Linux Foundation
 *
 * Author: Jérémie Galarneau <jeremie.galarneau@efficios.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

enum bt_packet_seek_error {
	BT_PACKET_SEEK_ERROR = 1,
	BT_PACKET_SEEK_ERROR_TRUNCATED_PACKET = 2,
};

/*
 * bt_packet_seek_get_error: get the return code of the last packet_seek use.
 *
 * The packet seek functions defined by the various formats don't return a
 * value. An implementation that can fail to seek can set a per-thread
 * packet_seek error to be checked by the caller.
 *
 * The error is cleared after access in order to preserve the compatibility
 * with implementation that don't set an error.
 */
int bt_packet_seek_get_error(void);

/*
 * bt_packet_seek_set_error: set the return code of the last packet_seek use.
 *
 * This function sets the per-thread packet_seek error. A value of 0 indicates
 * no error. Implementations of packet_seek are encouraged to use a negative
 * value to indicate an error.
 */
void bt_packet_seek_set_error(int error);

#endif /* BABELTRACE_ERROR_H */
