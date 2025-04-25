const std = @import("std");
const DoublyLinkedList = std.DoublyLinkedList;

const yak = @import("../main.zig");

pub const Page = struct {
    pfn: yak.arch.Pfn,
    node: DoublyLinkedList.Node,

    pub fn init(self: *Page, pfn: yak.arch.Pfn) void {
        self.pfn = pfn;
        self.node = .{};
    }

    pub fn toAddress(self: *Page) usize {
        return self.pfn << yak.arch.PAGE_SHIFT;
    }
};
