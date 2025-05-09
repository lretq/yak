const std = @import("std");
const yak = @import("../main.zig");
const limine = @import("generic/limine.zig");

pub const mm = @import("riscv64/mm.zig");

pub const Pfn = u52;

pub const PAGE_SHIFT = 12;
pub const PAGE_SIZE = 4096;
pub var MAX_PML_LEVEL: usize = 4;

pub const KERNEL_STACK_SIZE = 4;

pub var HHDM_BASE: u64 = undefined;

pub const BUDDY_ORDERS = 12;

pub var debug_console: *yak.io.console.Console = undefined;

pub fn hcf() noreturn {
    while (true) {
        asm volatile ("wfi");
    }
}

pub inline fn wait_int() void {
    asm volatile ("wfi");
}

pub const SStatus = packed struct(u64) {
    wpri: bool,
    sie: bool,
    other: u62,
};

pub const Satp = packed struct(u64) {
    ppn: u44,
    asid: u16,
    mode: u4,
};

inline fn read_sstatus() SStatus {
    return asm volatile ("csrr %[ret], sstatus"
        : [ret] "=r" (-> SStatus),
    );
}

inline fn write_sstatus(value: SStatus) void {
    asm volatile ("csrw sstatus, %[v]"
        :
        : [v] "r" (@as(u64, @bitCast(value))),
        : "memory"
    );
}

pub inline fn read_satp() Satp {
    return asm volatile ("csrr %[ret], satp"
        : [ret] "=r" (-> Satp),
    );
}

pub inline fn write_satp(value: Satp) void {
    asm volatile ("csrw satp, %[v]"
        :
        : [v] "r" (@as(u64, @bitCast(value))),
        : "memory"
    );
}

pub fn int_status() bool {
    const sstatus = read_sstatus();
    return sstatus.sie;
}

pub fn set_int(state: bool) bool {
    const old = read_sstatus();
    var new = old;
    new.sie = state;
    write_sstatus(new);
    return old.sie;
}

pub const Ipl = enum(u8) {
    Passive = 0,
    Apc = 1,
    Dispatch = 2,
    Device = 7,
    Clock = 13,
    Ipi = 14,
    High = 15,
    _,

    pub fn toIrq(self: @This()) u8 {
        return @intFromEnum(self) << 4;
    }

    pub fn fromIrq(irq: u8) @This() {
        return @enumFromInt(irq >> 4);
    }
};

pub inline fn getIpl() Ipl {
    return Ipl.Passive;
}

pub inline fn setIpl(ipl: Ipl) void {
    _ = ipl;
}

pub const CpuData = struct {};
pub const LocalData = extern struct {
    user_sp: u64 = 0,
    supervisor_sp: u64 = 0,
};

var bsp_cpudata: yak.CpuData = undefined;
var bsp_localdata: yak.LocalData = undefined;

const NS16550 = yak.io.serial.ns16550.NS16550(MmIo);
var serial: NS16550 = .init("serial", .{ .base = 0x10000000 });

pub fn init() !void {
    limine.healthcheck();

    // defined in trap.S
    const trap_asm = @extern(*u64, .{ .name = "__trap" });

    bsp_cpudata.init();
    bsp_localdata.init(&bsp_cpudata);

    asm volatile (
        \\ mv tp, %[data]
        \\ csrw sscratch, zero
        \\ csrw stvec, %[trap]
        :
        : [trap] "r" (trap_asm),
          [data] "r" (@intFromPtr(&bsp_localdata)),
        : "memory"
    );

    limine.early_setup();

    debug_console = &limine.fb_console;

    const paging_mode = limine.paging_mode_request.response.?;
    MAX_PML_LEVEL = switch (paging_mode.mode) {
        .sv39 => 3,
        .sv48 => 4,
        .sv57 => 5,
        else => @panic("invalid paging mode"),
    };

    try limine.initMemory();
    try limine.mapKernel();

    yak.mm.kernel_map.pmap.arch_ctx.activate();

    try yak.mm.kernel_map.pmap.enter(serial.io.base, serial.io.base, .{ .read = true, .write = true }, .Uncached, .{});

    if (serial.configure()) {
        yak.io.console.register(serial.getConsole());
        debug_console = &serial.console;
    }
}

pub const TrapContext = extern struct {
    ra: u64,
    sp: u64,
    gp: u64,
    tp: u64,
    t0: u64,
    t1: u64,
    t2: u64,
    s0: u64,
    s1: u64,
    a0: u64,
    a1: u64,
    a2: u64,
    a3: u64,
    a4: u64,
    a5: u64,
    a6: u64,
    a7: u64,
    s2: u64,
    s3: u64,
    s4: u64,
    s5: u64,
    s6: u64,
    s7: u64,
    s8: u64,
    s9: u64,
    s10: u64,
    s11: u64,
    t3: u64,
    t4: u64,
    t5: u64,
    t6: u64,
    sepc: u64,
    sstatus: u64,
    scause: u64,
    stval: u64,
};

export fn trapHandler(ctx: *TrapContext) callconv(.c) void {
    std.log.info("trap cause: {}", .{ctx.scause});
    std.log.info("in trap handler", .{});
    std.log.info("kernel sp: {X} user sp: {X}", .{ bsp_localdata.md.supervisor_sp, bsp_localdata.md.user_sp });
    std.log.info("trap info: 0x{X}", .{ctx.stval});
    std.log.info("trap addr: 0x{X}", .{ctx.sepc});

    hcf();
}

export fn _start() callconv(.naked) noreturn {
    asm volatile (
        \\ j %[start]
        :
        : [start] "X" (&yak.kentry),
    );
}

pub const MmIo = struct {
    const Self = @This();

    base: usize,

    pub inline fn read(self: Self, comptime T: type, offset: u16) T {
        return @as(*volatile T, @ptrFromInt(self.base + offset)).*;
    }

    pub inline fn write(self: Self, comptime T: type, offset: u16, value: T) void {
        @as(*volatile T, @ptrFromInt(self.base + offset)).* = value;
    }
};
