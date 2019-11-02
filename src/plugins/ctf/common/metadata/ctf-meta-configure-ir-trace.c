/*
 * Copyright 2019 - Philippe Proulx <pproulx@efficios.com>
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
 */

#include <babeltrace2/babeltrace.h>

#include "ctf-meta-configure-ir-trace.h"

BT_HIDDEN
int ctf_trace_class_configure_ir_trace(struct ctf_trace_class *tc,
		bt_trace *ir_trace)
{
	int ret = 0;
	uint64_t i;

	BT_ASSERT(tc);
	BT_ASSERT(ir_trace);

	if (tc->is_uuid_set) {
		bt_trace_set_uuid(ir_trace, tc->uuid);
	}

	for (i = 0; i < tc->env_entries->len; i++) {
		struct ctf_trace_class_env_entry *env_entry =
			ctf_trace_class_borrow_env_entry_by_index(tc, i);

		switch (env_entry->type) {
		case CTF_TRACE_CLASS_ENV_ENTRY_TYPE_INT:
			ret = bt_trace_set_environment_entry_integer(
				ir_trace, env_entry->name->str,
				env_entry->value.i);
			break;
		case CTF_TRACE_CLASS_ENV_ENTRY_TYPE_STR:
			ret = bt_trace_set_environment_entry_string(
				ir_trace, env_entry->name->str,
				env_entry->value.str->str);
			break;
		default:
			bt_common_abort();
		}

		if (ret) {
			goto end;
		}
	}

end:
	return ret;
}
