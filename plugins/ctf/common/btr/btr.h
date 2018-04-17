#ifndef CTF_BTR_H
#define CTF_BTR_H

/*
 * Babeltrace - CTF binary type reader (BTR)
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

/**
 * @file ctf-btr.h
 *
 * Event-driven CTF binary type reader (BTR).
 *
 * This is a common, internal API used by CTF source plugins. It allows
 * a binary CTF IR field type to be decoded from user-provided buffers.
 * As the type is decoded (and, possibly, its nested types), registered
 * user callback functions are called.
 *
 * This API is only concerned with reading one CTF type at a time from
 * one or more buffer of bytes. It does not know CTF dynamic scopes,
 * events, or streams. Sequence lengths and selected variant types are
 * requested to the user when needed.
 */

/**
 * Binary type reader API status codes.
 */
enum bt_btr_status {
	/** Out of memory. */
	BT_BTR_STATUS_ENOMEM =	-5,
	/**
	 * The binary stream reader reached the end of the user-provided
	 * buffer, but data is still needed to finish decoding the
	 * requested type.
	 *
	 * The user needs to call bt_btr_continue() as long as
	 * #BT_BTR_STATUS_EOF is returned to complete the decoding
	 * process of a given type.
	 */
	BT_BTR_STATUS_EOF =		1,

	/** Invalid argument. */
	BT_BTR_STATUS_INVAL =	-3,

	/** General error. */
	BT_BTR_STATUS_ERROR =	-1,

	/** Everything okay. */
	BT_BTR_STATUS_OK =		0,
};

/** Type reader. */
struct bt_btr;

/*
 * Type reader user callback functions.
 */
struct bt_btr_cbs {
	/**
	 * Type callback functions.
	 *
	 * This CTF binary type reader is event-driven. The following
	 * functions are called during the decoding process, either when
	 * a compound type begins/ends, or when a basic type is
	 * completely decoded (along with its value).
	 *
	 * Each function also receives the CTF IR field type associated
	 * with the call, and user data (registered to the type reader
	 * calling them). This field type is a weak reference; the
	 * callback function must use bt_field_type_get() to keep
	 * its own reference of it.
	 *
	 * Actual CTF IR fields are \em not created here; this would be
	 * the responsibility of a type reader's user (the provider of
	 * those callback functions).
	 *
	 * All the type callback functions return one of the following
	 * values:
	 *
	 *   - <b>#BT_BTR_STATUS_OK</b>: Everything is okay;
	 *     continue the decoding process.
	 *   - <b>#BT_BTR_STATUS_ERROR</b>: General error (reported
	 *     to type reader's user).
	 *
	 * Any member of this structure may be set to \c NULL, should
	 * a specific notification be not needed.
	 */
	struct {
		/**
		 * Called when a signed integer type is completely
		 * decoded. This could also be the supporting signed
		 * integer type of an enumeration type (\p type will
		 * indicate this).
		 *
		 * @param value		Signed integer value
		 * @param type		Integer or enumeration type (weak
		 *			reference)
		 * @param data		User data
		 * @returns		#BT_BTR_STATUS_OK or
		 *			#BT_BTR_STATUS_ERROR
		 */
		enum bt_btr_status (* signed_int)(int64_t value,
				struct bt_field_type *type, void *data);

		/**
		 * Called when an unsigned integer type is completely
		 * decoded. This could also be the supporting signed
		 * integer type of an enumeration type (\p type will
		 * indicate this).
		 *
		 * @param value		Unsigned integer value
		 * @param type		Integer or enumeration type (weak
		 *			reference)
		 * @param data		User data
		 * @returns		#BT_BTR_STATUS_OK or
		 *			#BT_BTR_STATUS_ERROR
		 */
		enum bt_btr_status (* unsigned_int)(uint64_t value,
				struct bt_field_type *type, void *data);

