#include <stddef.h>
#include <limine.h>
#include <nanoprintf.h>
#include <flanterm.h>
#include <flanterm_backends/fb.h>
#include <yak/heap.h>
#include <yak/spinlock.h>
#include <yak/io/console.h>
#include "request.h"

LIMINE_REQ
volatile struct limine_framebuffer_request fb_request = {
	.id = LIMINE_FRAMEBUFFER_REQUEST,
	.revision = 0,
};

static size_t fb_write(struct console *console, const char *buf, size_t size);

struct flanterm_context *kinfo_footer_ctx;
#define KINFO_HEIGHT 40

void limine_fb_setup()
{
	struct limine_framebuffer_response *res = fb_request.response;
	for (size_t i = 0; i < res->framebuffer_count; i++) {
		struct limine_framebuffer *fb = res->framebuffers[i];

		struct console *console = kmalloc(sizeof(struct console));

		npf_snprintf(console->name, sizeof(console->name) - 1,
			     "limine-fb%ld", i);

		console->write = fb_write;
		console->private = flanterm_fb_init(
			kmalloc, kfree, fb->address, fb->width,
			fb->height - KINFO_HEIGHT, fb->pitch, fb->red_mask_size,
			fb->red_mask_shift, fb->green_mask_size,
			fb->green_mask_shift, fb->blue_mask_size,
			fb->blue_mask_shift, NULL, NULL, NULL, NULL, NULL, NULL,
			NULL, NULL, 0, 0, 1, 0, 0, 0);
		console_register(console);

		void *footer_address =
			(void *)((uintptr_t)fb->address +
				 (fb->pitch * (fb->height - KINFO_HEIGHT)));

		uint32_t kinfo_fg = 0xcdd6f4;
		uint32_t kinfo_bg = 0x45475a;
		kinfo_footer_ctx = flanterm_fb_init(
			kmalloc, kfree, footer_address, fb->width, KINFO_HEIGHT,
			fb->pitch, fb->red_mask_size, fb->red_mask_shift,
			fb->green_mask_size, fb->green_mask_shift,
			fb->blue_mask_size, fb->blue_mask_shift, NULL, NULL,
			NULL, /* default bg */ &kinfo_bg, &kinfo_fg, NULL, NULL,
			NULL, 0, 0, 1, 0, 0, 0);

		break;
	}
}

static size_t fb_write(struct console *console, const char *buf, size_t size)
{
	flanterm_write(console->private, buf, size);
	return size;
}
