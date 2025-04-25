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

const ipl = @import("ipl.zig");
pub const Ipl = ipl.Ipl;
pub const raiseIpl = ipl.raiseIpl;
pub const lowerIpl = ipl.lowerIpl;

pub const mm = @import("mm/mm.zig");
pub const pm = @import("pm.zig");

pub const hint = @import("hint.zig");

pub const bithacks = @import("bithacks.zig");

pub const std_options: std.Options = .{
    .log_level = .debug,
    .logFn = kernelLogFn,
};

pub const LogLevel = std.log.Level;

pub const log_levels = struct {
    pub var default: LogLevel = .debug;
};

pub fn kernelLogFn(
    comptime level: std.log.Level,
    comptime scope: @Type(.enum_literal),
    comptime format: []const u8,
    args: anytype,
) void {
    const ansi = true;

    const scope_name = @tagName(scope);
    if (comptime @hasDecl(log_levels, scope_name)) {
        if (@intFromEnum(level) > @intFromEnum(@field(log_levels, scope_name))) return;
    }

    const scope_prefix = comptime if (scope != .default)
        "(" ++ scope_name ++ ")"
    else
        "unscoped";

    const ansi_default = comptime if (ansi) "\x1b[39m" else "";
    const ansi_white = comptime if (ansi) "\x1b[37m" else "";
    const color_codes = comptime if (ansi) switch (level) {
        .debug => "\x1b[35m",
        .info => "\x1b[32m",
        .warn => "\x1b[33m",
        .err => "\x1b[31m",
    } else "";

    const prefix = comptime ansi_white ++ "[" ++ color_codes ++ level.asText() ++ ansi_white ++ "] " ++ scope_prefix ++ ": ";

    const writer = comptime io.console.getConsoleWriter();

    const old = arch.set_int(false);
    defer _ = arch.set_int(old);

    writer.print(prefix, .{}) catch return;
    writer.print(ansi_default ++ format ++ "\n", args) catch return;
}

pub const panic = std.debug.FullPanic(kernelPanicFn);

fn kernelPanicFn(msg: []const u8, first_trace_addr: ?usize) noreturn {
    // TODO: Proper panic
    // Bypass std.log as we may not have access to these facilities (anymore)
    // Something like arch.panic_logger ??
    // Also, IPI Panic to other cores (maybe arch.panicOthers??)
    _ = first_trace_addr;
    std.log.err("kernel panic: {s}", .{msg});
    arch.hcf();
}

export fn kentry() noreturn {
    arch.init();
    arch.hcf();
}
