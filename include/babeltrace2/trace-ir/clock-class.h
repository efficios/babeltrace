#ifndef BABELTRACE2_TRACE_IR_CLOCK_CLASS_H
#define BABELTRACE2_TRACE_IR_CLOCK_CLASS_H

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

#ifndef __BT_IN_BABELTRACE_H
# error "Please include <babeltrace2/babeltrace.h> instead."
#endif

#include <stdint.h>

#include <babeltrace2/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/*!
@defgroup api-tir-clock-cls Clock class
@ingroup api-tir

@brief
    Class of \bt_stream clocks.

A <strong><em>clock class</em></strong> is the class of \bt_stream
clocks.

A clock class is a \ref api-tir "trace IR" metadata object.

<em>Stream clocks</em> only exist conceptually in \bt_name because they
are stateful objects. \bt_cp_msg cannot refer to stateful objects
because they must not change while being transported from one
\bt_comp to the other.

Instead of having a stream clock object, messages have a
default \bt_cs: this is a snapshot of the value of a stream's default
clock (a clock class instance):

@image html clocks.png

In the illustration above, notice that:

- \bt_cp_stream (horizontal blue rectangles) are instances of a
  \bt_stream_cls (orange).
- A stream class has a default clock class (orange bell alarm clock).
- Each stream has a default clock (yellow bell alarm clock): this is an
  instance of the stream's class's default clock class.
- Each \bt_msg (objects in blue stream rectangles) created for a given
  stream has a default \bt_cs (yellow star): this is a snapshot of the
  stream's default clock.

  In other words, a default clock snapshot contains the value of the
  stream's default clock when this message occurred.

The default clock class property of a \bt_stream_cls is optional:
if a stream class has no default clock class, then its instances
(\bt_p_stream) have no default clock, therefore all the \bt_p_msg
created from this stream have no default clock snapshot.

A clock class is a \ref api-fund-shared-object "shared object": get a
new reference with bt_clock_class_get_ref() and put an existing
reference with bt_clock_class_put_ref().

Some library functions \ref api-fund-freezing "freeze" clock classes on
success. The documentation of those functions indicate this
postcondition.

The type of a clock class is #bt_clock_class.

Create a default clock class from a \bt_self_comp with
bt_clock_class_create().

<h1>\anchor api-tir-clock-cls-origin Clock value vs. clock class origin</h1>

The value of a \bt_stream clock (a conceptual instance of a clock class)
is in <em>cycles</em>. This value is always positive and is relative to
the clock's class's offset, which is relative to its origin.

A clock class's origin is one of:

<dl>
  <dt>If bt_clock_class_origin_is_unix_epoch() returns #BT_TRUE</dt>
  <dd>
    The
    <a href="https://en.wikipedia.org/wiki/Unix_time">Unix epoch</a>.

    The stream clocks of all the clock classes which have a Unix
    epoch origin, whatever the clock class
    <a href="https://en.wikipedia.org/wiki/Universally_unique_identifier">UUIDs</a>,
    are correlatable.
  </dd>

  <dt>If bt_clock_class_origin_is_unix_epoch() returns #BT_FALSE</dt>
  <dd>
    Undefined.

    In that case, two clock classes which share the same UUID, as
    returned by bt_clock_class_get_uuid(), including having no UUID,
    also share the same origin: their instances (stream clocks) are
    correlatable.
  </dd>
</dl>

To compute an effective stream clock value, in cycles from its class's
origin:

-# Convert the clock class's
   \link api-tir-clock-cls-prop-offset "offset in seconds"\endlink
   property to cycles using its
   \ref api-tir-clock-cls-prop-freq "frequency".
-# Add the value of 1., the stream clock's value, and the clock class's
   \link api-tir-clock-cls-prop-offset "offset in cycles"\endlink
   property.

Because typical tracer clocks have a high frequency (often 1&nbsp;GHz
and more), an effective stream clock value (cycles since Unix epoch, for
example) can be larger than \c UINT64_MAX. This is why a clock class has
two offset properties (one in seconds and one in cycles): to make it
possible for a stream clock to have smaller values, relative to this
offset.

The bt_clock_class_cycles_to_ns_from_origin(),
bt_util_clock_cycles_to_ns_from_origin(), and
bt_clock_snapshot_get_ns_from_origin() functions convert a stream clock
value (cycles) to an equivalent <em>nanoseconds from origin</em> value
using the relevant clock class properties (frequency and offset).

