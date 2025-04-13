const builtin = @import("builtin");
const std = @import("std");
const limine = @import("generic/limine.zig");

const yak = @import("../main.zig");

pub const PAGE_SIZE = 4096;
pub const PAGE_SHIFT = 12;

pub const IRQ_SLOTS = 256 - 32;
pub const IRQ_SLOTS_PER_IPL = 16;

pub const KERNEL_STACK_SIZE = 4; // = 16KiB

pub var HHDM_BASE: u64 = undefined;
pub const PFNDB_BASE = 0xFFFFFFFa00000000;
pub const KERNEL_ARENA_BASE = 0xFFFFFFFb00000000;

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
    if (comptime state) {
        asm volatile ("sti");
    } else {
        asm volatile ("cli");
    }
    return old;
}

const NS16550 = yak.io.serial.ns16550.NS16550(PortIo);

const zflanterm = @import("zflanterm");

pub fn init() void {
    limine.healthcheck();

    if (limine.hhdm_request.response) |hhdm_response| {
        HHDM_BASE = hhdm_response.offset;
    }

    const COM1 = 0x3F8;
    const serial = NS16550.init(.{
        .base = COM1,
    });
    serial.configure();
    serial.write("Hello World!\n");

    if (limine.framebuffer_request.response) |framebuffer_response| {
        const fb = framebuffer_response.getFramebuffers()[0];
        const ctx = zflanterm.fb_init(@ptrCast(@alignCast(fb.address)), fb.width, fb.height, fb.pitch, fb.red_mask_size, fb.red_mask_shift, fb.green_mask_size, fb.green_mask_shift, fb.blue_mask_size, fb.blue_mask_shift, null, null, null, null, null, null, null, null, 0, 0, 1, 0, 0, 0) orelse @panic("flanterm");

        const dims = ctx.get_dimensions();
        var buf: [512:0]u8 = .{0} ** 512;
        const res = std.fmt.bufPrint(&buf, "dimensions: {}\n", .{dims}) catch @panic("stfu");

        ctx.write(res);

        ctx.write("Hello World\n");
    } else {
        @panic("Framebuffer response not present");
    }
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
