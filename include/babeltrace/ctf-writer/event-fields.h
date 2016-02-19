#ifndef BABELTRACE_CTF_WRITER_EVENT_FIELDS_H
#define BABELTRACE_CTF_WRITER_EVENT_FIELDS_H

/*
 * BabelTrace - CTF Writer: Event Fields
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

#include <babeltrace/ctf-ir/fields.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * bt_ctf_field_get and bt_ctf_field_put: increment and decrement the
 * field's reference count.
 *
 * You may also use bt_ctf_get() and bt_ctf_put() with field objects.
 *
 * These functions ensure that the field won't be destroyed when it
 * is in use. The same number of get and put (plus one extra put to
 * release the initial reference done at creation) have to be done to
 * destroy a field.
 *
 * When the field's reference count is decremented to 0 by a bt_ctf_field_put,
 * the field is freed.
 *
 * @param field Field instance.
 */
extern void bt_ctf_field_get(struct bt_ctf_field *field);
extern void bt_ctf_field_put(struct bt_ctf_field *field);

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_CTF_WRITER_EVENT_FIELDS_H */