Those functions perform this computation:

-# Convert the clock class's "offset in cycles" property to seconds
   using its frequency.
-# Convert the stream clock's value to seconds using the clock class's
   frequency.
-# Add the values of 1., 2., and the clock class's "offset in seconds"
   property.
-# Convert the value of 3. to nanoseconds.

The following illustration shows the possible scenarios:

@image html clock-terminology.png

The clock class's "offset in seconds" property can be negative.
For example, considering:

- Frequency: 1000&nbsp;Hz.
- Offset in seconds: -10&nbsp;seconds.
- Offset in cycles: 500&nbsp;cycles (that is, 0.5&nbsp;seconds).
- Stream clock's value: 2000&nbsp;cycles (that is, 2&nbsp;seconds).

Then the computed value is -7.5&nbsp;seconds from origin, or
-7,500,000,000&nbsp;nanoseconds from origin.

<h1>Properties</h1>

A clock class has the following properties:

<dl>
  <dt>\anchor api-tir-clock-cls-prop-freq Frequency</dt>
  <dd>
    Frequency of the clock class's instances (stream clocks)
    (cycles/second).

    Use bt_clock_class_set_frequency() and
    bt_clock_class_get_frequency().
  </dd>

  <dt>
    \anchor api-tir-clock-cls-prop-offset
    Offset (in seconds and in cycles)
  </dt>
  <dd>
    Offset in seconds relative to the clock class's
    \ref api-tir-clock-cls-origin "origin", and offset in cycles
    relative to the offset in seconds, of the clock class's
    instances (stream clocks).

    The values of the clock class's instances are relative to the
    computed offset.

    Use bt_clock_class_set_offset() and bt_clock_class_get_offset().
  </dd>

  <dt>\anchor api-tir-clock-cls-prop-precision Precision</dt>
  <dd>
    Precision of the clock class's instance (stream clocks) values
    (cycles).

    For example, considering a precision of 7&nbsp;cycles and the stream
    clock value 42&nbsp;cycles, the real stream clock value can be
    anything between 35&nbsp;cycles and 49&nbsp;cycles.

    Use bt_clock_class_set_precision() and
    bt_clock_class_get_precision().
  </dd>

  <dt>
    \anchor api-tir-clock-cls-prop-origin-unix-epoch
    Origin is Unix epoch?
  </dt>
  <dd>
    Whether or not the clock class's
    \ref api-tir-clock-cls-origin "origin"
    is the
    <a href="https://en.wikipedia.org/wiki/Unix_time">Unix epoch</a>.

    Use bt_clock_class_set_origin_is_unix_epoch() and
    bt_clock_class_origin_is_unix_epoch().
  </dd>

  <dt>\anchor api-tir-clock-cls-prop-name \bt_dt_opt Name</dt>
  <dd>
    Name of the clock class.

    Use bt_clock_class_set_name() and bt_clock_class_get_name().
  </dd>

  <dt>\anchor api-tir-clock-cls-prop-descr \bt_dt_opt Description</dt>
  <dd>
    Description of the clock class.

    Use bt_clock_class_set_description() and
    bt_clock_class_get_description().
  </dd>

  <dt>\anchor api-tir-clock-cls-prop-uuid \bt_dt_opt UUID</dt>
  <dd>
    <a href="https://en.wikipedia.org/wiki/Universally_unique_identifier">UUID</a>
    of the clock class.

    The clock class's UUID uniquely identifies the clock class.

    When the clock class's origin is \em not the
    <a href="https://en.wikipedia.org/wiki/Unix_time">Unix epoch</a>,
    then the clock class's UUID determines whether or not two different
    clock classes have correlatable instances.

    Use bt_clock_class_set_uuid() and bt_clock_class_get_uuid().
  </dd>

  <dt>
    \anchor api-tir-clock-cls-prop-user-attrs
    \bt_dt_opt User attributes
  </dt>
  <dd>
    User attributes of the clock class.

    User attributes are custom attributes attached to a clock class.

    Use bt_clock_class_set_user_attributes(),
    bt_clock_class_borrow_user_attributes(), and
    bt_clock_class_borrow_user_attributes_const().
  </dd>
</dl>
*/

/*! @{ */

