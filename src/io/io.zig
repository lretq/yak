pub const serial = struct {
    pub const ns16550 = @import("serial/NS16550.zig");
};

pub const console = @import("console.zig");
