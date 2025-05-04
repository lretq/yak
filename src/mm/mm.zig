const std = @import("std");
const DoublyLinkedList = std.DoublyLinkedList;

const yak = @import("../main.zig");

pub const Pfn = yak.arch.Pfn;

pub const Pmap = @import("pmap.zig").Pmap;

pub const Page = struct {
    pfn: Pfn,
    node: DoublyLinkedList.Node,
    max_order: usize,
    shares: usize,

    pub fn init(self: *Page, pfn: Pfn) void {
        self.pfn = pfn;
        self.node = .{};
        self.max_order = 0;
        self.shares = 0;
    }

    pub fn toAddress(self: *Page) usize {
        return self.pfn << yak.arch.PAGE_SHIFT;
    }

    pub fn toMappedAddress(self: *Page) usize {
        return self.toAddress() + yak.arch.HHDM_BASE;
    }

    pub fn zero(self: *Page) void {
        const m: *[yak.arch.PAGE_SIZE]u8 = @ptrFromInt(self.toMappedAddress());
        @memset(m, 0);
    }
};

pub const MapFlags = packed struct {
    read: bool = false,
    write: bool = false,
    execute: bool = false,
    user: bool = false,
    global: bool = false,
    pagesize: u8 = 12,
};

pub const CacheMode = enum(u8) {
    Uncached = 0,
    WriteCombine,
    WriteThrough,
    WriteBack,
};
