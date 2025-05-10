# Yak (Yet another kernel)

After realizing some shortcomings of Zig as a language, atleast for kernel development, I have decided to scrap this version and use C/C++ again, sadly :(

Features missing/not wanted in Zig:
* actual interface-like structures (i.e. something like rust traits) have been denied, manual vtable (see e.g. `std.mem.Allocator`) deemed as the way to go. Issue was closed ages ago, zig creator said its not needed
* dynamic linkage: While e.g. comptime is great and fun to use, I cannot dynamically link to other zig code/modules. Having API wrappers around everything (essentially doubling all kernel interface code) AND not being able to use standard Zig features because of Zig not having stable ABI, not even guaranteed between compiler versions makes it infeasible to have kernel modules. I want to implement module loading and a nice driver interface for the fun of it, but can't here.

Things I def do liked though:
* having such a nice and simple std.log, made it really easy to integrate
* generally, a very nicely structured stdlib for freestanding: nothing really depends on OS/specific global allocator
* nice to work with, especially metaprogramming, comptime ... Syntax is intuitive, const/var enforced, ... Very nice overall
* comptimePrint is very cool => example: generate ASM for inline functions easily
