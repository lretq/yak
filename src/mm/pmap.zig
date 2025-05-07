const std = @import("std");
const builtin = @import("builtin");
const yak = @import("../main.zig");
const mm = yak.mm;
const arch = yak.arch;

const utils = yak.utils;

pub const Pmap = struct {
    arch_ctx: arch.mm.Context = .{},

    pub fn init(self: *Pmap) !void {
        try self.arch_ctx.init();
    }

    pub fn enter(self: *Pmap, va: usize, pa: usize, prot: mm.MapFlags, cache: mm.CacheMode, flags: mm.MiscFlags) !void {
        switch (builtin.cpu.arch) {
            .x86_64 => {
                std.debug.assert(flags.pagesize == 12 or flags.pagesize == 21 or flags.pagesize == 30);
            },
            .riscv64 => {
                std.debug.assert(flags.pagesize == 12 or flags.pagesize == 21 or flags.pagesize == 30 or flags.pagesize == 39 or flags.pagesize == 48);
            },
            else => @compileError("unsupported VM architecture"),
        }

        if (!std.mem.isAlignedLog2(va, flags.pagesize) or !std.mem.isAlignedLog2(pa, flags.pagesize)) @panic("tried to map unaligned page");

        const res = try self.arch_ctx.traverse(va, .{
            .allocate = true,
            .pagesize = flags.pagesize,
        });

        if (res == null) @panic("what");

        const pte = res.?;
        if (flags.pagesize != arch.PAGE_SHIFT) {
            pte.* = arch.mm.Pte.makeLarge(pa, prot, cache);
        } else {
            pte.* = arch.mm.Pte.makeSmall(pa, prot, cache);
        }

        arch.mm.invalidate(va);
    }

    pub fn map_large_range(self: *Pmap, base: usize, length: usize, va_base: usize, prot: mm.MapFlags, cache: mm.CacheMode) !void {
        const end = base + length;

        var addr: usize = base;
        while (addr < end) {
            if (std.mem.isAlignedLog2(addr, 21)) {
                break;
            }

            try self.enter(va_base + addr, addr, prot, cache, .{});
            addr += arch.PAGE_SIZE;
        }

        while (addr < end) {
            if (std.mem.isAlignedLog2(addr, 30) and (addr + utils.GiB(1) <= end)) {
                try self.enter(va_base + addr, addr, prot, cache, .{ .pagesize = 30 });
                addr += utils.GiB(1);
            } else if (std.mem.isAlignedLog2(addr, 21) and (addr + utils.MiB(2) <= end)) {
                try self.enter(va_base + addr, addr, prot, cache, .{ .pagesize = 21 });
                addr += utils.MiB(2);
            } else {
                try self.enter(va_base + addr, addr, prot, cache, .{});
                addr += arch.PAGE_SIZE;
            }
        }

        std.debug.assert(addr == end);
    }
};
