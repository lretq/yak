const builtin = @import("builtin");
const std = @import("std");
const limine = @import("generic/limine.zig");

const yak = @import("../main.zig");

pub const gdt = @import("x86_64/gdt.zig");
pub const idt = @import("x86_64/idt.zig");

pub const PAGE_SIZE = 4096;
pub const PAGE_SHIFT = 12;

pub const IRQ_SLOTS = 256;
pub const IRQ_SLOTS_PER_IPL = 16;

pub const KERNEL_STACK_SIZE = 4; // = 16KiB

pub var HHDM_BASE: u64 = undefined;
pub const PFNDB_BASE = 0xFFFFFFFa00000000;
pub const KERNEL_ARENA_BASE = 0xFFFFFFFb00000000;

pub const BUDDY_ORDERS = 19;

pub const Pfn = u52;

pub const Ipl = enum(u4) {
    Passive = 0,
    Apc = 1,
    Dispatch = 2,
    Device0 = 3,
    Device1 = 4,
    Device2 = 5,
    Device3 = 6,
    Device4 = 7,
    Device5 = 8,
    Device6 = 9,
    Device7 = 10,
    Device8 = 11,
    Device9 = 12,
    Clock = 13,
    Ipi = 14,
    High = 15,

    pub fn toIrq(self: @This()) u8 {
        return @intFromEnum(self) << 4;
    }

    pub fn fromIrq(irq: u8) @This() {
        return @enumFromInt(irq >> 4);
    }
};

pub inline fn getIpl() Ipl {
    return asm ("mov %cr8, %[ret]"
        : [ret] "=r" (-> Ipl),
    );
}

pub inline fn setIpl(ipl: Ipl) void {
    asm volatile ("mov %[ipl_ref], %cr8"
        :
        : [ipl_ref] "r" (ipl),
    );
}

pub fn hcf() noreturn {
    while (true) {
        asm volatile ("hlt");
    }
    @trap();
}

pub fn wait_int() void {
    asm volatile ("hlt");
}

pub fn int_status() bool {
    const rflags =
        asm volatile (
            \\pushfq
            \\pop %[ret]
            : [ret] "=r" (-> u64),
            :
            : "memory"
        );
    return (rflags & (1 << 9)) != 0;
}

pub fn set_int(state: bool) bool {
    const old = int_status();
    if (state) {
        asm volatile ("sti");
    } else {
        asm volatile ("cli");
    }
    return old;
}

const NS16550 = yak.io.serial.ns16550.NS16550(PortIo);

const COM1 = 0x3F8;
var serial = NS16550.init(.{
    .base = COM1,
});

var com1_console: yak.io.console.Console = .{
    .name = "serial",
    .writeFn = NS16550.write,
    .user_ctx = @ptrCast(&serial),
};

pub const debug_console = &com1_console;

const zflanterm = @import("zflanterm");

var fb_console: yak.io.console.Console = .{
    .name = "fb",
    .writeFn = fbWrite,
};

fn fbWrite(ctx: ?*anyopaque, data: []const u8) !usize {
    const self: *zflanterm.FlantermContext = @ptrCast(@alignCast(ctx orelse unreachable));
    return try self.writer().write(data);
}

// ist are setup as in idt.zig
pub var kernel_tss: gdt.Tss = .{};

pub fn init() !void {
    limine.healthcheck();

    if (limine.hhdm_request.response) |hhdm_response| {
        HHDM_BASE = hhdm_response.offset;
    }

    serial.configure();
    yak.io.console.register(&com1_console);

    gdt.init();
    gdt.loadGdt();

    idt.init();
    idt.load();

    if (limine.framebuffer_request.response) |framebuffer_response| {
        const fb = framebuffer_response.getFramebuffers()[0];
        const ctx = zflanterm.initFramebufferContext(@ptrCast(@alignCast(fb.address)), fb.width, fb.height, fb.pitch, fb.red_mask_size, fb.red_mask_shift, fb.green_mask_size, fb.green_mask_shift, fb.blue_mask_size, fb.blue_mask_shift, null, null, null, null, null, null, null, null, 0, 0, 1, 0, 0, 0) orelse return;
        fb_console.user_ctx = ctx;
        yak.io.console.register(&fb_console);
    }

    if (limine.memory_map_request.response) |map_res| {
        for (map_res.getEntries()) |ent| {
            if (ent.type != .usable) continue;
            yak.pm.registerRegion(ent.base, ent.base + ent.length);
        }
    }

    const allocator = yak.pm.page_allocator;
    var page = try allocator.create([PAGE_SIZE]u8);
    kernel_tss.ist1 = @intFromPtr(page);
    page = try allocator.create([PAGE_SIZE]u8);
    kernel_tss.ist2 = @intFromPtr(page);
    page = try allocator.create([PAGE_SIZE]u8);
    kernel_tss.ist3 = @intFromPtr(page);

    gdt.loadTss(&kernel_tss);
}

pub inline fn port_out(comptime T: type, port: u16, value: T) void {
    switch (T) {
        u8 => {
            asm volatile ("outb %al, %dx"
                :
                : [value_reference] "{al}" (value),
                  [port_reference] "{dx}" (port),
            );
        },
        u16 => {
            asm volatile ("outw %ax, %dx"
                :
                : [value_reference] "{ax}" (value),
                  [port_reference] "{dx}" (port),
            );
        },
        u32 => {
            asm volatile ("outl %eax, %dx"
                :
                : [value_reference] "{eax}" (value),
                  [port_reference] "{dx}" (port),
            );
        },
        else => @compileError("Type must be of u8, u16 or u32"),
    }
}

pub inline fn port_in(comptime T: type, port: u16) T {
    switch (T) {
        u8 => {
            return asm volatile ("inb %dx, %[value]"
                : [value] "={al}" (-> T),
                : [port_reference] "{dx}" (port),
            );
        },
        u16 => {
            return asm volatile ("inw %dx, %[value]"
                : [value] "={ax}" (-> T),
                : [port_reference] "{dx}" (port),
            );
        },
        u32 => {
            return asm volatile ("inl %dx, %[value]"
                : [value] "={eax}" (-> T),
                : [port_reference] "{dx}" (port),
            );
        },
        else => @compileError("Type must be of u8, u16 or u32"),
    }
}

pub const PortIo = struct {
    const Self = @This();

    base: u16,

    pub inline fn read(self: Self, comptime T: type, offset: u16) T {
        return port_in(T, self.base + offset);
    }

    pub inline fn write(self: Self, comptime T: type, offset: u16, value: T) void {
        port_out(T, self.base + offset, value);
    }
};
