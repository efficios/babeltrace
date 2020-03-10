/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (C) 2010-2019 EfficiOS Inc. and Linux Foundation
 */

#ifndef BABELTRACE2_CTF_WRITER_EVENT_TYPES_H
#define BABELTRACE2_CTF_WRITER_EVENT_TYPES_H

#include <babeltrace2-ctf-writer/object.h>
#include <babeltrace2-ctf-writer/field-types.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * bt_ctf_field_type_get and bt_ctf_field_type_put: increment and decrement
 * the field type's reference count.
 *
 * You may also use bt_ctf_object_get_ref() and bt_ctf_object_put_ref() with field type objects.
 *
 * These functions ensure that the field type won't be destroyed while it
 * is in use. The same number of get and put (plus one extra put to
 * release the initial reference done at creation) have to be done to
 * destroy a field type.
 *
 * When the field type's reference count is decremented to 0 by a
 * bt_ctf_field_type_put, the field type is freed.
 *
 * @param type Field type.
 */

/* Pre-2.0 CTF writer compatibility */
static inline
void bt_ctf_field_type_get(struct bt_ctf_field_type *type)
{
	bt_ctf_object_get_ref(type);
}

/* Pre-2.0 CTF writer compatibility */
static inline
void bt_ctf_field_type_put(struct bt_ctf_field_type *type)
{
	bt_ctf_object_put_ref(type);
}

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE2_CTF_WRITER_EVENT_TYPES_H */
