#pragma once

#ifdef __cplusplus
extern "C" {
#endif

[[gnu::format(printf, 1, 2), gnu::noreturn]]
void panic(const char *fmt, ...);

int is_in_panic();

#ifdef __cplusplus
}
#endif
