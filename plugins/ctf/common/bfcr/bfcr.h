#ifndef CTF_BFCR_H
#define CTF_BFCR_H

/*
 * Babeltrace - CTF binary field class reader (BFCR)
 *
 * Copyright (c) 2015-2016 EfficiOS Inc. and Linux Foundation
 * Copyright (c) 2015-2016 Philippe Proulx <pproulx@efficios.com>
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

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <babeltrace/babeltrace.h>
#include <babeltrace/babeltrace-internal.h>

#include "../metadata/ctf-meta.h"

/**
 * @file bfcr.h
 *
 * Event-driven CTF binary field class reader (BFCR).
 *
 * This is a common, internal API used by CTF source plugins. It allows
 * a binary CTF IR field class to be decoded from user-provided buffers.
 * As the class is decoded (and, possibly, its nested classes),
 * registered user callback functions are called.
 *
 * This API is only concerned with reading one CTF class at a time from
 * one or more buffer of bytes. It does not know CTF dynamic scopes,
 * events, or streams. Sequence lengths and selected variant classes are
 * requested to the user when needed.
 */

/**
 * Binary class reader API status codes.
 */
enum bt_bfcr_status {
	/** Out of memory. */
	BT_BFCR_STATUS_ENOMEM =		-5,
	/**
	 * The binary stream reader reached the end of the user-provided
	 * buffer, but data is still needed to finish decoding the
	 * requested class.
	 *
	 * The user needs to call bt_bfcr_continue() as long as
	 * #BT_BFCR_STATUS_EOF is returned to complete the decoding
	 * process of a given class.
	 */
	BT_BFCR_STATUS_EOF =		1,

	/** Invalid argument. */
	BT_BFCR_STATUS_INVAL =		-3,

	/** General error. */
	BT_BFCR_STATUS_ERROR =		-1,

	/** Everything okay. */
	BT_BFCR_STATUS_OK =		0,
};

/** Field class reader. */
struct bt_bfcr;

typedef enum bt_bfcr_status (* bt_bfcr_unsigned_int_cb_func)(uint64_t,
		struct ctf_field_class *, void *);

/*
 * Field class reader user callback functions.
 */
struct bt_bfcr_cbs {
	/**
	 * Field class callback functions.
	 *
	 * This CTF binary class reader is event-driven. The following
	 * functions are called during the decoding process, either when
	 * a compound class begins/ends, or when a basic class is
	 * completely decoded (along with its value).
	 *
	 * Each function also receives the CTF field class associated
	 * with the call, and user data (registered to the class reader
	 * calling them).
	 *
	 * Actual trace IR fields are \em not created here; this would
	 * be the responsibility of a class reader's user (the provider
	 * of those callback functions).
	 *
	 * All the class callback functions return one of the following
	 * values:
	 *
	 *   - <b>#BT_BFCR_STATUS_OK</b>: Everything is okay;
	 *     continue the decoding process.
	 *   - <b>#BT_BFCR_STATUS_ERROR</b>: General error (reported
	 *     to class reader's user).
	 *
	 * Any member of this structure may be set to \c NULL, should
	 * a specific notification be not needed.
	 */
	struct {
		/**
		 * Called when a signed integer class is completely
		 * decoded. This could also be the supporting signed
		 * integer class of an enumeration class (\p class will
		 * indicate this).
		 *
		 * @param value		Signed integer value
		 * @param class		Integer or enumeration class
		 * @param data		User data
		 * @returns		#BT_BFCR_STATUS_OK or
		 *			#BT_BFCR_STATUS_ERROR
		 */
		enum bt_bfcr_status (* signed_int)(int64_t value,
				struct ctf_field_class *cls, void *data);

		/**
		 * Called when an unsigned integer class is completely
		 * decoded. This could also be the supporting signed
		 * integer class of an enumeration class (\p class will
		 * indicate this).
		 *
		 * @param value		Unsigned integer value
		 * @param class		Integer or enumeration class
		 * @param data		User data
		 * @returns		#BT_BFCR_STATUS_OK or
		 *			#BT_BFCR_STATUS_ERROR
		 */
		bt_bfcr_unsigned_int_cb_func unsigned_int;

