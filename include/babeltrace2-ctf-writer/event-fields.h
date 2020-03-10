/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (C) 2010-2019 EfficiOS Inc. and Linux Foundation
 */

#ifndef BABELTRACE2_CTF_WRITER_EVENT_FIELDS_H
#define BABELTRACE2_CTF_WRITER_EVENT_FIELDS_H

#include <babeltrace2-ctf-writer/object.h>
#include <babeltrace2-ctf-writer/field-types.h>
#include <babeltrace2-ctf-writer/fields.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * bt_ctf_field_get and bt_ctf_field_put: increment and decrement the
 * field's reference count.
 *
 * You may also use bt_ctf_object_get_ref() and bt_ctf_object_put_ref() with field objects.
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

/* Pre-2.0 CTF writer compatibility */
static inline
void bt_ctf_field_get(struct bt_ctf_field *field)
{
	bt_ctf_object_get_ref(field);
}

/* Pre-2.0 CTF writer compatibility */
static inline
void bt_ctf_field_put(struct bt_ctf_field *field)
{
	bt_ctf_object_put_ref(field);
}

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE2_CTF_WRITER_EVENT_FIELDS_H */
