const limine = @import("limine");
const yak = @import("../../main.zig");
const std = @import("std");

const arch = yak.arch;

export var start_marker: limine.RequestsStartMarker linksection(".limine_requests_start") = .{};
export var end_marker: limine.RequestsEndMarker linksection(".limine_requests_end") = .{};

pub export var base_revision: limine.BaseRevision linksection(".limine_requests") = .init(3);

pub export var framebuffer_request: limine.FramebufferRequest linksection(".limine_requests") = .{};

pub export var memory_map_request: limine.MemoryMapRequest linksection(".limine_requests") = .{};

pub export var hhdm_request: limine.HhdmRequest linksection(".limine_requests") = .{};

pub export var stack_request: limine.StackSizeRequest linksection(".limine_requests") = .{
    .stack_size = yak.arch.PAGE_SIZE * yak.arch.KERNEL_STACK_SIZE,
};

pub export var address_request: limine.ExecutableAddressRequest linksection(".limine_requests") = .{};

pub export var file_request: limine.ExecutableFileRequest linksection(".limine_requests") = .{};

pub export var paging_mode_request: limine.PagingModeRequest linksection(".limine_requests") = .{};

pub fn healthcheck() void {
    if (!base_revision.isSupported()) {
        @panic("Base revision not supported");
    }
}

const zflanterm = @import("zflanterm");

pub var fb_console: yak.io.console.Console = .{
    .name = "limine-fb",
    .writeFn = fbWrite,
};

fn fbWrite(ctx: ?*anyopaque, data: []const u8) !usize {
    const self: *zflanterm.FlantermContext = @ptrCast(@alignCast(ctx orelse unreachable));
    return try self.writer().write(data);
}

pub fn early_setup() void {
    // get fb output early
    if (framebuffer_request.response) |framebuffer_response| {
        const fb = framebuffer_response.getFramebuffers()[0];
        const ctx = zflanterm.initFramebufferContext(@ptrCast(@alignCast(fb.address)), fb.width, fb.height, fb.pitch, fb.red_mask_size, fb.red_mask_shift, fb.green_mask_size, fb.green_mask_shift, fb.blue_mask_size, fb.blue_mask_shift, null, null, null, null, null, null, null, null, 0, 0, 1, 0, 0, 0) orelse return;
        fb_console.user_ctx = ctx;
        yak.io.console.register(&fb_console);
    }

    arch.HHDM_BASE = hhdm_request.response.?.offset;
}

pub fn initMemory() !void {
    const map_res = memory_map_request.response.?;

    yak.pm.init();

    for (map_res.getEntries()) |ent| {
        if (ent.type != .usable) continue;
        yak.pm.registerRegion(ent.base, ent.base + ent.length);
    }

    try yak.mm.kernel_map.init();

    for (map_res.getEntries()) |ent| {
        const cache: yak.mm.CacheMode = switch (ent.type) {
            .framebuffer => .WriteCombine,
            // rely on correct MTRRs
            else => .WriteBack,
        };

        const base = std.mem.alignBackward(usize, ent.base, arch.PAGE_SIZE);
        const length = std.mem.alignForward(usize, ent.length, arch.PAGE_SIZE);

        try yak.mm.kernel_map.pmap.map_large_range(base, length, arch.HHDM_BASE, .{ .read = true, .write = true }, cache);
    }
}

const __kernel_text_start: [*c]u8 = @extern([*c]u8, .{
    .name = "__kernel_text_start",
});
const __kernel_text_end: [*c]u8 = @extern([*c]u8, .{
    .name = "__kernel_text_end",
});
const __kernel_rodata_start: [*c]u8 = @extern([*c]u8, .{
    .name = "__kernel_rodata_start",
});
const __kernel_rodata_end: [*c]u8 = @extern([*c]u8, .{
    .name = "__kernel_rodata_end",
});
const __kernel_data_start: [*c]u8 = @extern([*c]u8, .{
    .name = "__kernel_data_start",
});
const __kernel_data_end: [*c]u8 = @extern([*c]u8, .{
    .name = "__kernel_data_end",
});

pub fn mapKernel() !void {
    std.log.info("mapping kernel", .{});
    const addr_res = address_request.response.?;

    const pa_base = addr_res.physical_base;
    const va_base = addr_res.virtual_base;

    try mapSection(va_base, pa_base, @intFromPtr(__kernel_text_start), @intFromPtr(__kernel_text_end), .{
        .read = true,
        .execute = true,
        .global = true,
    });

    try mapSection(va_base, pa_base, @intFromPtr(__kernel_rodata_start), @intFromPtr(__kernel_rodata_end), .{
        .read = true,
        .global = true,
    });

    try mapSection(va_base, pa_base, @intFromPtr(__kernel_data_start), @intFromPtr(__kernel_data_end), .{
        .read = true,
        .write = true,
        .global = true,
    });
}

fn mapSection(vbase: usize, pbase: usize, start: u64, end: u64, prot: yak.mm.MapFlags) !void {
    const aligned_start = std.mem.alignBackward(usize, start, arch.PAGE_SIZE);
    const aligned_end = std.mem.alignForward(usize, end, arch.PAGE_SIZE);

    var i = aligned_start;
    while (i < aligned_end) : (i += arch.PAGE_SIZE) {
        try yak.mm.kernel_map.pmap.enter(i, i - vbase + pbase, prot, .WriteBack, .{});
    }
}
