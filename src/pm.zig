const std = @import("std");
const DoublyLinkedList = std.DoublyLinkedList;

const yak = @import("main.zig");
const mm = yak.mm;
const Page = mm.Page;

const arch = yak.arch;

const logger = std.log.scoped(.pm);

const Region = struct {
    start_addr: usize,
    end_addr: usize,
    pages: [*]Page,
    node: DoublyLinkedList.Node = .{},
};

var region_list: DoublyLinkedList = .{};

const BuddyList = DoublyLinkedList;

// order 0 == min. page size
// so on x86 with 18 orders it would actually start at
// 2^12 and go to 2^30
var order_list: [arch.BUDDY_ORDERS]BuddyList = undefined;
var npages = std.mem.zeroes([arch.BUDDY_ORDERS]usize);

//var pfndb: [*]Page = @ptrFromInt(arch.PFNDB_BASE);

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

    var i: usize = start_addr;
    while (i < start_addr + used_rounded) : (i += arch.PAGE_SIZE) {
        const pfn = (i >> arch.PAGE_SHIFT);
        const page = &region.pages[pfn - base_pfn];
        page.init(@truncate(pfn));
        // mark as 'non-free'
        page.shares = 1;
    }

    i = start_addr + used_rounded;
    while (i < end_addr) {
        var max_order: u6 = 0;
        while ((i + (@as(u64, 2) << (arch.PAGE_SHIFT + max_order))) < end_addr and std.mem.isAligned(i, @as(u64, 2) << (arch.PAGE_SHIFT + max_order)) and max_order < arch.BUDDY_ORDERS - 1) : (max_order += 1) {}

        const block_size = @as(u64, 1) << (arch.PAGE_SHIFT + max_order);

        var n = i;
        while (n < i + block_size) : (n += arch.PAGE_SIZE) {
            const pfn = (n >> arch.PAGE_SHIFT);
            const page = &region.pages[pfn - base_pfn];
            page.init(@truncate(pfn));
            page.max_order = max_order;
            page.shares = 0;
        }

        const pfn = (i >> arch.PAGE_SHIFT);
        const page = &region.pages[pfn - base_pfn];

        npages[max_order] += 1;
        order_list[max_order].append(&page.node);

        i += block_size;
    }

    logger.debug("added region {X}-{X}", .{ start_addr, end_addr });
}

const AllocationError = error{
    BadBuddyOrder,
    OutOfMemory,
};

pub fn allocPages(order: usize) AllocationError!*Page {
    if (order >= arch.BUDDY_ORDERS) {
        return AllocationError.BadBuddyOrder;
    }

    if (npages[order] > 0) {
        const node = order_list[order].pop().?;
        npages[order] -= 1;
        const page: *Page = @fieldParentPtr("node", node);
        page.shares = 1;
        return page;
    }

    var next_order: usize = order + 1;
    while (next_order < arch.BUDDY_ORDERS and npages[next_order] == 0) {
        next_order += 1;
    }

    if (next_order >= arch.BUDDY_ORDERS) {
        return AllocationError.OutOfMemory;
    }

    // split until at needed order
    var i: usize = next_order;
    var buddy_page: *Page = @fieldParentPtr("node", order_list[i].pop().?);
    npages[i] -= 1;

    while (i > order) {
        i -= 1;

        order_list[i].append(&buddy_page.node);
        npages[i] += 1;

        const block_size = @as(u64, 1) << @intCast(arch.PAGE_SHIFT + i);

        const buddy_addr = buddy_page.toAddress() + block_size;
        buddy_page = lookupPage(buddy_addr).?;
    }

    std.debug.assert(buddy_page.shares == 0);
    buddy_page.shares = 1;

    return buddy_page;
}

pub fn freePages(page: *Page, order: usize) void {
    std.debug.assert(page.shares == 1);

    var curr_order = order;
    var curr_page = page;

    while (curr_order < page.max_order) {
        const block_size = @as(u64, 1) << @intCast(arch.PAGE_SHIFT + curr_order);

        const addr = curr_page.toAddress();
        const buddy_addr = addr ^ block_size;
        const buddy_page = lookupPage(buddy_addr) orelse @panic("buddy out of bounds");
        if (buddy_page.shares != 0 or buddy_page.max_order != curr_page.max_order) break;

        order_list[curr_order].remove(&buddy_page.node);
        std.debug.assert(npages[curr_order] > 0);
        npages[curr_order] -= 1;

        curr_page.shares = 0;

        curr_order += 1;
        curr_page = if (buddy_addr < addr) buddy_page else curr_page;
    }

    curr_page.shares = 0;
    order_list[curr_order].append(&curr_page.node);
    npages[curr_order] += 1;
}

pub fn dump() void {
    std.log.debug("{any}", .{npages});
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

fn getOrder(len: usize) usize {
    return std.math.log2_int_ceil(usize, len) -| arch.PAGE_SHIFT;
}

fn alloc(_: *anyopaque, len: usize, _: std.mem.Alignment, _: usize) ?[*]u8 {
    if (len == 0) @panic("dont know if normal");

    const page = allocPages(getOrder(len)) catch return null;
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
        freePages(page, getOrder(memory.len));
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
