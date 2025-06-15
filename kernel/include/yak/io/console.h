#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <yak/queue.h>

struct console {
	char name[32];

	size_t (*write)(struct console *console, const char *buffer,
			size_t length);

	void *private;
	TAILQ_ENTRY(console) list_entry;
};

TAILQ_HEAD(console_list, console);

extern struct console_list console_list;

void console_lock();
void console_unlock();

void console_register(struct console *console);
void console_foreach(void (*cb)(struct console *, void *), void *private);

#ifdef __cplusplus
}
#endif
