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

pub fn initIdt() void {
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

pub fn loadIdt() void {
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
};

export fn handleFault(ctx: *RawContext, num: u8, errcode: u64) callconv(.C) void {
    std.log.debug("\ngot fault\n{}\n num:{} error:{}", .{ ctx, num, errcode });
    @panic("exception");
}

export fn handleIrq(ctx: *RawContext, num: u8) callconv(.C) void {
    const vec = num - 32;

    _ = ctx;
    std.log.debug("got irq {}", .{vec});
}
