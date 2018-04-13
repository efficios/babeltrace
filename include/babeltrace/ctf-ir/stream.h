#ifndef BABELTRACE_CTF_IR_STREAM_H
#define BABELTRACE_CTF_IR_STREAM_H

/*
 * BabelTrace - CTF IR: Stream
 *
 * Copyright 2013, 2014 Jérémie Galarneau <jeremie.galarneau@efficios.com>
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
 *
 * The Common Trace Format (CTF) Specification is available at
 * http://www.efficios.com/ctf
 */

/* For bt_get() */
#include <babeltrace/ref.h>

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct bt_stream_class;

/**
@defgroup ctfirstream CTF IR stream
@ingroup ctfir
@brief CTF IR stream.

@code
#include <babeltrace/ctf-ir/stream.h>
@endcode

@note
See \ref ctfwriterstream which documents additional CTF IR stream
functions exclusive to the CTF writer mode.

A CTF IR <strong><em>stream</em></strong> is an instance of a
\link ctfirstreamclass CTF IR stream class\endlink.

You can obtain a CTF IR stream object in two different modes:

- <strong>Normal mode</strong>: use bt_stream_create() or
  bt_stream_create_with_id() with a stream class having a
  \link ctfirtraceclass CTF IR trace class\endlink parent
  \em not created by a \link ctfwriter CTF writer\endlink object to
  create a default stream.
- <strong>CTF writer mode</strong>: use bt_stream_create() with
  a stream class having a trace class parent created by a CTF writer
  object, or use bt_writer_create_stream().

A CTF IR stream object represents a CTF stream, that is, a sequence of
packets containing events:

@imgtracestructure

A CTF IR stream does not contain, however, actual \link ctfirpacket CTF
IR packet\endlink objects: it only acts as a common parent to identify
the original CTF stream of packet objects.

As with any Babeltrace object, CTF IR stream objects have
<a href="https://en.wikipedia.org/wiki/Reference_counting">reference
counts</a>. See \ref refs to learn more about the reference counting
management of Babeltrace objects.

@sa ctfirstreamclass
@sa ctfirpacket
@sa ctfwriterstream

@file
@brief CTF IR stream type and functions.
@sa ctfirstream

@addtogroup ctfirstream
@{
*/

/**
@struct bt_stream
@brief A CTF IR stream.
@sa ctfirstream
@sa ctfwriterstream
*/
struct bt_stream;
struct bt_event;

/**
@brief  Creates a default CTF IR stream named \p name with ID \p id
	from the CTF IR stream class \p stream_class.

\p stream_class \em must have a parent
\link ctfirtraceclass CTF IR trace class\endlink.

\p id \em must be unique amongst the IDs of all the streams created
from \p stream_class with bt_stream_create_with_id().

\p name can be \c NULL to create an unnamed stream object.

@param[in] stream_class	CTF IR stream class to use to create the
			CTF IR stream.
@param[in] name		Name of the stream object to create (copied on
			success) or \c NULL to create an unnamed stream.
@param[in] id		ID of the stream object to create.
@returns		Created stream object, or \c NULL on error.

@prenotnull{stream_class}
@pre \p id is lesser than or equal to 9223372036854775807 (\c INT64_MAX).
@pre \p stream_class has a parent trace class.
@postsuccessrefcountret1
*/
extern struct bt_stream *bt_stream_create(struct bt_stream_class *stream_class,
		const char *name, uint64_t id);

/**
@brief	Returns the name of the CTF IR stream \p stream.

On success, \p stream remains the sole owner of the returned string.

@param[in] stream	Stream object of which to get the name.
@returns		Name of stream \p stream, or \c NULL if
			\p stream is unnamed or on error.

@prenotnull{stream}
@postrefcountsame{stream}
*/
extern const char *bt_stream_get_name(struct bt_stream *stream);

/**
@brief	Returns the numeric ID of the CTF IR stream \p stream.

@param[in] stream	Stream of which to get the numeric ID.
@returns		ID of stream \p stream, or a negative value
			on error.

@prenotnull{stream}
@postrefcountsame{stream}
*/
extern int64_t bt_stream_get_id(struct bt_stream *stream);

extern struct bt_stream_class *bt_stream_borrow_class(
		struct bt_stream *stream);

/**
@brief	Returns the parent CTF IR stream class of the CTF IR
	stream \p stream.

This function returns a reference to the stream class which was used
to create the stream object in the first place with
bt_stream_create().

@param[in] stream	Stream of which to get the parent stream class.
@returns		Parent stream class of \p stream,
			or \c NULL on error.

@prenotnull{stream}
@postrefcountsame{stream}
@postsuccessrefcountretinc
*/
static inline
struct bt_stream_class *bt_stream_get_class(
		struct bt_stream *stream)
{
	return bt_get(bt_stream_borrow_class(stream));
}

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_CTF_IR_STREAM_H */
