const std = @import("std");
const yak = @import("kernel");
const ipl = yak.ipl;
const Ipl = ipl.Ipl;

pub const Spinlock = struct {
    locked: std.atomic.Value(bool),

    pub fn init() Spinlock {
        return .{ .locked = .init(false) };
    }

    pub fn lock_elevated(self: *Spinlock) void {
        while (self.locked.cmpxchgWeak(false, true, .acquire, .monotonic) != null) {
            yak.hint.spinLoop();
        }
    }

    pub fn lock(self: *Spinlock) Ipl {
        const old = ipl.raiseIpl(.Dispatch);
        self.lock_elevated();
        return old;
    }

    pub fn unlock_elevated(self: *Spinlock) void {
        self.locked.store(false, .release);
    }

    pub fn unlock(self: *Spinlock, old: Ipl) void {
        self.unlock_elevated();
        ipl.lowerIpl(old);
    }
};
