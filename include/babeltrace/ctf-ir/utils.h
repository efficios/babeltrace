#ifndef BABELTRACE_CTF_IR_UTILS_H
#define BABELTRACE_CTF_IR_UTILS_H

/*
 * BabelTrace - CTF IR: Utilities
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
 *
 * The Common Trace Format (CTF) Specification is available at
 * http://www.efficios.com/ctf
 */

#ifdef __cplusplus
extern "C" {
#endif

/*
 * bt_ctf_validate_identifier: validate an identifier against the CTF spec.
 *
 * Validate that an identifier meets the CTF specification's restrictions on
 * identifiers. An identifier will be rejected if it is a keyword defined
 * in the CTF specification or if it does not meet any other requirement imposed
 * on identifiers.
 *
 * Note that this will not check whether or not the identifier clashes with
 * identifiers used in a given trace.
 *
 * Returns 0 if the identifier is valid, a negative value on error.
 */
extern int bt_ctf_validate_identifier(const char *identifier);

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_CTF_IR_UTILS_H */
