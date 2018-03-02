/*
 * test_bt_notification_heap.c
 *
 * bt_notification_heap tests
 *
 * Copyright 2016 - Jérémie Galarneau <jeremie.galarneau@efficios.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; under version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "tap/tap.h"
#include <stdlib.h>
#include <babeltrace/compiler-internal.h>
#include <babeltrace/ref.h>
#include <babeltrace/graph/notification-heap.h>
#include <babeltrace/graph/notification.h>
#include <babeltrace/graph/notification-internal.h>

#define NR_TESTS 7

struct dummy_notification {
	struct bt_notification parent;
	uint64_t value;
};

static
void dummy_notification_destroy(struct bt_object *obj)
{
	g_free(obj);
}

/* Reproduced from internal notification.c code. */
void bt_notification_init(struct bt_notification *notification,
		enum bt_notification_type type,
		bt_object_release_func release)
{
	assert(type > 0 && type < BT_NOTIFICATION_TYPE_NR);
	notification->type = type;
	bt_object_init(&notification->base, release);
}

static
struct bt_notification *dummy_notification_create(uint64_t value)
{
	struct dummy_notification *notification;

	notification = g_new0(struct dummy_notification, 1);
	if (!notification) {
		goto error;
	}
	bt_notification_init(&notification->parent,
			BT_NOTIFICATION_TYPE_NR - 1 /* dummy value */,
		        dummy_notification_destroy);
	notification->value = value;
	return &notification->parent;
error:
	return NULL;
}

static
bt_bool compare_notifications(struct bt_notification *a, struct bt_notification *b,
		void *unused)
{
	uint64_t val_a = ((struct dummy_notification *) a)->value;
	uint64_t val_b = ((struct dummy_notification *) b)->value;

	if (val_a == val_b) {
		return a < b;
	} else {
		return val_a < val_b;
	}
}

int main(int argc, char **argv)
{
	int i;
	uint64_t last_read_value = 0;
	struct bt_notification_heap *heap = NULL;

	/* Initialize tap harness before any tests */
	plan_tests(NR_TESTS);
	heap = bt_notification_heap_create(compare_notifications, NULL);
	ok(heap, "Created a notification heap");

	/* Insert 10 000 notifications with random values. */
	for (i = 0; i < 10000; i++) {
		int ret;
		struct bt_notification *notification =
				dummy_notification_create(rand());

		if (!notification) {
			diag("Dummy notification creation failed");
			goto end;
		}

		ret = bt_notification_heap_insert(heap, notification);
		if (ret) {
			diag("Failed to insert notification %i in heap", i);
			goto end;
		}
		bt_put(notification);
	}
	pass("Inserted 10 000 random notifications in notification heap");

	/* Pop 5000 notifications, making sure the values read are ascending */
	for (i = 0; i < 5000; i++) {
		struct bt_notification *pop_notification;
		struct bt_notification *peek_notification;
		struct dummy_notification *dummy;

		peek_notification = bt_notification_heap_peek(heap);
		if (!peek_notification) {
			fail("Failed to peek a notification");
			goto end;
		}

		pop_notification = bt_notification_heap_pop(heap);
		if (!pop_notification) {
			fail("Failed to pop a notification");
			goto end;
		}

		if (peek_notification != pop_notification) {
			fail("bt_notification_heap_peek and bt_notification_heap_pop do not return the same notification");
			bt_put(peek_notification);
			bt_put(pop_notification);
			goto end;
		}

		dummy = container_of(pop_notification,
				struct dummy_notification, parent);
		if (dummy->value < last_read_value) {
			fail("Notification heap did not provide notifications in ascending order");
		}
		last_read_value = dummy->value;
		bt_put(peek_notification);
		bt_put(pop_notification);
	}

	pass("bt_notification_heap_peek and bt_notification_heap_pop return the same notification");
	pass("Notification heap provided 5 000 notifications in ascending order");

	/* Insert 10 000 notifications with random values. */
	for (i = 0; i < 10000; i++) {
		int ret;
		struct bt_notification *notification =
				dummy_notification_create(rand());

		if (!notification) {
			diag("Dummy notification creation failed");
			goto end;
		}

		ret = bt_notification_heap_insert(heap, notification);
		if (ret) {
			diag("Failed to insert notification %i in heap", i);
			goto end;
		}
		bt_put(notification);
	}
	pass("Inserted 10 000 random notifications in notification heap after popping");

	last_read_value = 0;
	/* Pop remaining 15 000 notifications, making sure the values read are ascending */
	for (i = 0; i < 15000; i++) {
		struct bt_notification *pop_notification;
		struct dummy_notification *dummy;

		pop_notification = bt_notification_heap_pop(heap);
		if (!pop_notification) {
			fail("Failed to pop a notification");
			goto end;
		}
		dummy = container_of(pop_notification,
				struct dummy_notification, parent);
		if (dummy->value < last_read_value) {
			fail("Notification heap did not provide notifications in ascending order");
		}
		last_read_value = dummy->value;
		bt_put(pop_notification);
	}
	pass("Popped remaining 15 000 notifications from heap in ascending order");

	ok(!bt_notification_heap_peek(heap), "No notifications left in heap");
end:
	bt_put(heap);
	return exit_status();
}
