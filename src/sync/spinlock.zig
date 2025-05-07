const std = @import("std");
const yak = @import("../main.zig");
const Ipl = yak.Ipl;

pub const Spinlock = struct {
    locked: std.atomic.Value(bool),

    pub fn init() Spinlock {
        return .{ .locked = .init(false) };
    }

    pub fn lock_elevated(self: *Spinlock) void {
        while (self.locked.cmpxchgWeak(false, true, .acquire, .monotonic) != null) {
            std.atomic.spinLoopHint();
        }
    }

    pub fn lockInts(self: *Spinlock) bool {
        const old = yak.arch.set_int(false);
        self.lock_elevated();
        return old;
    }

    pub fn lock(self: *Spinlock) Ipl {
        const old = yak.raiseIpl(.Dispatch);
        self.lock_elevated();
        return old;
    }

    pub fn unlock_elevated(self: *Spinlock) void {
        self.locked.store(false, .release);
    }

    pub fn unlockInts(self: *Spinlock, old: bool) void {
        self.unlock_elevated();
        _ = yak.arch.set_int(old);
    }

    pub fn unlock(self: *Spinlock, old: Ipl) void {
        self.unlock_elevated();
        yak.lowerIpl(old);
    }
};