/*!
@name Type
@{

@typedef struct bt_clock_class bt_clock_class;

@brief
    Clock class.

@}
*/

/*!
@name Creation
@{
*/

/*!
@brief
    Creates a default clock class from the \bt_self_comp
    \bt_p{self_component}.

On success, the returned clock class has the following property values:

<table>
  <tr>
    <th>Property
    <th>Value
  <tr>
    <td>\ref api-tir-clock-cls-prop-freq "Frequency"
    <td>1&nbsp;GHz
  <tr>
    <td>\ref api-tir-clock-cls-prop-offset "Offset" in seconds
    <td>0&nbsp;seconds
  <tr>
    <td>\ref api-tir-clock-cls-prop-offset "Offset" in cycles
    <td>0&nbsp;cycles
  <tr>
    <td>\ref api-tir-clock-cls-prop-precision "Precision"
    <td>0&nbsp;cycles
  <tr>
    <td>\ref api-tir-clock-cls-prop-origin-unix-epoch "Origin is Unix epoch?"
    <td>Yes
  <tr>
    <td>\ref api-tir-clock-cls-prop-name "Name"
    <td>\em None
  <tr>
    <td>\ref api-tir-clock-cls-prop-descr "Description"
    <td>\em None
  <tr>
    <td>\ref api-tir-clock-cls-prop-uuid "UUID"
    <td>\em None
  <tr>
    <td>\ref api-tir-clock-cls-prop-user-attrs "User attributes"
    <td>Empty \bt_map_val
</table>

@param[in] self_component
    Self component from which to create the default clock class.

@returns
    New clock class reference, or \c NULL on memory error.

@bt_pre_not_null{self_component}
*/
extern bt_clock_class *bt_clock_class_create(bt_self_component *self_component);

/*! @} */

/*!
@name Properties
@{
*/

/*!
@brief
    Sets the frequency (Hz) of the clock class \bt_p{clock_class} to
    \bt_p{frequency}.

See the \ref api-tir-clock-cls-prop-freq "frequency" property.

@param[in] clock_class
    Clock class of which to set the frequency to \bt_p{frequency}.
@param[in] frequency
    New frequency of \bt_p{clock_class}.

@bt_pre_not_null{clock_class}
@bt_pre_hot{clock_class}
@pre
    \bt_p{frequency} is not 0.
@pre
    \bt_p{frequency} is not <code>UINT64_C(-1)</code>.
@pre
    \bt_p{frequency} is greater than the clock class's offset in cycles
    (as returned by bt_clock_class_get_offset()).

@sa bt_clock_class_get_frequency() &mdash;
    Returns the frequency of a clock class.
*/
extern void bt_clock_class_set_frequency(bt_clock_class *clock_class,
		uint64_t frequency);

/*!
@brief
    Returns the frequency (Hz) of the clock class \bt_p{clock_class}.

See the \ref api-tir-clock-cls-prop-freq "frequency" property.

@param[in] clock_class
    Clock class of which to get the frequency.

@returns
    Frequency (Hz) of \bt_p{clock_class}.

@bt_pre_not_null{clock_class}

@sa bt_clock_class_set_frequency() &mdash;
    Sets the frequency of a clock class.
*/
extern uint64_t bt_clock_class_get_frequency(
		const bt_clock_class *clock_class);

/*!
@brief
    Sets the offset of the clock class \bt_p{clock_class} to
    \bt_p{offset_seconds} plus \bt_p{offset_cycles} from its
    \ref api-tir-clock-cls-origin "origin".

See the \ref api-tir-clock-cls-prop-offset "offset" property.

@param[in] clock_class
    Clock class of which to set the offset to \bt_p{offset_seconds}
    and \bt_p{offset_cycles}.
@param[in] offset_seconds
    New offset in seconds of \bt_p{clock_class}.
@param[in] offset_cycles
    New offset in cycles of \bt_p{clock_class}.

@bt_pre_not_null{clock_class}
@bt_pre_hot{clock_class}
@pre
    \bt_p{offset_cycles} is less than the clock class's frequency
    (as returned by bt_clock_class_get_frequency()).

@sa bt_clock_class_get_offset() &mdash;
    Returns the offset of a clock class.
*/
extern void bt_clock_class_set_offset(bt_clock_class *clock_class,
		int64_t offset_seconds, uint64_t offset_cycles);