		/**
		 * Called when a floating point number type is
		 * completely decoded.
		 *
		 * @param value		Floating point number value
		 * @param type		Floating point number type (weak
		 *			reference)
		 * @param data		User data
		 * @returns		#BT_BTR_STATUS_OK or
		 *			#BT_BTR_STATUS_ERROR
		 */
		enum bt_btr_status (* floating_point)(double value,
				struct bt_field_type *type, void *data);

		/**
		 * Called when a string type begins.
		 *
		 * All the following user callback function calls will
		 * be made to bt_btr_cbs::types::string(), each of
		 * them providing one substring of the complete string
		 * type's value.
		 *
		 * @param type		Beginning string type (weak reference)
		 * @param data		User data
		 * @returns		#BT_BTR_STATUS_OK or
		 *			#BT_BTR_STATUS_ERROR
		 */
		enum bt_btr_status (* string_begin)(
				struct bt_field_type *type, void *data);

		/**
		 * Called when a string type's substring is decoded
		 * (between a call to bt_btr_cbs::types::string_begin()
		 * and a call to bt_btr_cbs::types::string_end()).
		 *
		 * @param value		String value (\em not null-terminated)
		 * @param len		String value length
		 * @param type		String type (weak reference)
		 * @param data		User data
		 * @returns		#BT_BTR_STATUS_OK or
		 *			#BT_BTR_STATUS_ERROR
		 */
		enum bt_btr_status (* string)(const char *value,
				size_t len, struct bt_field_type *type,
				void *data);

		/**
		 * Called when a string type ends.
		 *
		 * @param type		Ending string type (weak reference)
		 * @param data		User data
		 * @returns		#BT_BTR_STATUS_OK or
		 *			#BT_BTR_STATUS_ERROR
		 */
		enum bt_btr_status (* string_end)(
				struct bt_field_type *type, void *data);

		/**
		 * Called when a compound type begins.
		 *
		 * All the following type callback function calls will
		 * signal sequential elements of this compound type,
		 * until the next corresponding
		 * bt_btr_cbs::types::compound_end() is called.
		 *
		 * If \p type is a variant type, then only one type
		 * callback function call will follow before the call to
		 * bt_btr_cbs::types::compound_end(). This single
		 * call indicates the selected type of this variant
		 * type.
		 *
		 * @param type		Beginning compound type (weak reference)
		 * @param data		User data
		 * @returns		#BT_BTR_STATUS_OK or
		 *			#BT_BTR_STATUS_ERROR
		 */
		enum bt_btr_status (* compound_begin)(
				struct bt_field_type *type, void *data);

		/**
		 * Called when a compound type ends.
		 *
		 * @param type		Ending compound type (weak reference)
		 * @param data		User data
		 * @returns		#BT_BTR_STATUS_OK or
		 *			#BT_BTR_STATUS_ERROR
		 */
		enum bt_btr_status (* compound_end)(
				struct bt_field_type *type, void *data);
	} types;

	/**
	 * Query callback functions are used when the type reader needs
	 * dynamic information, i.e. a sequence type's current length
	 * or a variant type's current selected type.
	 *
	 * Both functions need to be set unless it is known that no
	 * sequences or variants will have to be decoded.
	 */
	struct {
		/**
		 * Called to query the current length of a given sequence
		 * type.
		 *
		 * @param type		Sequence type (weak reference)
		 * @param data		User data
		 * @returns		Sequence length or
		 *			#BT_BTR_STATUS_ERROR on error
		 */
		int64_t (* get_sequence_length)(struct bt_field_type *type,
				void *data);

		/**
		 * Called to query the current selected type of a given
		 * variant type.
		 *
		 * @param type		Variant type (weak reference)
		 * @param data		User data
		 * @returns		Current selected type (owned by
		 *			this) or \c NULL on error
		 */
		struct bt_field_type * (* borrow_variant_field_type)(
				struct bt_field_type *type, void *data);
	} query;
};