		/**
		 * Called when a floating point number class is
		 * completely decoded.
		 *
		 * @param value		Floating point number value
		 * @param class		Floating point number class
		 * @param data		User data
		 * @returns		#BT_BFCR_STATUS_OK or
		 *			#BT_BFCR_STATUS_ERROR
		 */
		enum bt_bfcr_status (* floating_point)(double value,
				struct ctf_field_class *cls, void *data);

		/**
		 * Called when a string class begins.
		 *
		 * All the following user callback function calls will
		 * be made to bt_bfcr_cbs::classes::string(), each of
		 * them providing one substring of the complete string
		 * class's value.
		 *
		 * @param class		Beginning string class
		 * @param data		User data
		 * @returns		#BT_BFCR_STATUS_OK or
		 *			#BT_BFCR_STATUS_ERROR
		 */
		enum bt_bfcr_status (* string_begin)(
				struct ctf_field_class *cls, void *data);

		/**
		 * Called when a string class's substring is decoded
		 * (between a call to bt_bfcr_cbs::classes::string_begin()
		 * and a call to bt_bfcr_cbs::classes::string_end()).
		 *
		 * @param value		String value (\em not null-terminated)
		 * @param len		String value length
		 * @param class		String class
		 * @param data		User data
		 * @returns		#BT_BFCR_STATUS_OK or
		 *			#BT_BFCR_STATUS_ERROR
		 */
		enum bt_bfcr_status (* string)(const char *value,
				size_t len, struct ctf_field_class *cls,
				void *data);

		/**
		 * Called when a string class ends.
		 *
		 * @param class		Ending string class
		 * @param data		User data
		 * @returns		#BT_BFCR_STATUS_OK or
		 *			#BT_BFCR_STATUS_ERROR
		 */
		enum bt_bfcr_status (* string_end)(
				struct ctf_field_class *cls, void *data);

		/**
		 * Called when a compound class begins.
		 *
		 * All the following class callback function calls will
		 * signal sequential elements of this compound class,
		 * until the next corresponding
		 * bt_bfcr_cbs::classes::compound_end() is called.
		 *
		 * If \p class is a variant class, then only one class
		 * callback function call will follow before the call to
		 * bt_bfcr_cbs::classes::compound_end(). This single
		 * call indicates the selected class of this variant
		 * class.
		 *
		 * @param class		Beginning compound class
		 * @param data		User data
		 * @returns		#BT_BFCR_STATUS_OK or
		 *			#BT_BFCR_STATUS_ERROR
		 */
		enum bt_bfcr_status (* compound_begin)(
				struct ctf_field_class *cls, void *data);

		/**
		 * Called when a compound class ends.
		 *
		 * @param class		Ending compound class
		 * @param data		User data
		 * @returns		#BT_BFCR_STATUS_OK or
		 *			#BT_BFCR_STATUS_ERROR
		 */
		enum bt_bfcr_status (* compound_end)(
				struct ctf_field_class *cls, void *data);
	} classes;

	/**
	 * Query callback functions are used when the class reader needs
	 * dynamic information, i.e. a sequence class's current length
	 * or a variant class's current selected class.
	 *
	 * Both functions need to be set unless it is known that no
	 * sequences or variants will have to be decoded.
	 */
	struct {
		/**
		 * Called to query the current length of a given sequence
		 * class.
		 *
		 * @param class		Sequence class
		 * @param data		User data
		 * @returns		Sequence length or
		 *			#BT_BFCR_STATUS_ERROR on error
		 */
		int64_t (* get_sequence_length)(struct ctf_field_class *cls,
				void *data);

		/**
		 * Called to query the current selected class of a given
		 * variant class.
		 *
		 * @param class		Variant class
		 * @param data		User data
		 * @returns		Current selected class (owned by
		 *			this) or \c NULL on error
		 */
		struct ctf_field_class * (* borrow_variant_selected_field_class)(
				struct ctf_field_class *cls, void *data);
	} query;
};

/**
 * Creates a CTF binary class reader.
 *
 * @param cbs		User callback functions
 * @param data		User data (passed to user callback functions)
 * @returns		New binary class reader on success, or \c NULL on error
 */
