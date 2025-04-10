const builtin = @import("builtin");
const limine = @import("generic/limine.zig");

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
            fb_ptr[i * (framebuffer.pitch / 4) + i] = 0xFF00FF;
        }
    } else {
        @panic("Framebuffer response not present");
    }
}
