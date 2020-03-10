/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2013, 2014 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 *
 * Babeltrace CTF Writer
 */

#include <glib.h>

#include "functor.h"
#include "utils.h"

BT_HIDDEN
void value_exists(gpointer element, gpointer search_query)
{
	if (element == ((struct bt_ctf_search_query *)search_query)->value) {
		((struct bt_ctf_search_query *)search_query)->found = 1;
	}
}
