const std = @import("std");

pub fn setup_exe(b: *std.Build, exe: *std.BuildExecutable) void {
    _ = .{ b, exe };
}

pub fn setup_module(b: *std.Build, mod: *std.Module) void {
    const limine_zig = b.dependency("limine_zig", .{ .api_revision = 3 });
    const limine_module = limine_zig.module("limine");
    mod.addImport("limine", limine_module);
}
