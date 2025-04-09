const limine = @import("../generic/limine/limine.zig");

pub const Arch = struct {
    pub fn hcf() noreturn {
        while (true) {
            asm volatile ("hlt");
        }
    }

    pub fn enable_interrupts() void {
        asm volatile ("sti");
    }

    pub fn disable_interrupts() void {
        asm volatile ("cli");
    }

    pub fn init() void {
        limine.healthcheck();

        if (limine.framebuffer_request.response) |framebuffer_response| {
            const framebuffer = framebuffer_response.getFramebuffers()[0];
            for (0..100) |i| {
                const fb_ptr: [*]volatile u32 = @ptrCast(@alignCast(framebuffer.address));
                const hue: u16 = @intCast(i * 1535 / 100);
                const color = rainbowColor(hue);
                fb_ptr[i * (framebuffer.pitch / 4) + i] = color;
            }
        } else {
            @panic("Framebuffer response not present");
        }
    }
};

fn rainbowColor(hue: u16) u32 {
    const segment: u8 = @intCast(hue / 256);
    const offset: u8 = @intCast(hue % 256);

    var r: u8 = 0;
    var g: u8 = 0;
    var b: u8 = 0;

    switch (segment) {
        0 => {
            r = 255;
            g = offset;
            b = 0;
        }, // Red → Yellow
        1 => {
            r = 255 - offset;
            g = 255;
            b = 0;
        }, // Yellow → Green
        2 => {
            r = 0;
            g = 255;
            b = offset;
        }, // Green → Cyan
        3 => {
            r = 0;
            g = 255 - offset;
            b = 255;
        }, // Cyan → Blue
        4 => {
            r = offset;
            g = 0;
            b = 255;
        }, // Blue → Magenta
        5 => {
            r = 255;
            g = 0;
            b = 255 - offset;
        }, // Magenta → Red
        else => {
            r = 255;
            g = 0;
            b = 0;
        }, // fallback to red
    }

    return (@as(u32, r) << 16) | (@as(u32, g) << 8) | b;
}