BT_HIDDEN
struct bt_bfcr *bt_bfcr_create(struct bt_bfcr_cbs cbs, void *data);

/**
 * Destroys a CTF binary class reader, freeing all internal resources.
 *
 * @param bfcr	Binary class reader
 */
BT_HIDDEN
void bt_bfcr_destroy(struct bt_bfcr *bfcr);

/**
 * Decodes a given CTF class from a buffer of bytes.
 *
 * The number of \em bits consumed by this function is returned.
 *
 * The \p status output parameter is where a status is written, amongst
 * the following:
 *
 *   - <b>#BT_BFCR_STATUS_OK</b>: Decoding is done.
 *   - <b>#BT_BFCR_STATUS_EOF</b>: The end of the buffer was reached,
 *     but more data is needed to finish the decoding process of the
 *     requested class. The user needs to call bt_bfcr_continue()
 *     as long as #BT_BFCR_STATUS_EOF is returned to complete the
 *     decoding process of the original class.
 *   - <b>#BT_BFCR_STATUS_INVAL</b>: Invalid argument.
 *   - <b>#BT_BFCR_STATUS_ERROR</b>: General error.
 *
 * Calling this function resets the class reader's internal state. If
 * #BT_BFCR_STATUS_EOF is returned, bt_bfcr_continue() needs to
 * be called next, \em not bt_bfcr_decode().
 *
 * @param bfcr			Binary class reader
 * @param class			Field class to decode
 * @param buf			Buffer
 * @param offset		Offset of first bit from \p buf (bits)
 * @param packet_offset		Offset of \p offset within the CTF
 *				binary packet containing \p class (bits)
 * @param sz			Size of buffer in bytes (from \p buf)
 * @param status		Returned status (see description above)
 * @returns			Number of consumed bits
 */
BT_HIDDEN
size_t bt_bfcr_start(struct bt_bfcr *bfcr,
		struct ctf_field_class *cls, const uint8_t *buf,
		size_t offset, size_t packet_offset, size_t sz,
		enum bt_bfcr_status *status);

/**
 * Continues the decoding process a given CTF class.
 *
 * The number of bits consumed by this function is returned.
 *
 * The \p status output parameter is where a status is placed, amongst
 * the following:
 *
 *   - <b>#BT_BFCR_STATUS_OK</b>: decoding is done.
 *   - <b>#BT_BFCR_STATUS_EOF</b>: the end of the buffer was reached,
 *     but more data is needed to finish the decoding process of the
 *     requested class. The user needs to call bt_bfcr_continue()
 *     as long as #BT_BFCR_STATUS_EOF is returned to complete the
 *     decoding process of the original class.
 *   - <b>#BT_BFCR_STATUS_INVAL</b>: invalid argument.
 *   - <b>#BT_BFCR_STATUS_ERROR</b>: general error.
 *
 * @param bfcr		Binary class reader
 * @param buf		Buffer
 * @param sz		Size of buffer in bytes (from \p offset)
 * @param status	Returned status (see description above)
 * @returns		Number of consumed bits
 */
BT_HIDDEN
size_t bt_bfcr_continue(struct bt_bfcr *bfcr,
		const uint8_t *buf, size_t sz,
		enum bt_bfcr_status *status);

BT_HIDDEN
void bt_bfcr_set_unsigned_int_cb(struct bt_bfcr *bfcr,
		bt_bfcr_unsigned_int_cb_func cb);

static inline
const char *bt_bfcr_status_string(enum bt_bfcr_status status)
{
	switch (status) {
	case BT_BFCR_STATUS_ENOMEM:
		return "BT_BFCR_STATUS_ENOMEM";
	case BT_BFCR_STATUS_EOF:
		return "BT_BFCR_STATUS_EOF";
	case BT_BFCR_STATUS_INVAL:
		return "BT_BFCR_STATUS_INVAL";
	case BT_BFCR_STATUS_ERROR:
		return "BT_BFCR_STATUS_ERROR";
	case BT_BFCR_STATUS_OK:
		return "BT_BFCR_STATUS_OK";
	default:
		return "(unknown)";
	}
}

#endif /* CTF_BFCR_H */
