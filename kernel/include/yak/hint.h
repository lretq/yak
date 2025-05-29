#pragma once

#define __hot [[gnu::hot]]

#define __cold [[gnu::cold]]

#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

#define __no_san [[clang::no_sanitize("undefined", "address")]]
#define __noreturn [[gnu::noreturn]]
