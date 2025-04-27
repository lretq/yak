const std = @import("std");
const DoublyLinkedList = std.DoublyLinkedList;

const yak = @import("main.zig");
const mm = yak.mm;
const Page = mm.Page;

const arch = yak.arch;

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

fn lookupRegion(addr: usize) ?*Region {
    var it = region_list.first;
    while (it) |node| : (it = node.next) {
        const l: *Region = @fieldParentPtr("node", node);
        if (addr >= l.start_addr and addr <= l.end_addr) return l;
    }
    return null;
}

pub fn lookupPage(addr: usize) ?*Page {
    const region = lookupRegion(addr) orelse return null;
    const base_pfn = (region.start_addr >> arch.PAGE_SHIFT);
    return &region.pages[(addr >> arch.PAGE_SHIFT) - base_pfn];
}

fn alloc(_: *anyopaque, len: usize, _: std.mem.Alignment, _: usize) ?[*]u8 {
    if (len == 0) @panic("dont know if normal");
    if (len > arch.PAGE_SIZE) return null;

    const page = allocPage() catch return null;
    const addr = page.toAddress();

    return @as([*]u8, @ptrFromInt(arch.HHDM_BASE + addr));
}

fn resize(_: *anyopaque, _: []u8, _: std.mem.Alignment, _: usize, _: usize) bool {
    return false;
}

fn remap(_: *anyopaque, _: []u8, _: std.mem.Alignment, _: usize, _: usize) ?[*]u8 {
    return null;
}

fn free(_: *anyopaque, memory: []u8, _: std.mem.Alignment, _: usize) void {
    const addr = @intFromPtr(memory.ptr) - arch.HHDM_BASE;
    if (lookupPage(addr)) |page| {
        freePage(page);
    }
}

const page_allocator_vtable: std.mem.Allocator.VTable = .{
    .alloc = alloc,
    .free = free,
    .resize = resize,
    .remap = remap,
};

pub const page_allocator: std.mem.Allocator = .{
    .ptr = undefined,
    .vtable = &page_allocator_vtable,
};
