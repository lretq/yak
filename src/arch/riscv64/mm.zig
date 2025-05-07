const std = @import("std");
const yak = @import("../../main.zig");
const arch = @import("../riscv64.zig");

// if rwx==0b000 then addr points to next pml
pub const Pte = packed struct(u64) {
    valid: bool,
    read: bool,
    write: bool,
    execute: bool,
    user: bool,
    global: bool,
    hw_access: bool = false,
    hw_dirty: bool = false,
    rsw: u2 = 0,
    physical_address: u51,
    pbmt: u2 = 0,
    n: u1 = 0,

    pub fn makeSmall(pa: usize, prot: yak.mm.MapFlags, cache: yak.mm.CacheMode) Pte {
        _ = cache;
        return .{
            .valid = true,
            .read = prot.read,
            .write = prot.write,
            .execute = prot.execute,
            .user = prot.user,
            .global = prot.global,
            .physical_address = @truncate(pa >> arch.PAGE_SHIFT),
        };
    }

    pub const makeLarge = makeSmall;

    pub fn setAddress(self: *Pte, pa: usize, large: bool) void {
        _ = large;
        self.physical_address = @truncate(pa >> arch.PAGE_SHIFT);
    }

    pub fn getAddress(self: *Pte, large: bool) usize {
        _ = large;
        return @as(usize, self.physical_address) << arch.PAGE_SHIFT;
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

fn mapIndex(va: usize, level: usize) u64 {
    _ = va;
    _ = level;
    return 0;
}

pub const Context = struct {
    top_level: usize = 0,

    pub fn init(self: *Context) !void {
        const page = try yak.pm.allocPages(0);
        self.top_level = page.toAddress();
    }

    pub fn deinit(self: *Context) void {
        std.debug.assert(self.cr3 > 0);
        yak.pm.freePages(yak.pm.lookupPage(self.cr3, 0).?);
    }

    pub fn traverse(self: *Context, va: usize, flags: WalkFlags) !?*Pte {
        var current_pml: *Pml = @ptrFromInt(self.top_level + arch.HHDM_BASE);

        const level_for_pagesize = (flags.pagesize - 12) / 9;

        // based on sv{39, 48, 57}
        var level: usize = arch.MAX_PML_LEVEL - 1;
        while (level > 0) : (level -= 1) {
            const index = mapIndex(va, level);

            var pte = &current_pml.entries[index];

            if (level == level_for_pagesize) {
                break;
            }

            if (!pte.valid) {
                if (!flags.allocate) return null;
                const page = try yak.pm.allocPages(0);

                pte.* = .{
                    .valid = true,
                    .read = false,
                    .user = false,
                    .execute = false,
                    .write = false,
                    .global = false,
                    .physical_address = undefined,
                };

                pte.setAddress(page.toAddress(), false);
            }

            current_pml = pte.next_pml();
        }

        return &current_pml.entries[mapIndex(va, level)];
    }
};

pub fn invalidate(va: usize) void {
    asm volatile ("sfence.vma %[p]"
        :
        : [p] "r" (va),
        : "memory"
    );
}
