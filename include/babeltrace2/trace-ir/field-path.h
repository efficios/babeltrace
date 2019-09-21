#ifndef BABELTRACE2_TRACE_IR_FIELD_PATH_H
#define BABELTRACE2_TRACE_IR_FIELD_PATH_H

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
@defgroup api-tir-field-path Field path
@ingroup api-tir

@brief
    Path to a \bt_field.

A <strong><em>field path</em></strong> indicates how to reach a given
\bt_field from a given <em>root scope</em>.

More specifically, a field path indicates how to reach:

- The length field of a \bt_darray_field (with a length field).
- The selector field of a \bt_opt_field (with a selector field).
- The selector field of a \bt_var_field (with a selector field).

You can borrow the field path from the \ref api-tir-fc "classes" of such
fields with
bt_field_class_array_dynamic_with_length_field_borrow_length_field_path_const(),
bt_field_class_option_with_selector_field_borrow_selector_field_path_const(),
and
bt_field_class_variant_with_selector_field_borrow_selector_field_path_const().
Note that the field path properties of those field classes only becomes
available when the field class becomes part of an \bt_ev_cls or of a
\bt_stream_cls. See
\ref api-tir-fc-link "Field classes with links to other field classes".

A field path is a \ref api-tir "trace IR" metadata object.

A field path is a \ref api-fund-shared-object "shared object": get a
new reference with bt_field_path_get_ref() and put an existing
reference with bt_field_path_put_ref().

The type of a field path is #bt_field_path.

<h1>Properties</h1>

A field path has the following properties:

<dl>
  <dt>
    \anchor api-tir-field-path-prop-root
    Root scope
  </dt>
  <dd>
    Indicates from which \bt_struct_field to start a lookup.

    See \ref api-tir-field-path-lookup-algo "Lookup algorithm" to
    learn more.

    Get a field path's root scope with bt_field_path_get_root_scope().
  </dd>

  <dt>
    \anchor api-tir-field-path-prop-items
    Items
  </dt>
  <dd>
    Each item in a field path's item list indicates which action to take
    to follow the path to the linked \bt_field.

    See \ref api-tir-field-path-lookup-algo "Lookup algorithm" to
    learn more.

    Get the number of items in a field path with
    bt_field_path_get_item_count().

    Borrow an item from a field path with
    bt_field_path_borrow_item_by_index_const(). This function
    returns the #bt_field_path_item type.

    A field path item is a \ref api-fund-unique-object "unique object":
    it belongs to the field path which contains it.
  </dd>
</dl>

<h1>\anchor api-tir-field-path-lookup-algo Lookup algorithm</h1>

The field resolution algorithm using a field path is:

-# Use the appropriate function to set a <em>current field</em> variable
   from the root scope (as returned by bt_field_path_get_root_scope()):

   <dl>
     <dt>#BT_FIELD_PATH_SCOPE_PACKET_CONTEXT</dt>
     <dd>
       bt_packet_borrow_context_field_const().
     </dd>
     <dt>#BT_FIELD_PATH_SCOPE_EVENT_COMMON_CONTEXT</dt>
     <dd>
       bt_event_borrow_common_context_field_const().
     </dd>
     <dt>#BT_FIELD_PATH_SCOPE_EVENT_SPECIFIC_CONTEXT</dt>
     <dd>
       bt_event_borrow_specific_context_field_const().
     </dd>
     <dt>#BT_FIELD_PATH_SCOPE_EVENT_PAYLOAD</dt>
     <dd>
       bt_event_borrow_payload_field_const().
     </dd>
   </dl>

-# For each field path item (use bt_field_path_get_item_count()
   and bt_field_path_borrow_item_by_index_const()), depending on
   the item's type (as returned by bt_field_path_item_get_type()):

   <dl>
     <dt>#BT_FIELD_PATH_ITEM_TYPE_INDEX</dt>
     <dd>
       Call bt_field_path_item_index_get_index() to get the item's
       index value.

       Depending on the current field's class's type (as
       returned by bt_field_get_class_type()):

       <dl>
         <dt>\bt_c_struct_fc</dt>
         <dd>
           Call bt_field_structure_borrow_member_field_by_index_const()
           with the current field and with the item's index to set the
           new current field.
         </dd>

         <dt>\bt_c_var_fc</dt>
         <dd>
           Call bt_field_variant_borrow_selected_option_field_const()
           with the current field to set the new current field.
         </dd>
       </dl>
     </dd>

     <dt>#BT_FIELD_PATH_ITEM_TYPE_CURRENT_ARRAY_ELEMENT</dt>
     <dd>
       Call bt_field_array_borrow_element_field_by_index_const()
       with the index of the field eventually containing the
       field with a link (\bt_darray_field, \bt_opt_field, or
       \bt_var_field) and the current field to set the new current
       field.
     </dd>

     <dt>#BT_FIELD_PATH_ITEM_TYPE_CURRENT_OPTION_CONTENT</dt>
     <dd>
       Call bt_field_option_borrow_field_const() with the current
       field to set the new current field.
     </dd>
   </dl>

After applying this procedure, the current field is the linked
field.
*/

/*! @{ */

/*!
@name Field path
@{

@typedef struct bt_field_path bt_field_path;

@brief
    Field path.

*/

/*!
@brief
    Field path scopes.
*/
typedef enum bt_field_path_scope {
	/*!
	@brief
	    Packet context.
	*/
	BT_FIELD_PATH_SCOPE_PACKET_CONTEXT		= 0,

	/*!
	@brief
	    Event common context.
	*/
	BT_FIELD_PATH_SCOPE_EVENT_COMMON_CONTEXT	= 1,

	/*!
	@brief
	    Event specific context.
	*/
	BT_FIELD_PATH_SCOPE_EVENT_SPECIFIC_CONTEXT	= 2,

	/*!
	@brief
	    Event payload.
	*/
	BT_FIELD_PATH_SCOPE_EVENT_PAYLOAD		= 3,
} bt_field_path_scope;

