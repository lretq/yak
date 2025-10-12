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

void hashtable_init(struct hashtable *tbl)
{
	tbl->count = 0;
	tbl->capacity = 0;
	tbl->entries = NULL;
}

void hashtable_destroy(struct hashtable *tbl)
{
	for (size_t i = 0; i < tbl->capacity; i++) {
		struct hashtable_entry *entry = &tbl->entries[i];
		if (entry->key)
			kfree(entry->key, entry->key_len);
	}
	kfree(tbl->entries, sizeof(struct hashtable_entry) * tbl->capacity);
	hashtable_init(tbl);
}

#ifdef _LP64
static inline uint64_t hash_fnv1a(const void *key, const size_t len)
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
static inline uint32_t hash_fnv1a(const void *key, const size_t len)
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

static struct hashtable_entry *find_entry(struct hashtable_entry *entries,
					  int capacity, const char *key)
{
	size_t key_len = strlen(key);
	size_t index = hash_fnv1a(key, key_len) % capacity;

	struct hashtable_entry *tombstone = NULL;

	for (;;) {
		struct hashtable_entry *entry = &entries[index];

		if (entry->key == NULL) {
			if (entry->value == TOMB) {
				if (tombstone == NULL)
					tombstone = entry;
			} else {
				return tombstone != NULL ? tombstone : entry;
			}

		} else if (key_len == entry->key_len &&
			   memcmp(key, entry->key, key_len) == 0) {
			return entry;
		}

		index = (index + 1) % capacity;
	}
}

status_t hashtable_resize(struct hashtable *tbl, size_t new_cap)
{
	struct hashtable_entry *entries =
		kmalloc(new_cap * sizeof(struct hashtable_entry));

	if (!entries)
		return YAK_OOM;

	memset(entries, 0, new_cap * sizeof(struct hashtable_entry));

	tbl->count = 0;

	for (size_t i = 0; i < tbl->capacity; i++) {
		struct hashtable_entry *entry = &tbl->entries[i];
		if (entry->key == NULL)
			continue;

		struct hashtable_entry *dst =
			find_entry(entries, new_cap, entry->key);
		dst->key = entry->key;
		dst->key_len = entry->key_len;
		dst->value = entry->value;

		tbl->count++;
	}

	kfree(tbl->entries, tbl->capacity * sizeof(struct hashtable_entry));

	tbl->entries = entries;
	tbl->capacity = new_cap;

	return YAK_SUCCESS;
}

status_t hashtable_set(struct hashtable *tbl, const char *key, void *value,
		       int overwrite)
{
	// max load of 0.75
	// count + 1 > capacity * 0.75 | * 4
	if ((tbl->count + 1) * 4 > tbl->capacity * 3) {
		size_t new_cap = tbl->capacity == 0 ? 16 : tbl->capacity * 2;
		status_t res = hashtable_resize(tbl, new_cap);
		IF_ERR(res)
		{
			return res;
		}
	}

	struct hashtable_entry *entry =
		find_entry(tbl->entries, tbl->capacity, key);
	int new_key = (entry->key == NULL);
	if (!new_key && !overwrite)
		return YAK_EXISTS;

	if (new_key && entry->value != TOMB)
		tbl->count++;

	entry->key_len = strlen(key);
	entry->key = strndup(key, entry->key_len + 1);
	entry->value = value;

	return YAK_SUCCESS;
}

void *hashtable_get(struct hashtable *tbl, const char *key)
{
	if (tbl->count == 0)
		return NULL;

	struct hashtable_entry *entry =
		find_entry(tbl->entries, tbl->capacity, key);

	if (!entry->key)
		return NULL;

	return entry->value;
}

void hashtable_dump(struct hashtable *tbl)
{
	pr_debug("=== dump hashtable %p ===\n", tbl);
	for (size_t i = 0; i < tbl->capacity; i++) {
		struct hashtable_entry *entry = &tbl->entries[i];
		if (entry->key)
			pr_debug("%s -> %p\n", entry->key, entry->value);
	}
	pr_debug("=== dump end ===\n");
}

int hashtable_delete(struct hashtable *tbl, const char *key)
{
	if (tbl->count == 0)
		return 0;

	struct hashtable_entry *entry =
		find_entry(tbl->entries, tbl->capacity, key);

	if (entry->key == NULL)
		return 0;

	entry->key = NULL;
	entry->key_len = 0;
	entry->value = TOMB;
	return 1;
}
