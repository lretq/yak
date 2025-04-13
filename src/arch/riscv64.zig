const limine = @import("generic/limine.zig");

pub const PAGE_SIZE = 4096;
pub const KERNEL_STACK_SIZE = 4;

pub fn hcf() noreturn {
    while (true) {
        asm volatile ("wfi");
    }
}

pub fn init() void {
    limine.healthcheck();
}
