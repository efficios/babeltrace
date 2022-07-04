/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2015 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 */

#include "object.h"
#include <babeltrace2-ctf-writer/object.h>

BT_EXPORT
void *bt_ctf_object_get_ref(void *obj)
{
	if (G_UNLIKELY(!obj)) {
		goto end;
	}

	bt_ctf_object_get_no_null_check(obj);

end:
	return obj;
}

BT_EXPORT
void bt_ctf_object_put_ref(void *obj)
{
	if (G_UNLIKELY(!obj)) {
		return;
	}

	bt_ctf_object_put_no_null_check(obj);
}
