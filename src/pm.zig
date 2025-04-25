const std = @import("std");
const DoublyLinkedList = std.DoublyLinkedList;

const yak = @import("main.zig");
const mm = yak.mm;
const arch = yak.arch;

const bit = yak.bithacks;

const Page = mm.Page;

const Region = struct {
    start_addr: usize,
    end_addr: usize,
    pages: [*]Page,
    node: DoublyLinkedList.Node = .{},
};

var region_list: DoublyLinkedList = .{};
var free_list: DoublyLinkedList = .{};

var pfndb: [*]Page = @ptrFromInt(yak.arch.PFNDB_BASE);

const logger = std.log.scoped(.pm);

pub fn access(comptime T: type, addr: usize) *T {
    return @ptrFromInt(arch.HHDM_BASE + addr);
}

pub fn registerRegion(start_addr: usize, end_addr: usize) void {
    const page_count = (end_addr - start_addr) / arch.PAGE_SIZE;
    const length = end_addr - start_addr;
    const used = @sizeOf(Region) + @sizeOf(Page) * page_count;
    const used_rounded =
        std.mem.alignForward(usize, used, arch.PAGE_SIZE);

    if (length - used_rounded == 0) {
        logger.debug("ignoring region {X}-{X}", .{ start_addr, end_addr });
        return;
    }

    var region = access(Region, start_addr);
    region.start_addr = start_addr;
    region.end_addr = end_addr;
    region.pages = @ptrFromInt(@intFromPtr(region) + @sizeOf(Region));
    region.node = .{};
    region_list.append(&region.node);

    const base_pfn = (start_addr >> arch.PAGE_SHIFT);
    var i: usize = start_addr + used_rounded;
    while (i < end_addr) : (i += arch.PAGE_SIZE) {
        const pfn = (i >> arch.PAGE_SHIFT);
        const page = &region.pages[pfn - base_pfn];
        page.init(@truncate(pfn));
        free_list.append(&page.node);
    }

    logger.debug("added region {X}-{X}", .{ start_addr, end_addr });
}

const AllocationError = error{OutOfMemory};

pub fn allocPage() AllocationError!*Page {
    if (free_list.pop()) |node| {
        return @fieldParentPtr("node", node);
    }
    return AllocationError.OutOfMemory;
}

pub fn freePage(page: *Page) void {
    free_list.append(&page.node);
}

pub fn lookupPage(addr: usize) ?*Page {
    var it = region_list.first;
    while (it) |node| : (it = node.next) {
        const l: *Region = @fieldParentPtr("node", node);
        if (addr >= l.start_addr and addr <= l.end_addr) return l;
    }
}
