const Range = struct {
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

const Units = struct {
    pub const KiB = 1024;
    pub const MiB = KiB * 1024;
    pub const GiB = MiB * 1024;
};