/*!
@brief
    Returns the offsets in seconds and cycles of the clock class
    \bt_p{clock_class}.

See the \ref api-tir-clock-cls-prop-offset "offset" property.

@param[in] clock_class
    Clock class of which to get the offset.
@param[out] offset_seconds
    When this function returns, \bt_p{*offset_seconds} is the offset in
    seconds of
    \bt_p{clock_class}.
@param[out] offset_cycles
    When this function returns, \bt_p{*offset_cycles} is the offset in
    cycles of \bt_p{clock_class}.

@bt_pre_not_null{clock_class}
@bt_pre_not_null{offset_seconds}
@bt_pre_not_null{offset_cycles}

@sa bt_clock_class_set_offset() &mdash;
    Sets the offset of a clock class.
*/
extern void bt_clock_class_get_offset(const bt_clock_class *clock_class,
		int64_t *offset_seconds, uint64_t *offset_cycles);

/*!
@brief
    Sets the precision (cycles) of the clock class \bt_p{clock_class} to
    \bt_p{precision}.

See the \ref api-tir-clock-cls-prop-precision "precision" property.

@param[in] clock_class
    Clock class of which to set the precision to \bt_p{precision}.
@param[in] precision
    New precision of \bt_p{clock_class}.

@bt_pre_not_null{clock_class}
@bt_pre_hot{clock_class}

@sa bt_clock_class_get_precision() &mdash;
    Returns the precision of a clock class.
*/
extern void bt_clock_class_set_precision(bt_clock_class *clock_class,
		uint64_t precision);

/*!
@brief
    Returns the precision (cycles) of the clock class
    \bt_p{clock_class}.

See the \ref api-tir-clock-cls-prop-precision "precision" property.

@param[in] clock_class
    Clock class of which to get the precision.

@returns
    Precision (cycles) of \bt_p{clock_class}.

@bt_pre_not_null{clock_class}

@sa bt_clock_class_set_precision() &mdash;
    Sets the precision of a clock class.
*/
extern uint64_t bt_clock_class_get_precision(
		const bt_clock_class *clock_class);

/*!
@brief
    Sets whether or not the \ref api-tir-clock-cls-origin "origin"
    of the clock class \bt_p{clock_class} is the
    <a href="https://en.wikipedia.org/wiki/Unix_time">Unix epoch</a>.

See the \ref api-tir-clock-cls-prop-origin-unix-epoch "origin is Unix epoch?"
property.

@param[in] clock_class
    Clock class of which to set whether or not its origin is the
    Unix epoch.
@param[in] origin_is_unix_epoch
    #BT_TRUE to make \bt_p{clock_class} have a Unix epoch origin.

@bt_pre_not_null{clock_class}
@bt_pre_hot{clock_class}

@sa bt_clock_class_origin_is_unix_epoch() &mdash;
    Returns whether or not the origin of a clock class is the
    Unix epoch.
*/
extern void bt_clock_class_set_origin_is_unix_epoch(bt_clock_class *clock_class,
		bt_bool origin_is_unix_epoch);

/*!
@brief
    Returns whether or not the \ref api-tir-clock-cls-origin "origin"
    of the clock class \bt_p{clock_class} is the
    <a href="https://en.wikipedia.org/wiki/Unix_time">Unix epoch</a>.

See the \ref api-tir-clock-cls-prop-origin-unix-epoch "origin is Unix epoch?"
property.

@param[in] clock_class
    Clock class of which to get whether or not its origin is the
    Unix epoch.

@returns
    #BT_TRUE if the origin of \bt_p{clock_class} is the Unix epoch.

@bt_pre_not_null{clock_class}

@sa bt_clock_class_set_origin_is_unix_epoch() &mdash;
    Sets whether or not the origin of a clock class is the Unix epoch.
*/
extern bt_bool bt_clock_class_origin_is_unix_epoch(
		const bt_clock_class *clock_class);

/*!
@brief
    Status codes for bt_clock_class_set_name().
*/
typedef enum bt_clock_class_set_name_status {
	/*!
	@brief
	    Success.
	*/
	BT_CLOCK_CLASS_SET_NAME_STATUS_OK		= __BT_FUNC_STATUS_OK,

	/*!
	@brief
	    Out of memory.
	*/
	BT_CLOCK_CLASS_SET_NAME_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,
} bt_clock_class_set_name_status;

