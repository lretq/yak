const builtin = @import("builtin");
pub const Arch = switch (builtin.cpu.arch) {
    .x86_64 => @import("arch/x86_64/x86_64.zig").Arch,
    .riscv64 => @import("arch/riscv64/riscv64.zig").Arch,
    else => @compileError("Unsupported architecture"),
};
