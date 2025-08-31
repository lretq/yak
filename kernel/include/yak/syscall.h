#pragma once

#include <yak-abi/syscall.h>
#include <yak/arch-syscall.h>

#define MAX_SYSCALLS 256

typedef long (*syscall_fn)(long, long, long, long, long, long);
extern syscall_fn syscall_table[MAX_SYSCALLS];

#define DEFINE_SYSCALL(num, name, ...) long sys_##name(__VA_ARGS__)
