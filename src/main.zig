const builtin = @import("builtin");
const std = @import("std");

pub const utils = @import("utils.zig");

pub const arch = switch (builtin.cpu.arch) {
    .x86_64 => @import("arch/x86_64.zig"),
    .riscv64 => @import("arch/riscv64.zig"),
    else => @compileError("Unsupported architecture"),
};

pub const io = @import("io/io.zig");

export fn kentry() noreturn {
    arch.init();
    arch.hcf();
}
