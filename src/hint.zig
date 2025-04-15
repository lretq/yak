const builtin = @import("builtin");

pub inline fn spinLoop() void {
    switch (builtin.cpu.arch) {
        .x86, .x86_64 => {
            asm volatile ("pause");
        },
        .aarch64 => {
            asm volatile ("isb sy");
        },
        .riscv32, .riscv64 => {
            asm volatile ("pause");
        },
        else => {},
    }
}
