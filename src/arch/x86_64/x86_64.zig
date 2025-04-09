const limine = @import("../generic/limine/limine.zig");

pub const Arch = struct {
    pub fn hcf() noreturn {
        while (true) {
            asm volatile ("hlt");
        }
    }

    pub fn init() void {
        limine.healthcheck();

        if (limine.framebuffer_request.response) |framebuffer_response| {
            const framebuffer = framebuffer_response.getFramebuffers()[0];
            for (0..100) |i| {
                const fb_ptr: [*]volatile u32 = @ptrCast(@alignCast(framebuffer.address));
                fb_ptr[i * (framebuffer.pitch / 4) + i] = 0xffffff;
            }
        } else {
            @panic("Framebuffer response not present");
        }
    }
};
