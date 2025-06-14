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

// scale by 2, builtin font height = 16?, 3 kinfo lines
// 10 margin
#define KINFO_MARGIN 5
#define KINFO_HEIGHT(fontscale) (16 * 3 * fontscale + KINFO_MARGIN * 2)

void limine_fb_setup()
{
	struct limine_framebuffer_response *res = fb_request.response;
	for (size_t i = 0; i < res->framebuffer_count; i++) {
		struct limine_framebuffer *fb = res->framebuffers[i];

		struct console *console = kcalloc(sizeof(struct console));

		npf_snprintf(console->name, sizeof(console->name) - 1,
			     "limine-fb%ld", i);

		console->write = fb_write;

		size_t font_scale_x = 1, font_scale_y = 1;

		if (fb->width >= (1920 + 1920 / 3) &&
		    fb->height >= (1080 + 1080 / 3)) {
			font_scale_x = 2;
			font_scale_y = 2;
		}
		if (fb->width >= (3840 + 3840 / 3) &&
		    fb->height >= (2160 + 2160 / 3)) {
			font_scale_x = 4;
			font_scale_y = 4;
		}

		size_t kinfo_height = KINFO_HEIGHT(font_scale_y);

		console->private = flanterm_fb_init(
			kcalloc, kfree, fb->address, fb->width,
			fb->height - kinfo_height, fb->pitch, fb->red_mask_size,
			fb->red_mask_shift, fb->green_mask_size,
			fb->green_mask_shift, fb->blue_mask_size,
			fb->blue_mask_shift, NULL, NULL, NULL, NULL, NULL, NULL,
			NULL, NULL, 0, 0, 1, font_scale_x, font_scale_y, 0);
		console_register(console);

		void *footer_address =
			(void *)((uintptr_t)fb->address +
				 (fb->pitch * (fb->height - kinfo_height)));

		uint32_t kinfo_fg = 0xcdd6f4;
		uint32_t kinfo_bg = 0x45475a;
		kinfo_footer_ctx = flanterm_fb_init(
			kcalloc, kfree, footer_address, fb->width, kinfo_height,
			fb->pitch, fb->red_mask_size, fb->red_mask_shift,
			fb->green_mask_size, fb->green_mask_shift,
			fb->blue_mask_size, fb->blue_mask_shift, NULL, NULL,
			NULL, &kinfo_bg, &kinfo_fg, NULL, NULL, NULL, 0, 0, 1,
			font_scale_x, font_scale_y, KINFO_MARGIN);

		break;
	}
}

static size_t fb_write(struct console *console, const char *buf, size_t size)
{
	flanterm_write(console->private, buf, size);
	return size;
}
