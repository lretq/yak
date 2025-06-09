#include <yak/spinlock.h>
#include <yak/list.h>
#include <yak/io/console.h>

SPINLOCK(console_list_lock);
LIST_HEAD(console_list);

void console_lock()
{
	spinlock_lock_noipl(&console_list_lock);
}

void console_unlock()
{
	spinlock_unlock_noipl(&console_list_lock);
}

void console_register(struct console *console)
{
	int state = spinlock_lock_interrupts(&console_list_lock);
	list_init(&console->list_entry);
	list_add_tail(&console_list, &console->list_entry);
	spinlock_unlock_interrupts(&console_list_lock, state);
}

void console_foreach(void (*cb)(struct console *, void *), void *private)
{
	struct list_head *el;

	console_lock();
	list_for_each(el, &console_list) {
		struct console *cons =
			list_entry(el, struct console, list_entry);
		cb(cons, private);
	}
	console_unlock();
}
