const std = @import("std");
const yak = @import("../../main.zig");
const arch = @import("../x86_64.zig");

const CacheBits = packed struct(u3) {
    pwt: bool,
    pcd: bool,
    pat: bool,

    pub fn toBits(mode: yak.mm.CacheMode) CacheBits {
        var pwt = false;
        var pcd = false;
        var pat = false;
        switch (mode) {
            .WriteCombine => {
                pat = true;
                pwt = true;
            },
            .WriteThrough => {
                pwt = true;
            },
            .WriteBack => {},
            .Uncached => {
                pcd = true;
                pwt = true;
            },
        }

        return .{ .pwt = pwt, .pcd = pcd, .pat = pat };
    }
};

pub const Pte = packed struct(u64) {
    present: bool = true,
    write: bool,
    user: bool,
    pwt: bool,
    pcd: bool,
    accessed: bool = false,
    dirty: bool = false,
    pat_ps: bool,
    global: bool,
    avl: u3 = 0,
    address: packed union {
        large_page: packed struct(u40) {
            pat: bool,
            _rsv: u17,
            addr: u22,
        },
        small_page: packed struct(u40) {
            addr: u40,
        },
    },
    avl2: u7 = 0,
    mkey: u4 = 0,
    no_execute: bool,

    pub fn makeLarge(pa: usize, prot: yak.mm.MapFlags, cache: yak.mm.CacheMode) Pte {
        const bits = CacheBits.toBits(cache);
        return .{
            .present = prot.read,
            .write = prot.write,
            .user = prot.user,
            .pwt = bits.pwt,
            .pcd = bits.pcd,
            .global = prot.global,
            .pat_ps = true,
            .address = .{
                .large_page = .{
                    .pat = bits.pat,
                    ._rsv = 0,
                    .addr = @truncate(pa >> 30),
                },
            },
            .no_execute = !prot.execute,
        };
    }

    pub fn makeSmall(pa: usize, prot: yak.mm.MapFlags, cache: yak.mm.CacheMode) Pte {
        const bits = CacheBits.toBits(cache);
        return .{
            .present = prot.read,
            .write = prot.write,
            .user = prot.user,
            .pwt = bits.pwt,
            .pcd = bits.pcd,
            .global = prot.global,
            .pat_ps = bits.pat,
            .address = .{
                .small_page = .{
                    .addr = @truncate(pa >> arch.PAGE_SHIFT),
                },
            },
            .no_execute = !prot.execute,
        };
    }

    pub fn setAddress(self: *Pte, pa: usize, large: bool) void {
        if (large) {
            self.address.large_page.addr = @truncate(pa >> 30);
        }
        self.address.small_page.addr = @truncate(pa >> arch.PAGE_SHIFT);
    }

    pub fn getAddress(self: *Pte, large: bool) usize {
        if (large) {
            return @as(usize, self.address.large_page.addr) << 30;
        }
        return @as(usize, self.address.small_page.addr) << arch.PAGE_SHIFT;
    }

    pub fn next_pml(self: *Pte) *Pml {
        return @ptrFromInt(self.getAddress(false) + arch.HHDM_BASE);
    }
};

const Pml = extern struct {
    entries: [512]Pte,
};

const WalkFlags = packed struct {
    allocate: bool,
    // pagesize pow2
    pagesize: u8 = arch.PAGE_SHIFT,
};

inline fn mapIndex(addr: usize, level: usize) usize {
    @setRuntimeSafety(false);
    // 512 entries per level = 9 bits
    return ((addr >> @intCast(arch.PAGE_SHIFT + 9 * level)) & 0x1ff);
}

pub const Context = struct {
    cr3: usize = 0,

    pub fn init(self: *Context) !void {
        const page = try yak.pm.allocPages(0);
        self.cr3 = page.toAddress();
    }

    pub fn activate(self: *Context) void {
        asm volatile ("mov %[p], %%cr3"
            :
            : [p] "r" (self.cr3),
            : "memory"
        );
    }

    pub fn deinit(self: *Context) void {
        yak.pm.page_allocator
            .destroy(@ptrFromInt(arch.HHDM_BASE + self.cr3));
    }

    pub fn traverse(self: *Context, va: usize, flags: WalkFlags) !?*Pte {
        var current_pml: *Pml = @ptrFromInt(self.cr3 + arch.HHDM_BASE);

        // change when implementing 5lvl
        const max_level = 3;
        var level: usize = max_level;
        while (level > 0) : (level -= 1) {
            const index = mapIndex(va, level);
            var pte = &current_pml.entries[index];

            if ((level == 2 and flags.pagesize == 30) or (level == 1 and flags.pagesize == 21)) {
                return pte;
            } else if (pte.pat_ps) {
                std.log.warn("found large page but should not return", .{});
                return null;
            }

            if (!pte.present) {
                if (!flags.allocate) return null;
                const page = try yak.pm.allocPages(0);

                pte.* = .{
                    .user = true,
                    .write = true,
                    .global = false,
                    .address = undefined,
                    .pwt = false,
                    .pat_ps = false,
                    .pcd = false,
                    .no_execute = false,
                };

                pte.setAddress(page.toAddress(), false);
            }

            current_pml = pte.next_pml();
        }

        return &current_pml.entries[mapIndex(va, 0)];
    }
};

pub inline fn invalidate(va: usize) void {
    asm volatile ("invlpg %[p]"
        :
        : [p] "m" (va),
        : "memory"
    );
}
