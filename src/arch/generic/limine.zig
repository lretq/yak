const limine = @import("limine");
const Arch = @import("../../arch.zig").Arch;

export var start_marker: limine.RequestsStartMarker linksection(".limine_requests_start") = .{};
export var end_marker: limine.RequestsEndMarker linksection(".limine_requests_end") = .{};

pub export var base_revision: limine.BaseRevision linksection(".limine_requests") = .init(3);

pub export var framebuffer_request: limine.FramebufferRequest linksection(".limine_requests") = .{};
pub export var memory_map_request: limine.MemoryMapRequest linksection(".limine_requests") = .{};

pub export var stack_request: limine.StackSizeRequest linksection(".limine_requests") = .{
    .stack_size = Arch.PAGE_SIZE * Arch.KERNEL_STACK_SIZE,
};

pub fn healthcheck() void {
    if (!base_revision.isSupported()) {
        @panic("Base revision not supported");
    }
}
