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

const SStatus = packed struct(u64) {
    wpri: bool,
    sie: bool,
    other: u62,
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

const NS16550 = yak.io.serial.ns16550.NS16550(MmIo);
var serial: NS16550 = undefined;

pub fn init() !void {
    limine.healthcheck();
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
        return @as(*T, @ptrFromInt(self.base + offset)).*;
    }

    pub inline fn write(self: Self, comptime T: type, offset: u16, value: T) void {
        @as(*T, @ptrFromInt(self.base + offset)).* = value;
    }
};
