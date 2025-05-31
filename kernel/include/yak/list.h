#pragma once

#include <stddef.h>
#include <yak/macro.h>

struct list_head {
	struct list_head *next, *prev;
};

#define LIST_INITIALIZER(name) { &(name), &(name) }

#define LIST_HEAD(name) struct list_head name = LIST_INITIALIZER(name)

static inline void list_init(struct list_head *list)
{
	list->next = list;
	list->prev = list;
}

// add between two list entries
static inline void __list_add(struct list_head *node, struct list_head *prev,
			      struct list_head *next)
{
	next->prev = node;
	node->next = next;
	node->prev = prev;
	prev->next = node;
}

static inline void list_add(struct list_head *head, struct list_head *node)
{
	__list_add(node, head, head->next);
}

static inline void list_add_tail(struct list_head *head, struct list_head *node)
{
	__list_add(node, head->prev, head);
}

static inline void list_del(struct list_head *node)
{
	node->prev->next = node->next;
	node->next->prev = node->prev;
	node->next = node->prev = NULL;
}

static inline void list_move(struct list_head *node, struct list_head *head)
{
	list_del(node);
	list_add(node, head);
}

static inline void list_move_tail(struct list_head *node,
				  struct list_head *head)
{
	list_del(node);
	list_add_tail(node, head);
}

static inline int list_empty(const struct list_head *node)
{
	return node->next == node;
}

static inline void list_splice(struct list_head *list, struct list_head *head)
{
	if (!list_empty(list)) {
		struct list_head *first = list->next;
		struct list_head *last = list->prev;
		struct list_head *at = head->next;

		first->prev = head;
		head->next = first;

		last->next = at;
		at->prev = last;
	}
}

static inline struct list_head *list_pop_front(struct list_head *head)
{
	if (list_empty(head))
		return NULL;

	struct list_head *node = head->next;
	list_del(node);
	return node;
}

static inline struct list_head *list_pop_back(struct list_head *head)
{
	if (list_empty(head))
		return NULL;

	struct list_head *node = head->prev;
	list_del(node);
	return node;
}

#define list_entry(ptr, type, member) container_of(ptr, type, member)

#define list_for_each(pos, head) \
	for (pos = (head)->next; pos != (head); pos = pos->next)

#define list_for_each_safe(pos, n, head)                       \
	for (pos = (head)->next, n = pos->next; pos != (head); \
	     pos = n, n = pos->next)
