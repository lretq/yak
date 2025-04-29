const std = @import("std");
const DoublyLinkedList = std.DoublyLinkedList;

const yak = @import("../main.zig");

pub const Pfn = yak.arch.Pfn;

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
};
