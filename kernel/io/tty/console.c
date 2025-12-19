#include <yak/tty.h>

void console_backend_setup();
void console_backend_write(const void *buf, size_t length);

struct tty *console_tty;

static ssize_t console_write(struct tty *tty, const char *buf, size_t len)
{
	(void)tty;
	console_backend_write(buf, len);
	return len;
}

static struct tty_driver_ops console_ops = {
	.write = console_write,
	.set_termios = NULL,
	.flush = NULL,
};

void console_init()
{
	console_backend_setup();
	console_tty = tty_create("console", &console_ops, NULL);
}
