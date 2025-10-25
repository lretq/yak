#pragma once

#include <stdint.h>

#define ARCHCTL_SET_FSBASE 1
#define ARCHCTL_SET_GSBASE 2

struct syscall_result {
	uint64_t retval;
	long errno; // 0 for success, non-zero on error
};

static inline struct syscall_result __syscall0(int num)
{
	struct syscall_result res;
	register uint64_t _num asm("rax") = num;
	asm volatile("syscall"
		     : "=a"(res.retval), "=d"(res.errno)
		     : "a"(_num)
		     : "rcx", "r11", "memory");
	return res;
}

static inline struct syscall_result __syscall1(int num, uint64_t arg1)
{
	struct syscall_result res;
	register uint64_t _num asm("rax") = num;
	register uint64_t _arg1 asm("rdi") = arg1;
	asm volatile("syscall"
		     : "=a"(res.retval), "=d"(res.errno)
		     : "a"(_num), "r"(_arg1)
		     : "rcx", "r11", "memory");
	return res;
}

static inline struct syscall_result __syscall2(int num, uint64_t arg1,
					       uint64_t arg2)
{
	struct syscall_result res;
	register uint64_t _num asm("rax") = num;
	register uint64_t _arg1 asm("rdi") = arg1;
	register uint64_t _arg2 asm("rsi") = arg2;
	asm volatile("syscall"
		     : "=a"(res.retval), "=d"(res.errno)
		     : "a"(_num), "r"(_arg1), "r"(_arg2)
		     : "rcx", "r11", "memory");
	return res;
}

static inline struct syscall_result __syscall3(int num, uint64_t arg1,
					       uint64_t arg2, uint64_t arg3)
{
	struct syscall_result res;
	register uint64_t _num asm("rax") = num;
	register uint64_t _arg1 asm("rdi") = arg1;
	register uint64_t _arg2 asm("rsi") = arg2;
	register uint64_t _arg3 asm("rdx") = arg3;
	asm volatile("syscall"
		     : "=a"(res.retval), "=d"(res.errno)
		     : "a"(_num), "r"(_arg1), "r"(_arg2), "r"(_arg3)
		     : "rcx", "r11", "memory");
	return res;
}

static inline struct syscall_result
__syscall4(int num, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4)
{
	struct syscall_result res;
	register uint64_t _num asm("rax") = num;
	register uint64_t _arg1 asm("rdi") = arg1;
	register uint64_t _arg2 asm("rsi") = arg2;
	register uint64_t _arg3 asm("rdx") = arg3;
	register uint64_t _arg4 asm("r10") = arg4;
	asm volatile("syscall"
		     : "=a"(res.retval), "=d"(res.errno)
		     : "a"(_num), "r"(_arg1), "r"(_arg2), "r"(_arg3), "r"(_arg4)
		     : "rcx", "r11", "memory");
	return res;
}

static inline struct syscall_result __syscall5(int num, uint64_t arg1,
					       uint64_t arg2, uint64_t arg3,
					       uint64_t arg4, uint64_t arg5)
{
	struct syscall_result res;
	register uint64_t _num asm("rax") = num;
	register uint64_t _arg1 asm("rdi") = arg1;
	register uint64_t _arg2 asm("rsi") = arg2;
	register uint64_t _arg3 asm("rdx") = arg3;
	register uint64_t _arg4 asm("r10") = arg4;
	register uint64_t _arg5 asm("r8") = arg5;
	asm volatile("syscall"
		     : "=a"(res.retval), "=d"(res.errno)
		     : "a"(_num), "r"(_arg1), "r"(_arg2), "r"(_arg3),
		       "r"(_arg4), "r"(_arg5)
		     : "rcx", "r11", "memory");
	return res;
}

static inline struct syscall_result __syscall6(int num, uint64_t arg1,
					       uint64_t arg2, uint64_t arg3,
					       uint64_t arg4, uint64_t arg5,
					       uint64_t arg6)
{
	struct syscall_result res;
	register uint64_t _num asm("rax") = num;
	register uint64_t _arg1 asm("rdi") = arg1;
	register uint64_t _arg2 asm("rsi") = arg2;
	register uint64_t _arg3 asm("rdx") = arg3;
	register uint64_t _arg4 asm("r10") = arg4;
	register uint64_t _arg5 asm("r8") = arg5;
	register uint64_t _arg6 asm("r9") = arg6;
	asm volatile("syscall"
		     : "=a"(res.retval), "=d"(res.errno)
		     : "a"(_num), "r"(_arg1), "r"(_arg2), "r"(_arg3),
		       "r"(_arg4), "r"(_arg5), "r"(_arg6)
		     : "rcx", "r11", "memory");
	return res;
}

#define SYSCALL_SELECT(_0, _1, _2, _3, _4, _5, _6, NAME, ...) NAME

#define syscall(...)                                                   \
	SYSCALL_SELECT(__VA_ARGS__, _SYSCALL6_CAST, _SYSCALL5_CAST,    \
		       _SYSCALL4_CAST, _SYSCALL3_CAST, _SYSCALL2_CAST, \
		       _SYSCALL1_CAST, _SYSCALL0_CAST)(__VA_ARGS__)

#define syscall_err(...) (syscall(__VA_ARGS__).errno)

// Internal helpers that cast arguments to uint64_t
#define _SYSCALL0_CAST(num) __syscall0((int)(num))

#define _SYSCALL1_CAST(num, arg1) __syscall1((int)(num), (uint64_t)(arg1))

#define _SYSCALL2_CAST(num, arg1, arg2) \
	__syscall2((int)(num), (uint64_t)(arg1), (uint64_t)(arg2))

#define _SYSCALL3_CAST(num, arg1, arg2, arg3)                      \
	__syscall3((int)(num), (uint64_t)(arg1), (uint64_t)(arg2), \
		   (uint64_t)(arg3))

#define _SYSCALL4_CAST(num, arg1, arg2, arg3, arg4)                \
	__syscall4((int)(num), (uint64_t)(arg1), (uint64_t)(arg2), \
		   (uint64_t)(arg3), (uint64_t)(arg4))

#define _SYSCALL5_CAST(num, arg1, arg2, arg3, arg4, arg5)          \
	__syscall5((int)(num), (uint64_t)(arg1), (uint64_t)(arg2), \
		   (uint64_t)(arg3), (uint64_t)(arg4), (uint64_t)(arg5))

#define _SYSCALL6_CAST(num, arg1, arg2, arg3, arg4, arg5, arg6)          \
	__syscall6((int)(num), (uint64_t)(arg1), (uint64_t)(arg2),       \
		   (uint64_t)(arg3), (uint64_t)(arg4), (uint64_t)(arg5), \
		   (uint64_t)(arg6))
