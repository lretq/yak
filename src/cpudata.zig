const yak = @import("main.zig");
const arch = yak.arch;

pub const CpuData = struct {
    selfptr: *CpuData,

    pub fn init(self: *@This()) void {
        self.selfptr = self;
    }
};
