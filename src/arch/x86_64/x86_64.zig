const builtin = @import("builtin");
const limine = @import("../generic/limine.zig");

const util = @import("../../util.zig");

pub const Arch = struct {
    pub const PAGE_SIZE = 4096;
    pub const PAGE_SHIFT = 12;

    pub const IRQ_SLOTS = 256 - 32;
    pub const IRQ_SLOTS_PER_IPL = 16;

    pub const KERNEL_STACK_SIZE = 4; // = 16KiB

    pub const PFNDB_BASE = 0xFFFFFFFa00000000;
    pub const KERNEL_ARENA_BASE = 0xFFFFFFFb00000000;

    pub fn hcf() noreturn {
        while (true) {
            asm volatile ("hlt");
        }
        @trap();
    }

    pub fn wait_int() void {
        asm volatile ("hlt");
    }

    pub fn int_status() bool {
        const rflags =
            asm volatile (
                \\pushfq
                \\pop %[ret]
                : [ret] "=r" (-> u64),
                :
                : "memory"
            );
        return (rflags & (1 << 9)) != 0;
    }

    pub fn set_int(state: bool) bool {
        const old = int_status();
        if (comptime state) {
            asm volatile ("sti");
        } else {
            asm volatile ("cli");
        }
        return old;
    }

    pub fn init() void {
        limine.healthcheck();
        _ = int_status();

        if (limine.framebuffer_request.response) |framebuffer_response| {
            const framebuffer = framebuffer_response.getFramebuffers()[0];
            for (0..150) |i| {
                const fb_ptr: [*]volatile u32 = @ptrCast(@alignCast(framebuffer.address));
                const hue: u16 = @intCast(i * 1535 / 150);
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
