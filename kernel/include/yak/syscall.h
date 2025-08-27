#pragma once

#include <yak/arch-syscall.h>

enum {
	SYS_ARCHCTL,
	SYS_DEBUG_SLEEP,
	SYS_DEBUG_LOG,
};

#ifdef __YAK_PRIVATE__
#define MAX_SYSCALLS 256

typedef long (*syscall_fn)(long, long, long, long, long, long);
extern syscall_fn syscall_table[MAX_SYSCALLS];

#define DEFINE_SYSCALL(num, name, ...) long sys_##name(__VA_ARGS__)

#endif
