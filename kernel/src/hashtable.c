/*
    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to
    deal in the Software without restriction, including without limitation the
    rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
    sell copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
    IN THE SOFTWARE.

    https://github.com/munificent/craftinginterpreters/blob/master/c/table.c
*/

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <yak/heap.h>
#include <yak/status.h>
#include <yak/hashtable.h>
#include <yak/log.h>

#define TOMB (void *)1

void ht_init(struct hashtable *tbl, ht_hash_fn hash, ht_eq_fn eq)
{
	tbl->count = 0;
	tbl->capacity = 0;
	tbl->entries = NULL;
	tbl->hash = hash;
	tbl->eq = eq;
}

void ht_destroy(struct hashtable *tbl)
{
	for (size_t i = 0; i < tbl->capacity; i++) {
		struct ht_entry *entry = &tbl->entries[i];
		if (entry->key)
			kfree(entry->key, entry->key_len);
	}
	kfree(tbl->entries, sizeof(struct ht_entry) * tbl->capacity);
	ht_init(tbl, NULL, NULL);
}

static struct ht_entry *find_entry(hashtable_t *tbl, struct ht_entry *entries,
				   int capacity, const void *key,
				   size_t key_len)
{
	size_t index = tbl->hash(key, key_len) % capacity;

	struct ht_entry *tombstone = NULL;

	for (;;) {
		struct ht_entry *entry = &entries[index];

		if (entry->key == NULL) {
			if (entry->value == TOMB) {
				if (tombstone == NULL)
					tombstone = entry;
			} else {
				return tombstone != NULL ? tombstone : entry;
			}

		} else if (key_len == entry->key_len &&
			   tbl->eq(key, entry->key, key_len)) {
			return entry;
		}

		index = (index + 1) % capacity;
	}
}

status_t ht_resize(struct hashtable *tbl, size_t new_cap)
{
	struct ht_entry *entries = kzalloc(new_cap * sizeof(struct ht_entry));

	if (!entries)
		return YAK_OOM;

	// reinsert all entries; removes the tombstones
	tbl->count = 0;

	for (size_t i = 0; i < tbl->capacity; i++) {
		struct ht_entry *entry = &tbl->entries[i];
		if (entry->key == NULL)
			continue;

		struct ht_entry *dest = find_entry(tbl, entries, new_cap,
						   entry->key, entry->key_len);
		dest->key = entry->key;
		dest->key_len = entry->key_len;
		dest->value = entry->value;

		tbl->count++;
	}

	kfree(tbl->entries, tbl->capacity * sizeof(struct ht_entry));

	tbl->entries = entries;
	tbl->capacity = new_cap;

	return YAK_SUCCESS;
}

status_t ht_set(struct hashtable *tbl, const void *key, size_t key_len,
		void *value, int overwrite)
{
	// max load of 0.75
	// count + 1 > capacity * 0.75 | * 4
	if ((tbl->count + 1) * 4 > tbl->capacity * 3) {
		size_t new_cap = tbl->capacity == 0 ? 16 : tbl->capacity * 2;
		status_t res = ht_resize(tbl, new_cap);
		IF_ERR(res)
		{
			return res;
		}
	}

	struct ht_entry *entry =
		find_entry(tbl, tbl->entries, tbl->capacity, key, key_len);
	int new_key = (entry->key == NULL);
	if (!new_key && !overwrite)
		return YAK_EXISTS;

	if (new_key && entry->value != TOMB)
		tbl->count++;

	entry->key_len = key_len;
	void *key_copy = kmalloc(key_len);
	if (!key_copy)
		return YAK_OOM;
	memcpy(key_copy, key, key_len);
	entry->key = key_copy;
	entry->value = value;

	return YAK_SUCCESS;
}

void *ht_get(struct hashtable *tbl, const void *key, size_t key_len)
{
	if (tbl->count == 0)
		return NULL;

	struct ht_entry *entry =
		find_entry(tbl, tbl->entries, tbl->capacity, key, key_len);

	if (!entry->key)
		return NULL;

	return entry->value;
}

void ht_debug_dump(struct hashtable *tbl)
{
	pr_debug("=== dump hashtable %p ===\n", tbl);
	for (size_t i = 0; i < tbl->capacity; i++) {
		struct ht_entry *entry = &tbl->entries[i];
		if (entry->key)
			pr_debug("%p(%lx) -> %p\n", entry->key, entry->key_len,
				 entry->value);
	}
	pr_debug("=== dump end ===\n");
}

bool ht_del(struct hashtable *tbl, const void *key, size_t key_len)
{
	if (tbl->count == 0)
		return false;

	struct ht_entry *entry =
		find_entry(tbl, tbl->entries, tbl->capacity, key, key_len);

	if (entry->key == NULL)
		return false;

	entry->key = NULL;
	entry->key_len = 0;
	entry->value = TOMB;
	return true;
}

#ifdef _LP64
hash_t ht_hash_str(const void *key, const size_t len)
{
	const char *data = (char *)key;
	uint64_t hash = 0xcbf29ce484222325;
	uint64_t prime = 0x100000001b3;

	for (size_t i = 0; i < len; ++i) {
		uint8_t value = data[i];
		hash = hash ^ value;
		hash *= prime;
	}

	return hash;
}
#else
hash_t ht_hash_str(const void *key, const size_t len)
{
	const char *data = (char *)key;
	uint32_t hash = 0x811c9dc5;
	uint32_t prime = 0x1000193;

	for (size_t i = 0; i < len; ++i) {
		uint8_t value = data[i];
		hash = hash ^ value;
		hash *= prime;
	}

	return hash;
}
#endif

bool ht_eq_str(const void *a, const void *b, const size_t len)
{
	return memcmp(a, b, len) == 0;
}
