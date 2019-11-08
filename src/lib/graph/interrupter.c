/*
 * Copyright (c) 2019 Philippe Proulx <pproulx@efficios.com>
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

#define BT_LOG_TAG "LIB/INTERRUPTER"
#include "lib/logging.h"

#include <stdlib.h>
#include <stdint.h>
#include <babeltrace2/babeltrace.h>

#include "interrupter.h"
#include "lib/assert-pre.h"

static
void destroy_interrupter(struct bt_object *obj)
{
	g_free(obj);
}

extern struct bt_interrupter *bt_interrupter_create(void)
{
	struct bt_interrupter *intr = g_new0(struct bt_interrupter, 1);

	BT_ASSERT_PRE_NO_ERROR();

	if (!intr) {
		BT_LIB_LOGE_APPEND_CAUSE("Failed to allocate one interrupter.");
		goto error;
	}

	bt_object_init_shared(&intr->base, destroy_interrupter);
	goto end;

error:
	BT_OBJECT_PUT_REF_AND_RESET(intr);

end:
	return intr;
}

void bt_interrupter_set(struct bt_interrupter *intr)
{
	BT_ASSERT_PRE_NON_NULL(intr, "Interrupter");
	intr->is_set = true;
}

void bt_interrupter_reset(struct bt_interrupter *intr)
{
	BT_ASSERT_PRE_NON_NULL(intr, "Interrupter");
	intr->is_set = false;
}

bt_bool bt_interrupter_is_set(const struct bt_interrupter *intr)
{
	BT_ASSERT_PRE_NON_NULL(intr, "Interrupter");
	return (bt_bool) intr->is_set;
}

void bt_interrupter_get_ref(const struct bt_interrupter *intr)
{
	bt_object_get_ref(intr);
}

void bt_interrupter_put_ref(const struct bt_interrupter *intr)
{
	bt_object_put_ref(intr);
}
