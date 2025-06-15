#include <yak/spinlock.h>
#include <yak/queue.h>
#include <yak/io/console.h>

SPINLOCK(console_list_lock);
struct console_list console_list = TAILQ_HEAD_INITIALIZER(console_list);

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
	TAILQ_INSERT_TAIL(&console_list, console, list_entry);
	spinlock_unlock_interrupts(&console_list_lock, state);
}

void console_foreach(void (*cb)(struct console *, void *), void *private)
{
	struct console *console;

	console_lock();
	TAILQ_FOREACH(console, &console_list, list_entry)
	{
		cb(console, private);
	}
	console_unlock();
}
