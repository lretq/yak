pub const Arch = @import("arch.zig").Arch;

export fn kentry() noreturn {
    Arch.init();
    Arch.hcf();
}
