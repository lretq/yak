const arch = @import("../x86_64.zig");
const yak = @import("../../main.zig");

const IdtEntry = packed struct(u128) {
    isr_low: u16 = 0,
    kernel_cs: u16 = 0,
    ist: u8 = 0,
    attributes: u8 = 0,
    isr_high: u48 = 0,
    rsv: u32 = 0,

    fn makeEntry(self: *@This(), isr: usize) void {
        self.isr_low = @truncate(isr);
        self.kernel_cs = arch.gdt.GdtIndex.KernelCode.toSeg();
        self.ist = 0;
        self.attributes = 0x8E;
        self.isr_high = @truncate(isr >> 16);
        self.rsv = 0;
    }

    fn setIst(self: *@This(), ist: u8) void {
        self.ist = ist;
    }
};

const Idtr = packed struct(u80) {
    limit: u16,
    base: u64,
};

var idt: [256]IdtEntry = undefined;
extern const itable: [256]u64;

const std = @import("std");

pub fn init() void {
    for (&idt, 0..) |*ent, i| {
        ent.makeEntry(itable[i]);
    }

    // NMI
    idt[2].setIst(1);
    // debug & breakpoint
    idt[1].setIst(2);
    idt[3].setIst(2);
    // double fault
    idt[8].setIst(3);
}

pub fn load() void {
    const idtr: Idtr = .{
        .limit = @sizeOf(@TypeOf(idt)) - 1,
        .base = @intFromPtr(&idt[0]),
    };

    asm volatile ("lidt %[p]"
        :
        : [p] "*p" (&idtr.limit),
    );
}

const faultNames = .{ "Divide by Zero", "Debug", "NMI", "Breakpoint", "Overflow", "Bound Range Exceeded", "Invalid Opcode", "Device not available", "Double Fault", "", "Invalid TSS", "Segment not present", "Stack Segment fault", "GPF", "Page Fault", "", "x87 Floating point exception", "Alignment check", "Machine check", "SIMD Floating point exception", "Virtualization Exception", "Control protection exception", "", "", "", "", "", "", "Hypervisor injection exception", "VMM Communication exception", "Security exception", "" };

const RawContext = packed struct {
    rax: u64,
    rbx: u64,
    rcx: u64,
    rdx: u64,
    rsi: u64,
    rdi: u64,
    r8: u64,
    r9: u64,
    r10: u64,
    r11: u64,
    r12: u64,
    r13: u64,
    r14: u64,
    r15: u64,
    rbp: u64,

    rip: u64,
    cs: u64,
    rflags: u64,
    rsp: u64,
    ss: u64,

    pub fn format(
        self: @This(),
        comptime fmt: []const u8,
        options: std.fmt.FormatOptions,
        writer: anytype,
    ) !void {
        _ = options;
        _ = fmt;
        try fmtCtx(RawContext, self, writer);
    }
};

fn fmtCtx(comptime T: type, self: T, writer: anytype) !void {
    switch (@typeInfo(T)) {
        .@"struct" => |info| {
            try writer.writeAll(@typeName(T));
            try writer.writeAll("{\n");
            inline for (info.fields, 0..) |f, i| {
                if (i != 0 and i % 2 == 0) {
                    try writer.writeAll("\n");
                }
                try writer.writeAll("  ");
                try writer.print("{s: <6}", .{f.name});
                try writer.writeAll(" = ");
                try writer.print("0x{X:0>16}", .{@field(self, f.name)});
            }
            try writer.writeAll("\n}");
        },
        else => @compileError("cannot format non-struct"),
    }
}

fn numToFault(num: u8) []const u8 {
    return switch (num) {
        0 => "Division Error",
        1 => "Debug",
        2 => "Non-maskable Interrupt",
        3 => "Breakpoint",
        4 => "Overflow",
        5 => "Bound Range Exceeded",
        6 => "Invalid Opcode",
        7 => "Device not Available",
        8 => "Double Fault",
        9 => "Coprocessor Segment Overrun",
        10 => "Invalid TSS",
        11 => "Segment not present",
        12 => "Stack-segment Fault",
        13 => "General Protection Fault",
        14 => "Page Fault",
        16 => "x87 exception",
        17 => "Alignment Check",
        18 => "Machine Check",
        19 => "SIMD exception",
        20 => "Virtualization exception",
        21 => "Control protection exception",
        28 => "Hypervisor injection exception",
        29 => "VMM Communication exception",
        30 => "Security exception",
        15, 22...27, 31 => "Reserved",
        else => @panic("unknown fault"),
    };
}

export fn handleFault(ctx: *RawContext, num: u8, errcode: u64) callconv(.C) void {
    std.log.err("\nFAULT: {s} ({})\n{}", .{ numToFault(num), errcode, ctx });
    @panic("exception");
}

export fn handleIrq(ctx: *RawContext, num: u8) callconv(.C) void {
    const ipl = yak.raiseIpl(yak.Ipl.fromIrq(num));
    defer yak.lowerIpl(ipl);

    const vec = num - 32;

    _ = ctx;
    std.log.debug("got irq {}", .{vec});
}
