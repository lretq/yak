const std = @import("std");
const Console = @import("../console.zig").Console;

pub fn NS16550(comptime IO: type) type {
    return struct {
        const Self = @This();

        io: IO = undefined,
        console: Console = undefined,

        pub fn getConsole(self: *Self) *Console {
            self.console.user_ctx = @ptrCast(self);
            return &self.console;
        }

        pub fn init(comptime name: []const u8, io: IO) Self {
            return .{
                .io = io,
                .console = .{
                    .name = name,
                    .writeFn = Self.write,
                    .user_ctx = null,
                },
            };
        }

        fn out(self: *Self, reg: Register, value: u8) void {
            self.io.write(u8, @intFromEnum(reg), value);
        }

        fn in(self: *Self, reg: Register) u8 {
            return self.io.read(u8, @intFromEnum(reg));
        }

        pub fn isTransmitEmpty(self: *Self) bool {
            return (self.in(.lsr) & 0x20) != 0;
        }

        pub fn putc(self: *Self, c: u8) void {
            while (!self.isTransmitEmpty()) {}
            self.out(.rbr_thr, c);
        }

        pub fn putString(self: *Self, str: []const u8) void {
            for (str) |c| {
                if (c == '\n') self.putc('\r');
                self.putc(c);
            }
        }

        pub fn write(ctx: ?*anyopaque, data: []const u8) !usize {
            const self: *Self = @ptrCast(@alignCast(ctx orelse unreachable));
            self.putString(data);
            return data.len;
        }

        const Writer = std.io.Writer(*anyopaque, error{}, write);
        pub fn writer(self: *Self) Writer {
            return .{ .context = self };
        }

        pub fn configure(self: *Self) bool {
            // Disable all interrupts
            self.out(.ier, 0x0);
            // Enable DLAB (set baud rate divisor)
            self.out(.lcr, 0x80);
            // Set divisor to 3 (lo byte) 38400 baud
            self.out(.rbr_thr, 0x03);
            // (hi byte)
            self.out(.ier, 0x00);
            // 8N1
            self.out(.lcr, 0x03);
            // Enable FIFO, clear them, with 14-byte threshold
            self.out(.iir_fcr, 0xC7);
            // IRQs enabled, RTS/DSR set
            self.out(.mcr, 0x0B);
            // Set in loopback mode, test the serial chip
            self.out(.mcr, 0x1E);

            self.out(.rbr_thr, 0xAE);
            if (self.in(.rbr_thr) != 0xAE) {
                return false;
            }

            // If serial is not faulty set it in normal operation mode
            // (not-loopback with IRQs enabled and OUT#1 and OUT#2 bits enabled)
            self.out(.mcr, 0x0F);

            return true;
        }
    };
}

pub const Register = enum(u3) {
    // Receive Buffer Register / Transmit Holding Register
    rbr_thr = 0,
    // Interrupt Enable
    ier = 1,
    // Interrupt identification register / FIFO Control
    iir_fcr = 2,
    // Line control Register
    lcr = 3,
    // Modem control Register
    mcr = 4,
    // Line status register
    lsr = 5,
    // Modem status register
    msr = 6,
    // Scratch register
    scr = 7,
};
