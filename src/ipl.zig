const std = @import("std");
const yak = @import("main.zig");
const arch = yak.arch;

pub const Ipl = arch.Ipl;

pub fn raiseIpl(level: Ipl) Ipl {
    const old = arch.getIpl();
    if (@intFromEnum(level) < @intFromEnum(old)) {
        @panic("Raise IPL to less than current");
    }
    arch.setIpl(level);
    return old;
}

pub fn lowerIpl(level: Ipl) void {
    const old = arch.getIpl();
    if (@intFromEnum(level) > @intFromEnum(old)) {
        @panic("Restore IPL to greater than current");
    }
    arch.setIpl(level);
    // XXX: check for softints
}
