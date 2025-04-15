const builtin = @import("builtin");
const std = @import("std");

pub const utils = @import("utils.zig");

pub const arch = switch (builtin.cpu.arch) {
    .x86_64 => @import("arch/x86_64.zig"),
    .riscv64 => @import("arch/riscv64.zig"),
    else => @compileError("Unsupported architecture"),
};

pub const io = @import("io/io.zig");

pub const sync = @import("sync/sync.zig");

pub const ipl = @import("ipl.zig");

pub const hint = @import("hint.zig");

pub const std_options: std.Options = .{
    .log_level = .debug,
    .logFn = kernelLogFn,
};

pub fn kernelLogFn(
    comptime level: std.log.Level,
    comptime scope: @Type(.enum_literal),
    comptime format: []const u8,
    args: anytype,
) void {
    const scope_prefix = "(" ++ comptime @tagName(scope) ++ "): ";
    const prefix = "[" ++ comptime level.asText() ++ "] " ++ scope_prefix;

    // Print the message, silently ignoring any errors
    const writer = io.console.getConsoleWriter();
    nosuspend writer.print(prefix ++ format ++ "\n", args) catch return;
}

export fn kentry() noreturn {
    arch.init();
    std.log.info("info log test", .{});
    std.log.debug("debug log test", .{});
    arch.hcf();
}