/*!
@brief
    Returns the root scope of the field path \bt_p{field_path}.

See the \ref api-tir-field-path-prop-root "root scope" property.

@param[in] field_path
    Field path of which to get the root scope.

@returns
    Root scope of \bt_p{field_path}.

@bt_pre_not_null{field_path}
*/
extern bt_field_path_scope bt_field_path_get_root_scope(
		const bt_field_path *field_path);

/*!
@brief
    Returns the number of items contained in the field path
    \bt_p{field_path}.

See the \ref api-tir-field-path-prop-items "items" property.

@param[in] field_path
    Field path of which to get the number of contained items.

@returns
    Number of contained items in \bt_p{field_path}.

@bt_pre_not_null{field_path}
*/
extern uint64_t bt_field_path_get_item_count(
		const bt_field_path *field_path);

/*!
@brief
    Borrows the item at index \bt_p{index} from the
    field path \bt_p{field_path}.

See the \ref api-tir-field-path-prop-items "items" property.

@param[in] field_path
    Field path from which to borrow the item at index \bt_p{index}.
@param[in] index
    Index of the item to borrow from \bt_p{field_path}.

@returns
    @parblock
    \em Borrowed reference of the item of
    \bt_p{field_path} at index \bt_p{index}.

    The returned pointer remains valid as long as \bt_p{field_path}
    exists.
    @endparblock

@bt_pre_not_null{field_path}
@pre
    \bt_p{index} is less than the number of items in
    \bt_p{field_path} (as returned by bt_field_path_get_item_count()).

@sa bt_field_path_get_item_count() &mdash;
    Returns the number of items contained in a field path.
*/
extern const bt_field_path_item *bt_field_path_borrow_item_by_index_const(
		const bt_field_path *field_path, uint64_t index);

/*!
@brief
    Increments the \ref api-fund-shared-object "reference count" of
    the field path \bt_p{field_path}.

@param[in] field_path
    @parblock
    Field path of which to increment the reference count.

    Can be \c NULL.
    @endparblock

@sa bt_field_path_put_ref() &mdash;
    Decrements the reference count of a field path.
*/
extern void bt_field_path_get_ref(const bt_field_path *field_path);

/*!
@brief
    Decrements the \ref api-fund-shared-object "reference count" of
    the field path \bt_p{field_path}.

@param[in] field_path
    @parblock
    Field path of which to decrement the reference count.

    Can be \c NULL.
    @endparblock

@sa bt_field_path_get_ref() &mdash;
    Increments the reference count of a field path.
*/
extern void bt_field_path_put_ref(const bt_field_path *field_path);

/*!
@brief
    Decrements the reference count of the field path
    \bt_p{_field_path}, and then sets \bt_p{_field_path} to \c NULL.

@param _field_path
    @parblock
    Field path of which to decrement the reference count.

    Can contain \c NULL.
    @endparblock

@bt_pre_assign_expr{_field_path}
*/
#define BT_FIELD_PATH_PUT_REF_AND_RESET(_field_path)	\
	do {						\
		bt_field_path_put_ref(_field_path);	\
		(_field_path) = NULL;			\
	} while (0)

/*!
@brief
    Decrements the reference count of the field path \bt_p{_dst}, sets
    \bt_p{_dst} to \bt_p{_src}, and then sets \bt_p{_src} to \c NULL.

This macro effectively moves a field path reference from the expression
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
#define BT_FIELD_PATH_MOVE_REF(_dst, _src)	\
	do {					\
		bt_field_path_put_ref(_dst);	\
		(_dst) = (_src);		\
		(_src) = NULL;			\
	} while (0)

/*! @} */

/*!
@name Field path item
@{

@typedef struct bt_field_path_item bt_field_path_item;

@brief
    Field path item.

*/

/*!
@brief
    Field path item type enumerators.
*/
typedef enum bt_field_path_item_type {
	/*!
	@brief
	    Index of a \bt_struct_field member or selected \bt_var_field
	    option's field.
	*/
	BT_FIELD_PATH_ITEM_TYPE_INDEX				= 1 << 0,

	/*!
	@brief
	    Common field of an \bt_array_field.
	*/
	BT_FIELD_PATH_ITEM_TYPE_CURRENT_ARRAY_ELEMENT		= 1 << 1,

	/*!
	@brief
	    Current field of an \bt_opt_field.
	*/
	BT_FIELD_PATH_ITEM_TYPE_CURRENT_OPTION_CONTENT		= 1 << 2,
} bt_field_path_item_type;

/*!
@brief
    Returns the type enumerator of the field path item
    \bt_p{item}.

See the \ref api-tir-field-path-prop-items "items" property.

@param[in] item
    Field path item of which to get the type enumerator

@returns
    Type enumerator of \bt_p{item}.

@bt_pre_not_null{item}
*/
extern bt_field_path_item_type bt_field_path_item_get_type(
		const bt_field_path_item *item);

/*!
@brief
    Returns the index value of the index field path item
    \bt_p{item}.

See the \ref api-tir-field-path-prop-items "items" property.

@param[in] item
    Index field path item of which to get the index value.

@returns
    Index value of \bt_p{item}.

@bt_pre_not_null{item}
@pre
    \bt_p{item} is an index field path item
    (bt_field_path_item_get_type() returns
    #BT_FIELD_PATH_ITEM_TYPE_INDEX).
*/
extern uint64_t bt_field_path_item_index_get_index(
		const bt_field_path_item *item);

/*! @} */

/*! @} */

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE2_TRACE_IR_FIELD_PATH_H */
