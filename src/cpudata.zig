const std = @import("std");
const yak = @import("main.zig");
const arch = yak.arch;

pub const CpuData = extern struct {
    md: arch.CpuData,

    pub fn init(self: *CpuData) void {
        if (comptime std.meta.hasFn(arch.CpuData, "init")) {
            self.md.init();
        } else {
            self.md = .{};
        }
    }
};

pub const LocalData = extern struct {
    md: arch.LocalData,
    cpu: *CpuData,

    pub fn init(self: *LocalData, cpu: *CpuData) void {
        if (comptime std.meta.hasFn(arch.LocalData, "init")) {
            self.md.init();
        } else {
            self.md = .{};
        }

        self.cpu = cpu;
    }
};
