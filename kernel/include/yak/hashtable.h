#pragma once

#include <stddef.h>
#include <stdint.h>
#include <yak/status.h>

typedef uintptr_t hash_t;

typedef hash_t (*ht_hash_fn)(const void *data, const size_t len);
typedef bool (*ht_eq_fn)(const void *a, const void *b, const size_t len);

struct ht_entry {
	void *key;
	size_t key_len;
	void *value;
};

typedef struct hashtable {
	size_t count;
	size_t capacity;
	struct ht_entry *entries;

	ht_hash_fn hash;
	ht_eq_fn eq;
} hashtable_t;

#define HASHTABLE_FOR_EACH(ht, entry)                     \
	for (size_t __i = 0; __i < (ht)->capacity; __i++) \
		if (((entry) = &(ht)->entries[__i]))      \
			if ((entry)->key != NULL)

void ht_init(struct hashtable *tbl, ht_hash_fn hash, ht_eq_fn eq);
void ht_destroy(struct hashtable *tbl);

status_t ht_resize(struct hashtable *tbl, size_t new_cap);

status_t ht_set(struct hashtable *tbl, const void *key, size_t ken_len,
		void *value, int overwrite);

void *ht_get(struct hashtable *tbl, const void *key, size_t key_len);

bool ht_del(struct hashtable *tbl, const void *key, size_t key_len);

void ht_debug_dump(struct hashtable *tbl);

// ready-made hash functions
hash_t ht_hash_str(const void *data, const size_t len);
bool ht_eq_str(const void *a, const void *b, const size_t len);
