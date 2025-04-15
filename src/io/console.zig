const std = @import("std");
const yak = @import("kernel");

var console_list: std.DoublyLinkedList = .{};
var console_list_lock: yak.sync.Spinlock = .init();

pub const Console = struct {
    name: []const u8,
    writeFn: ?*const fn (ctx: ?*anyopaque, data: []const u8) anyerror!usize,
    user_ctx: ?*anyopaque = null,
    node: std.DoublyLinkedList.Node = undefined,
};

const ConsoleWriter = std.io.Writer(void, anyerror, consolesWrite);
var consoleWriter: ConsoleWriter = .{ .context = undefined };
fn consolesWrite(self: void, data: []const u8) !usize {
    _ = self;
    const ipl = console_list_lock.lock();
    defer console_list_lock.unlock(ipl);
    var it = console_list.first;
    while (it) |node| : (it = node.next) {
        const console: *Console = @fieldParentPtr("node", node);
        if (console.writeFn) |writeFn| {
            _ = try writeFn(console.user_ctx, data);
        }
    }
    return data.len;
}

pub fn getConsoleWriter() *ConsoleWriter {
    return &consoleWriter;
}

pub fn register(console: *Console) void {
    const ipl = console_list_lock.lock();
    defer console_list_lock.unlock(ipl);
    console_list.append(&console.node);
}
