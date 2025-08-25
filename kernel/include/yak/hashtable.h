#pragma once

#include <stddef.h>
#include <yak/status.h>

struct hashtable_entry {
	char *key;
	size_t key_len;
	void *value;
};

struct hashtable {
	size_t count;
	size_t capacity;
	struct hashtable_entry *entries;
};

#define HASHTABLE_FOR_EACH(ht, entry)                     \
	for (size_t __i = 0; __i < (ht)->capacity; __i++) \
		if (((entry) = &(ht)->entries[__i]))      \
			if ((entry)->key != NULL)

void hashtable_init(struct hashtable *tbl);
void hashtable_destroy(struct hashtable *tbl);

status_t hashtable_resize(struct hashtable *tbl, size_t new_cap);

status_t hashtable_set(struct hashtable *tbl, const char *key, void *value,
		       int overwrite);

void *hashtable_get(struct hashtable *tbl, const char *key);

int hashtable_delete(struct hashtable *tbl, const char *key);

void hashtable_dump(struct hashtable *tbl);
