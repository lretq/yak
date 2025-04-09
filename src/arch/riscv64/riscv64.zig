pub const Arch = struct {
    pub fn hcf() noreturn {
        while (true) {
            asm volatile ("wfi");
        }
    }
};
