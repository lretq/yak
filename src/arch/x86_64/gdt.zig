const std = @import("std");
const yak = @import("../../main.zig");

const Gdtr = packed struct(u80) {
    limit: u16,
    base: u64,
};

const Dpl = enum(u2) {
    Kernel = 0,
    Ring1,
    Ring2,
    User,
};

const Access = packed struct(u8) {
    accessed: bool = false,
    rw: bool = false,
    dc: bool = false,
    executable: bool = false,
    segment: bool = false,
    dpl: Dpl = .Kernel,
    present: bool = false,
};

const GdtEntry = packed struct(u64) {
    limit: u16,
    base_low: u24,
    access: Access,
    granularity: u8,
    base_mid: u8,
};

const TssEntry = extern struct {
    common: GdtEntry align(1),
    base_high: u32 align(1),
    rsv: u32 align(1),
};

pub const GdtIndex = enum(u8) {
    Null = 0,
    KernelCode,
    KernelData,
    UserData,
    UserCode,
    Tss,

    pub fn toSeg(comptime self: @This()) comptime_int {
        return @intFromEnum(self) * 8;
    }
};

const Gdt = extern struct {
    entries: [5]GdtEntry align(1),
    tss: TssEntry align(1),

    fn makeEntry(self: *Gdt, index: GdtIndex, base: u32, limit: u16, access: Access, granularity: u8) void {
        const entry = &self.entries[@intFromEnum(index)];
        entry.limit = limit;
        entry.access = access;
        entry.granularity = granularity;
        entry.base_low = @truncate(base);
        entry.base_mid = @truncate(base >> 24);
    }

    fn makeTssEntry(self: *Gdt) void {
        self.tss.common.limit = 104;
        self.tss.common.access = .{
            .accessed = true,
            .executable = true,
            .present = true,
        };
    }
};

const Tss = packed struct {
    unused0: u32,
    rsp0: u64,
    rsp1: u64,
    rsp2: u64,
    unused1: u64,
    ist1: u64, // NMI
    ist2: u64, // debug/breakpoint
    ist3: u64, // double fault
    ist4: u64,
    ist5: u64,
    ist6: u64,
    ist7: u64,
    unused2: u64,
    iopb: u32,
};

var gdt: Gdt align(16) = undefined;

pub fn initGdt() void {
    @memset(std.mem.asBytes(&gdt), 0);
    gdt.makeEntry(.Null, 0, 0, .{}, 0);
    gdt.makeEntry(.KernelCode, 0, 0, .{
        .accessed = true,
        .rw = true,
        .dc = false,
        .executable = true,
        .segment = true,
        .dpl = .Kernel,
        .present = true,
    }, 0x20);
    gdt.makeEntry(.KernelData, 0, 0, .{
        .accessed = true,
        .rw = true,
        .dc = false,
        .executable = false,
        .segment = true,
        .dpl = .Kernel,
        .present = true,
    }, 0);
    gdt.makeEntry(.UserCode, 0, 0, .{
        .accessed = true,
        .rw = true,
        .dc = false,
        .executable = true,
        .segment = true,
        .dpl = .User,
        .present = true,
    }, 0x20);
    gdt.makeEntry(.UserData, 0, 0, .{
        .accessed = true,
        .rw = true,
        .dc = false,
        .executable = false,
        .segment = true,
        .dpl = .User,
        .present = true,
    }, 0);
    gdt.makeTssEntry();
}

pub fn loadGdt() void {
    var gdtr: Gdtr = .{
        .base = @intFromPtr(&gdt),
        .limit = @sizeOf(Gdt) - 1,
    };

    asm volatile ("lgdt %[p]"
        :
        : [p] "*p" (&gdtr.limit),
    );

    asm volatile (
        \\pushq %[cs]
        \\leaq 1f(%%rip), %%rax
        \\pushq %%rax
        \\lretq
        \\1:
        \\mov %[ds], %%ds
        \\mov %[ds], %%es
        \\mov %[ds], %%ss
        \\mov %[ds], %%gs
        \\mov %[ds], %%fs
        :
        : [cs] "i" (GdtIndex.KernelCode.toSeg()),
          [ds] "rm" (GdtIndex.KernelData.toSeg()),
        : "rax"
    );
}

var tsslock: yak.sync.Spinlock = .init();

pub fn loadTss(tss: *Tss) void {
    const ints = tsslock.lockInts();
    defer tsslock.unlockInts(ints);

    const tss_addr: u64 = @bitCast(tss);
    gdt.tss.common.base_low = @truncate(tss_addr);
    gdt.tss.common.base_mid = @truncate(tss_addr >> 24);
    gdt.tss.common.flags = 0;
    gdt.tss.common.access = .{
        .present = true,
        .executable = true,
        .accessed = true,
    };

    gdt.tss.base_high = @truncate(tss_addr >> 32);
    gdt.tss.rsv = 0;

    asm volatile ("ltr %[p]"
        :
        : [p] "rm16" (GdtIndex.Tss.toSeg()),
        : "memory"
    );
}
