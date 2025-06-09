#pragma once

#include <stddef.h>
#include <yak/list.h>

struct console {
	char name[32];

	size_t (*write)(struct console *console, const char *buffer,
			size_t length);

	void *private;
	struct list_head list_entry;
};

extern struct list_head console_list;

void console_lock();
void console_unlock();

void console_register(struct console *console);
void console_foreach(void (*cb)(struct console *, void *), void *private);
