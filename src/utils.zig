pub const Range = struct {
    start: usize,
    end: usize,

    pub fn init_with_size(start: usize, size: usize) Range {
        return .{ start, start + size };
    }

    pub fn contains(self: Range, value: usize) bool {
        return value >= self.start and value < self.end;
    }

    pub fn len(self: Range) usize {
        return self.end - self.start;
    }
};

pub const Units = enum(usize) {
    KiB = 1024,
    MiB = 1024 * 1024,
    GiB = 1024 * 1024 * 1024,
    TiB = 1024 * 1024 * 1024 * 1024,

    pub fn int(comptime self: @This()) comptime_int {
        return @enumFromInt(self);
    }
};

pub fn MiB(n: usize) usize {
    return n * (2 << 19);
}

pub fn GiB(n: usize) usize {
    return n * (2 << 29);
}