/*!
@brief
    Sets the name of the clock class \bt_p{clock_class} to
    a copy of \bt_p{name}.

See the \ref api-tir-clock-cls-prop-name "name" property.

@param[in] clock_class
    Clock class of which to set the name to \bt_p{name}.
@param[in] name
    New name of \bt_p{clock_class} (copied).

@retval #BT_CLOCK_CLASS_SET_NAME_STATUS_OK
    Success.
@retval #BT_CLOCK_CLASS_SET_NAME_STATUS_MEMORY_ERROR
    Out of memory.

@bt_pre_not_null{clock_class}
@bt_pre_hot{clock_class}
@bt_pre_not_null{name}

@sa bt_clock_class_get_name() &mdash;
    Returns the name of a clock class.
*/
extern bt_clock_class_set_name_status bt_clock_class_set_name(
		bt_clock_class *clock_class, const char *name);

/*!
@brief
    Returns the name of the clock class \bt_p{clock_class}.

See the \ref api-tir-clock-cls-prop-name "name" property.

If \bt_p{clock_class} has no name, this function returns \c NULL.

@param[in] clock_class
    Clock class of which to get the name.

@returns
    @parblock
    Name of \bt_p{clock_class}, or \c NULL if none.

    The returned pointer remains valid as long as \bt_p{clock_class}
    is not modified.
    @endparblock

@bt_pre_not_null{clock_class}

@sa bt_clock_class_set_name() &mdash;
    Sets the name of a clock class.
*/
extern const char *bt_clock_class_get_name(
		const bt_clock_class *clock_class);

/*!
@brief
    Status codes for bt_clock_class_set_description().
*/
typedef enum bt_clock_class_set_description_status {
	/*!
	@brief
	    Success.
	*/
	BT_CLOCK_CLASS_SET_DESCRIPTION_STATUS_OK		= __BT_FUNC_STATUS_OK,

	/*!
	@brief
	    Out of memory.
	*/
	BT_CLOCK_CLASS_SET_DESCRIPTION_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,
} bt_clock_class_set_description_status;

/*!
@brief
    Sets the description of the clock class \bt_p{clock_class} to a copy
    of \bt_p{description}.

See the \ref api-tir-clock-cls-prop-descr "description" property.

@param[in] clock_class
    Clock class of which to set the description to \bt_p{description}.
@param[in] description
    New description of \bt_p{clock_class} (copied).

@retval #BT_CLOCK_CLASS_SET_DESCRIPTION_STATUS_OK
    Success.
@retval #BT_CLOCK_CLASS_SET_DESCRIPTION_STATUS_MEMORY_ERROR
    Out of memory.

@bt_pre_not_null{clock_class}
@bt_pre_hot{clock_class}
@bt_pre_not_null{description}

@sa bt_clock_class_get_description() &mdash;
    Returns the description of a clock class.
*/
extern bt_clock_class_set_description_status bt_clock_class_set_description(
		bt_clock_class *clock_class, const char *description);

/*!
@brief
    Returns the description of the clock class \bt_p{clock_class}.

See the \ref api-tir-clock-cls-prop-descr "description" property.

If \bt_p{clock_class} has no description, this function returns \c NULL.

@param[in] clock_class
    Clock class of which to get the description.

@returns
    @parblock
    Description of \bt_p{clock_class}, or \c NULL if none.

    The returned pointer remains valid as long as \bt_p{clock_class}
    is not modified.
    @endparblock

@bt_pre_not_null{clock_class}

@sa bt_clock_class_set_description() &mdash;
    Sets the description of a clock class.
*/
extern const char *bt_clock_class_get_description(
		const bt_clock_class *clock_class);

/*!
@brief
    Sets the
    <a href="https://en.wikipedia.org/wiki/Universally_unique_identifier">UUID</a>
    of the clock class \bt_p{clock_class} to a copy of \bt_p{uuid}.

See the \ref api-tir-clock-cls-prop-uuid "UUID" property.

@param[in] clock_class
    Clock class of which to set the UUID to \bt_p{uuid}.
@param[in] uuid
    New UUID of \bt_p{clock_class} (copied).

@bt_pre_not_null{clock_class}
@bt_pre_hot{clock_class}
@bt_pre_not_null{uuid}

@sa bt_clock_class_get_uuid() &mdash;
    Returns the UUID of a clock class.
*/
extern void bt_clock_class_set_uuid(bt_clock_class *clock_class,
		bt_uuid uuid);