/**
 * Creates a CTF binary type reader.
 *
 * @param cbs		User callback functions
 * @param data		User data (passed to user callback functions)
 * @returns		New binary type reader on success, or \c NULL on error
 */
struct bt_btr *bt_btr_create(struct bt_btr_cbs cbs, void *data);

/**
 * Destroys a CTF binary type reader, freeing all internal resources.
 *
 * @param btr	Binary type reader
 */
void bt_btr_destroy(struct bt_btr *btr);

/**
 * Decodes a given CTF type from a buffer of bytes.
 *
 * The number of \em bits consumed by this function is returned.
 *
 * The \p status output parameter is where a status is written, amongst
 * the following:
 *
 *   - <b>#BT_BTR_STATUS_OK</b>: Decoding is done.
 *   - <b>#BT_BTR_STATUS_EOF</b>: The end of the buffer was reached,
 *     but more data is needed to finish the decoding process of the
 *     requested type. The user needs to call bt_btr_continue()
 *     as long as #BT_BTR_STATUS_EOF is returned to complete the
 *     decoding process of the original type.
 *   - <b>#BT_BTR_STATUS_INVAL</b>: Invalid argument.
 *   - <b>#BT_BTR_STATUS_ERROR</b>: General error.
 *
 * Calling this function resets the type reader's internal state. If
 * #BT_BTR_STATUS_EOF is returned, bt_btr_continue() needs to
 * be called next, \em not bt_btr_decode().
 *
 * @param btr			Binary type reader
 * @param type			Type to decode (weak reference)
 * @param buf			Buffer
 * @param offset		Offset of first bit from \p buf (bits)
 * @param packet_offset		Offset of \p offset within the CTF
 *				binary packet containing \p type (bits)
 * @param sz			Size of buffer in bytes (from \p buf)
 * @param status		Returned status (see description above)
 * @returns			Number of consumed bits
 */
size_t bt_btr_start(struct bt_btr *btr,
		struct bt_field_type *type, const uint8_t *buf,
		size_t offset, size_t packet_offset, size_t sz,
		enum bt_btr_status *status);

/**
 * Continues the decoding process a given CTF type.
 *
 * The number of bits consumed by this function is returned.
 *
 * The \p status output parameter is where a status is placed, amongst
 * the following:
 *
 *   - <b>#BT_BTR_STATUS_OK</b>: decoding is done.
 *   - <b>#BT_BTR_STATUS_EOF</b>: the end of the buffer was reached,
 *     but more data is needed to finish the decoding process of the
 *     requested type. The user needs to call bt_btr_continue()
 *     as long as #BT_BTR_STATUS_EOF is returned to complete the
 *     decoding process of the original type.
 *   - <b>#BT_BTR_STATUS_INVAL</b>: invalid argument.
 *   - <b>#BT_BTR_STATUS_ERROR</b>: general error.
 *
 * @param btr		Binary type reader
 * @param buf		Buffer
 * @param sz		Size of buffer in bytes (from \p offset)
 * @param status	Returned status (see description above)
 * @returns		Number of consumed bits
 */
size_t bt_btr_continue(struct bt_btr *btr,
		const uint8_t *buf, size_t sz,
		enum bt_btr_status *status);

static inline
const char *bt_btr_status_string(enum bt_btr_status status)
{
	switch (status) {
	case BT_BTR_STATUS_ENOMEM:
		return "BT_BTR_STATUS_ENOMEM";
	case BT_BTR_STATUS_EOF:
		return "BT_BTR_STATUS_EOF";
	case BT_BTR_STATUS_INVAL:
		return "BT_BTR_STATUS_INVAL";
	case BT_BTR_STATUS_ERROR:
		return "BT_BTR_STATUS_ERROR";
	case BT_BTR_STATUS_OK:
		return "BT_BTR_STATUS_OK";
	default:
		return "(unknown)";
	}
}

#endif /* CTF_BTR_H */
