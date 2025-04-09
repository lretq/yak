const std = @import("std");

const Arch = enum {
    x86_64,
    riscv64,

    fn toStd(self: @This()) std.Target.Cpu.Arch {
        return switch (self) {
            .x86_64 => .x86_64,
            .riscv64 => .riscv64,
        };
    }
};

fn targetQueryForArch(arch: Arch) std.Target.Query {
    var query: std.Target.Query = .{
        .cpu_arch = arch.toStd(),
        .os_tag = .freestanding,
        .abi = .none,
    };

    switch (arch) {
        .x86_64 => {
            const Target = std.Target.x86;

            query.cpu_features_add = Target.featureSet(&.{ .popcnt, .soft_float });
            query.cpu_features_sub = Target.featureSet(&.{ .avx, .avx2, .sse, .sse2, .mmx });
        },
        .riscv64 => {
            const Target = std.Target.riscv;

            query.cpu_features_add = Target.featureSet(&.{});
            query.cpu_features_sub = Target.featureSet(&.{.d});
        },
    }

    return query;
}

pub fn build(b: *std.Build) void {
    const arch = b.option(Arch, "arch", "Architecture to build the kernel for") orelse .x86_64;

    const query = targetQueryForArch(arch);

    const target = b.resolveTargetQuery(query);
    const optimize = b.standardOptimizeOption(.{});

    const kernel_module = b.createModule(.{
        .root_source_file = b.path("src/main.zig"),
        .target = target,
        .optimize = optimize,
    });

    switch (arch) {
        .x86_64 => {
            kernel_module.red_zone = false;
            kernel_module.code_model = .kernel;
        },
        .riscv64 => {},
    }

    switch (arch) {
        .x86_64, .riscv64 => {
            const limine_zig = b.dependency("limine_zig", .{ .api_revision = 3 });
            const limine_module = limine_zig.module("limine");
            kernel_module.addImport("limine", limine_module);
        },
    }

    const kernel = b.addExecutable(.{
        .name = "kernel",
        .root_module = kernel_module,
    });

    // TODO: cleanup
    switch (arch) {
        .x86_64 => {
            kernel.addAssemblyFile(b.path("src/arch/x86_64/start.S"));
        },
        else => {},
    }

    kernel.setLinkerScript(b.path(b.fmt("src/arch/{s}/linker.lds", .{@tagName(arch)})));

    b.resolveInstallPrefix(null, .{ .exe_dir = b.fmt("bin-{s}", .{@tagName(arch)}) });
    b.installArtifact(kernel);

    const iso_cmd = b.addSystemCommand(&.{ "./tools/mkiso.sh", @tagName(arch) });
    iso_cmd.step.dependOn(b.getInstallStep());

    const qemu_kernel = b.addSystemCommand(&.{ "./tools/qemu.sh", @tagName(arch) });
    const qemu_step = b.step("run", "Run the kernel in qemu");

    qemu_kernel.step.dependOn(&iso_cmd.step);
    qemu_step.dependOn(&qemu_kernel.step);
}