/*!
@brief
    Returns the UUID of the clock class \bt_p{clock_class}.

See the \ref api-tir-clock-cls-prop-uuid "UUID" property.

If \bt_p{clock_class} has no UUID, this function returns \c NULL.

@param[in] clock_class
    Clock class of which to get the UUID.

@returns
    @parblock
    UUID of \bt_p{clock_class}, or \c NULL if none.

    The returned pointer remains valid as long as \bt_p{clock_class}
    is not modified.
    @endparblock

@bt_pre_not_null{clock_class}

@sa bt_clock_class_set_uuid() &mdash;
    Sets the UUID of a clock class.
*/
extern bt_uuid bt_clock_class_get_uuid(
		const bt_clock_class *clock_class);

/*!
@brief
    Sets the user attributes of the clock class \bt_p{clock_class} to
    \bt_p{user_attributes}.

See the \ref api-tir-clock-cls-prop-user-attrs "user attributes"
property.

@note
    When you create a default clock class with bt_clock_class_create(),
    the clock class's initial user attributes is an empty \bt_map_val.
    Therefore you can borrow it with
    bt_clock_class_borrow_user_attributes() and fill it directly instead
    of setting a new one with this function.

@param[in] clock_class
    Clock class of which to set the user attributes to
    \bt_p{user_attributes}.
@param[in] user_attributes
    New user attributes of \bt_p{clock_class}.

@bt_pre_not_null{clock_class}
@bt_pre_hot{clock_class}
@bt_pre_not_null{user_attributes}
@bt_pre_is_map_val{user_attributes}

@sa bt_clock_class_borrow_user_attributes() &mdash;
    Borrows the user attributes of a clock class.
*/
extern void bt_clock_class_set_user_attributes(
		bt_clock_class *clock_class, const bt_value *user_attributes);

/*!
@brief
    Borrows the user attributes of the clock class \bt_p{clock_class}.

See the \ref api-tir-clock-cls-prop-user-attrs "user attributes"
property.

@note
    When you create a default clock class with bt_clock_class_create(),
    the clock class's initial user attributes is an empty \bt_map_val.

@param[in] clock_class
    Clock class from which to borrow the user attributes.

@returns
    User attributes of \bt_p{clock_class} (a \bt_map_val).

@bt_pre_not_null{clock_class}

@sa bt_clock_class_set_user_attributes() &mdash;
    Sets the user attributes of a clock class.
@sa bt_clock_class_borrow_user_attributes_const() &mdash;
    \c const version of this function.
*/
extern bt_value *bt_clock_class_borrow_user_attributes(
		bt_clock_class *clock_class);

/*!
@brief
    Borrows the user attributes of the clock class \bt_p{clock_class}
    (\c const version).

See bt_clock_class_borrow_user_attributes().
*/
extern const bt_value *bt_clock_class_borrow_user_attributes_const(
		const bt_clock_class *clock_class);

/*! @} */

/*!
@name Utilities
@{
*/

/*!
@brief
    Status codes for bt_clock_class_cycles_to_ns_from_origin().
*/
typedef enum bt_clock_class_cycles_to_ns_from_origin_status {
	/*!
	@brief
	    Success.
	*/
	BT_CLOCK_CLASS_CYCLES_TO_NS_FROM_ORIGIN_STATUS_OK		= __BT_FUNC_STATUS_OK,

	/*!
	@brief
	    Integer overflow while computing the result.
	*/
	BT_CLOCK_CLASS_CYCLES_TO_NS_FROM_ORIGIN_STATUS_OVERFLOW_ERROR	= __BT_FUNC_STATUS_OVERFLOW_ERROR,
} bt_clock_class_cycles_to_ns_from_origin_status;

