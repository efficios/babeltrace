#ifndef BABELTRACE_CTF_WRITER_VISITOR_H
#define BABELTRACE_CTF_WRITER_VISITOR_H

/*
 * Copyright 2016 Jérémie Galarneau <jeremie.galarneau@efficios.com>
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

#ifdef __cplusplus
extern "C" {
#endif

/**
@defgroup ctfirvisitor CTF IR visitor
@ingroup ctfir
@brief CTF IR visitor.

@code
#include <babeltrace/trace-ir/visitor.h>
@endcode

A CTF IR <strong><em>visitor</em></strong> is a function that you
can use to visit the hierarchy of a
\link ctfirtraceclass CTF IR trace class\endlink with
bt_ctf_trace_visit() or of a
\link ctfirstreamclass CTF IR stream class\endlink with
bt_ctf_stream_class_visit().

The traversal of the object's hierarchy is always done in a
pre-order fashion.

@sa ctfirstreamclass
@sa ctfirtraceclass

@file
@brief CTF IR visitor types and functions.
@sa ctfirvisitor

@addtogroup ctfirvisitor
@{
*/

/**
@struct bt_ctf_object
@brief A CTF IR object wrapper.

This structure wraps both a CTF IR object and its type
(see #bt_ctf_object_type). It is used in the visiting function.

You can use the bt_ctf_object_get_type() and
bt_ctf_object_get_object() accessors to get the type and wrapped
CTF IR object of a CTF IR object wrapper.

A CTF IR object wrapper has <strong>no reference count</strong>: do \em
not use bt_ctf_object_put_ref() or bt_ctf_object_get_ref() on it.

@sa ctfirvisitor
*/
struct bt_ctf_visitor_object;

/**
@brief	CTF IR object wrapper type.
*/
enum bt_ctf_visitor_object_type {
	/// Unknown (used for errors).
	BT_CTF_VISITOR_OBJECT_TYPE_UNKNOWN = -1,

	/// \ref ctfirtraceclass.
	BT_CTF_VISITOR_OBJECT_TYPE_TRACE = 0,

	/// \ref ctfirstreamclass.
	BT_CTF_VISITOR_OBJECT_TYPE_STREAM_CLASS = 1,

	/// \ref ctfirstream.
	BT_CTF_VISITOR_OBJECT_TYPE_STREAM = 2,

	/// \ref ctfireventclass.
	BT_CTF_VISITOR_OBJECT_TYPE_EVENT_CLASS = 3,

	/// \ref ctfirevent.
	BT_CTF_VISITOR_OBJECT_TYPE_EVENT = 4,

	/// Number of entries in this enumeration.
	BT_CTF_VISITOR_OBJECT_TYPE_NR,
};

/**
@brief	Visting function type.

A function of this type is called by bt_ctf_trace_visit() or
bt_ctf_stream_class_visit() when visiting the CTF IR object wrapper
\p object.

\p object has <strong>no reference count</strong>: do \em not use
bt_ctf_object_put_ref() or bt_ctf_object_get_ref() on it.

@param[in] object	Currently visited CTF IR object wrapper.
@param[in] data		User data.
@returns		0 on success, or a negative value on error.

@prenotnull{object}

@sa bt_ctf_trace_visit(): Accepts a visitor to visit a trace class.
@sa bt_ctf_stream_class_visit(): Accepts a visitor to visit a stream
	class.
*/
typedef int (*bt_ctf_visitor)(struct bt_ctf_visitor_object *object,
		void *data);

/**
@brief	Returns the type of the CTF IR object wrapped by the CTF IR
	object wrapper \p object.

@param[in] object	Object wrapper of which to get the type.
@returns		Type of \p object.

@prenotnull{object}

@sa bt_ctf_visitor_object_get_object(): Returns the object wrapped by a given
	CTF IR object wrapper.
*/
enum bt_ctf_visitor_object_type bt_ctf_visitor_object_get_type(
		struct bt_ctf_visitor_object *object);

/**
@brief	Returns the CTF IR object wrapped by the CTF IR object
	wrapper \p object.

The reference count of \p object is \em not incremented by this
function. On success, you must call bt_ctf_object_get_ref() on the return value to
have your own reference.

@param[in] object	Object wrapper of which to get the wrapped
			CTF IR object.
@returns		CTF IR object wrapped by \p object.

@prenotnull{object}
@post The reference count of the returned object is not modified.

@sa bt_ctf_visitor_object_get_type(): Returns the type of a given
	CTF IR object wrapper.
*/
void *bt_ctf_visitor_object_get_object(struct bt_ctf_visitor_object *object);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_CTF_WRITER_VISITOR_H */
