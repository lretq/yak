const std = @import("std");
const builtin = @import("builtin");
const yak = @import("../main.zig");
const mm = yak.mm;
const arch = yak.arch;

pub const Pmap = struct {
    arch_ctx: arch.mm.Context,

    pub fn init(self: *Pmap) !void {
        try self.arch_ctx.init();
    }

    pub fn enter(self: *Pmap, va: usize, pa: usize, prot: mm.MapFlags, cache: mm.CacheMode) !void {
        switch (builtin.cpu.arch) {
            .x86_64 => {
                std.debug.assert(prot.pagesize == 12 or prot.pagesize == 21 or prot.pagesize == 30);
            },
            else => unreachable,
        }

        if (!std.mem.isAlignedLog2(va, prot.pagesize) or !std.mem.isAlignedLog2(pa, prot.pagesize)) @panic("tried to map unaligned page");

        const res = try self.arch_ctx.traverse(va, .{
            .allocate = true,
            .pagesize = prot.pagesize,
        });

        if (res == null) @panic("what");

        const pte = res.?;
        if (prot.pagesize != arch.PAGE_SHIFT) {
            pte.* = arch.mm.Pte.makeLarge(pa, prot, cache);
        } else {
            pte.* = arch.mm.Pte.makeSmall(pa, prot, cache);
        }

        arch.mm.invalidate(va);
    }
};