/*!
@brief
    Converts the stream clock value \bt_p{value} from cycles to
    nanoseconds from the \ref api-tir-clock-cls-origin "origin" of the
    clock class \bt_p{clock_class} and sets \bt_p{*ns_from_origin}
    to the result.

This function:

-# Converts the
   \link api-tir-clock-cls-prop-offset "offset in cycles"\endlink
   property of \bt_p{clock_class} to seconds using its
   \ref api-tir-clock-cls-prop-freq "frequency".
-# Converts the \bt_p{value} value to seconds using the frequency of
   \bt_p{clock_class}.
-# Adds the values of 1., 2., and the
   \link api-tir-clock-cls-prop-offset "offset in seconds"\endlink
   property of \bt_p{clock_class}.
-# Converts the value of 3. to nanoseconds and sets
   \bt_p{*ns_from_origin} to this result.

The following illustration shows the possible scenarios:

@image html clock-terminology.png

This function can fail and return the
#BT_CLOCK_CLASS_CYCLES_TO_NS_FROM_ORIGIN_STATUS_OVERFLOW_ERROR status
code if any step of the computation process causes an integer overflow.

@param[in] clock_class
    Stream clock's class.
@param[in] value
    Stream clock's value (cycles) to convert to nanoseconds from
    the origin of \bt_p{clock_class}.
@param[out] ns_from_origin
    <strong>On success</strong>, \bt_p{*ns_from_origin} is \bt_p{value}
    converted to nanoseconds from the origin of \bt_p{clock_class}.

@retval #BT_UTIL_CLOCK_CYCLES_TO_NS_FROM_ORIGIN_STATUS_OK
    Success.
@retval #BT_UTIL_CLOCK_CYCLES_TO_NS_FROM_ORIGIN_STATUS_OVERFLOW_ERROR
    Integer overflow while computing the result.

@bt_pre_not_null{clock_class}
@bt_pre_not_null{ns_from_origin}

@sa bt_util_clock_cycles_to_ns_from_origin() &mdash;
    Converts a clock value from cycles to nanoseconds from the clock's
    origin.
*/
extern bt_clock_class_cycles_to_ns_from_origin_status
bt_clock_class_cycles_to_ns_from_origin(
		const bt_clock_class *clock_class,
		uint64_t value, int64_t *ns_from_origin);

/*! @} */

/*!
@name Reference count
@{
*/

/*!
@brief
    Increments the \ref api-fund-shared-object "reference count" of
    the clock class \bt_p{clock_class}.

@param[in] clock_class
    @parblock
    Clock class of which to increment the reference count.

    Can be \c NULL.
    @endparblock

@sa bt_clock_class_put_ref() &mdash;
    Decrements the reference count of a clock class.
*/
extern void bt_clock_class_get_ref(const bt_clock_class *clock_class);

/*!
@brief
    Decrements the \ref api-fund-shared-object "reference count" of
    the clock class \bt_p{clock_class}.

@param[in] clock_class
    @parblock
    Clock class of which to decrement the reference count.

    Can be \c NULL.
    @endparblock

@sa bt_clock_class_get_ref() &mdash;
    Increments the reference count of a clock class.
*/
extern void bt_clock_class_put_ref(const bt_clock_class *clock_class);

/*!
@brief
    Decrements the reference count of the clock class
    \bt_p{_clock_class}, and then sets \bt_p{_clock_class} to \c NULL.

@param _clock_class
    @parblock
    Clock class of which to decrement the reference count.

    Can contain \c NULL.
    @endparblock

@bt_pre_assign_expr{_clock_class}
*/
#define BT_CLOCK_CLASS_PUT_REF_AND_RESET(_clock_class)	\
	do {						\
		bt_clock_class_put_ref(_clock_class);	\
		(_clock_class) = NULL;			\
	} while (0)

/*!
@brief
    Decrements the reference count of the clock class \bt_p{_dst}, sets
    \bt_p{_dst} to \bt_p{_src}, and then sets \bt_p{_src} to \c NULL.

This macro effectively moves a clock class reference from the expression
\bt_p{_src} to the expression \bt_p{_dst}, putting the existing
\bt_p{_dst} reference.

@param _dst
    @parblock
    Destination expression.

    Can contain \c NULL.
    @endparblock
@param _src
    @parblock
    Source expression.

    Can contain \c NULL.
    @endparblock

@bt_pre_assign_expr{_dst}
@bt_pre_assign_expr{_src}
*/
#define BT_CLOCK_CLASS_MOVE_REF(_dst, _src)	\
	do {					\
		bt_clock_class_put_ref(_dst);	\
		(_dst) = (_src);		\
		(_src) = NULL;			\
	} while (0)

/*! @} */

/*! @} */

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE2_TRACE_IR_CLOCK_CLASS_H */
