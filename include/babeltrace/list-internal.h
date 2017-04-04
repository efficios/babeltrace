/*
 * Copyright (C) 2002 Free Software Foundation, Inc.
 * This file is part of the GNU C Library.
 * Contributed by Ulrich Drepper <drepper@redhat.com>, 2002.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; only
 * version 2.1 of the License.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef _BT_LIST_H
#define _BT_LIST_H	1

/* The definitions of this file are adopted from those which can be
   found in the Linux kernel headers to enable people familiar with
   the latter find their way in these sources as well.  */

#ifdef __cplusplus
extern "C" {
#endif

/* Basic type for the double-link list.  */
struct bt_list_head
{
  struct bt_list_head *next;
  struct bt_list_head *prev;
};


/* Define a variable with the head and tail of the list.  */
#define BT_LIST_HEAD(name) \
  struct bt_list_head name = { &(name), &(name) }

/* Initialize a new list head.  */
#define BT_INIT_LIST_HEAD(ptr) \
  (ptr)->next = (ptr)->prev = (ptr)

#define BT_LIST_HEAD_INIT(name) { .prev = &(name), .next = &(name) }

/* Add new element at the head of the list.  */
static inline void
bt_list_add (struct bt_list_head *newp, struct bt_list_head *head)
{
  head->next->prev = newp;
  newp->next = head->next;
  newp->prev = head;
  head->next = newp;
}


/* Add new element at the tail of the list.  */
static inline void
bt_list_add_tail (struct bt_list_head *newp, struct bt_list_head *head)
{
  head->prev->next = newp;
  newp->next = head;
  newp->prev = head->prev;
  head->prev = newp;
}


/* Remove element from list.  */
static inline void
__bt_list_del (struct bt_list_head *prev, struct bt_list_head *next)
{
  next->prev = prev;
  prev->next = next;
}

/* Remove element from list.  */
static inline void
bt_list_del (struct bt_list_head *elem)
{
  __bt_list_del (elem->prev, elem->next);
}

/* delete from list, add to another list as head */
static inline void
bt_list_move (struct bt_list_head *elem, struct bt_list_head *head)
{
  __bt_list_del (elem->prev, elem->next);
  bt_list_add (elem, head);
}

/* replace an old entry.
 */
static inline void
bt_list_replace(struct bt_list_head *old, struct bt_list_head *_new)
{
	_new->next = old->next;
	_new->prev = old->prev;
	_new->prev->next = _new;
	_new->next->prev = _new;
}

/* Join two lists.  */
static inline void
bt_list_splice (struct bt_list_head *add, struct bt_list_head *head)
{
  /* Do nothing if the list which gets added is empty.  */
  if (add != add->next)
    {
      add->next->prev = head;
      add->prev->next = head->next;
      head->next->prev = add->prev;
      head->next = add->next;
    }
}


/* Get typed element from list at a given position.  */
#define bt_list_entry(ptr, type, member) \
  ((type *) ((char *) (ptr) - (unsigned long) (&((type *) 0)->member)))



/* Iterate forward over the elements of the list.  */
#define bt_list_for_each(pos, head) \
  for (pos = (head)->next; pos != (head); pos = pos->next)


/* Iterate forward over the elements of the list.  */
#define bt_list_for_each_prev(pos, head) \
  for (pos = (head)->prev; pos != (head); pos = pos->prev)


/* Iterate backwards over the elements list.  The list elements can be
   removed from the list while doing this.  */
#define bt_list_for_each_prev_safe(pos, p, head) \
  for (pos = (head)->prev, p = pos->prev; \
       pos != (head); \
       pos = p, p = pos->prev)

#define bt_list_for_each_entry(pos, head, member)				\
	for (pos = bt_list_entry((head)->next, typeof(*pos), member);	\
	     &pos->member != (head);					\
	     pos = bt_list_entry(pos->member.next, typeof(*pos), member))

#define bt_list_for_each_entry_reverse(pos, head, member)			\
	for (pos = bt_list_entry((head)->prev, typeof(*pos), member);	\
	     &pos->member != (head);					\
	     pos = bt_list_entry(pos->member.prev, typeof(*pos), member))

#define bt_list_for_each_entry_safe(pos, p, head, member)			\
	for (pos = bt_list_entry((head)->next, typeof(*pos), member),	\
		     p = bt_list_entry(pos->member.next,typeof(*pos), member); \
	     &pos->member != (head);					\
	     pos = p, p = bt_list_entry(pos->member.next, typeof(*pos), member))

static inline int bt_list_empty(struct bt_list_head *head)
{
	return head == head->next;
}

static inline void bt_list_replace_init(struct bt_list_head *old,
				     struct bt_list_head *_new)
{
	struct bt_list_head *head = old->next;
	bt_list_del(old);
	bt_list_add_tail(_new, head);
	BT_INIT_LIST_HEAD(old);
}

#ifdef __cplusplus
}
#endif

#endif	/* _BT_LIST_H */
