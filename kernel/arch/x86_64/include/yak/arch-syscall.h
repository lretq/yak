#pragma once

#include <stdint.h>

#define ARCHCTL_SET_FSBASE 1
#define ARCHCTL_SET_GSBASE 2

static inline long __syscall0(int num)
{
	long ret;
	asm volatile("syscall" : "=a"(ret) : "a"(num) : "memory", "r11", "rcx");
	return ret;
}

static inline long __syscall1(int num, uint64_t arg1)
{
	long ret;
	asm volatile("syscall"
		     : "=a"(ret)
		     : "a"(num), "D"(arg1)
		     : "memory", "r11", "rcx");
	return ret;
}

static inline long __syscall2(int num, uint64_t arg1, uint64_t arg2)
{
	long ret;
	asm volatile("syscall"
		     : "=a"(ret)
		     : "a"(num), "D"(arg1), "S"(arg2)
		     : "memory", "r11", "rcx");
	return ret;
}

static inline long __syscall3(int num, uint64_t arg1, uint64_t arg2,
			      uint64_t arg3)
{
	long ret;
	asm volatile("syscall"
		     : "=a"(ret)
		     : "a"(num), "D"(arg1), "S"(arg2), "d"(arg3)
		     : "memory", "r11", "rcx");
	return ret;
}

static inline long __syscall4(int num, uint64_t arg1, uint64_t arg2,
			      uint64_t arg3, uint64_t arg4)
{
	long ret;

	register uint64_t _arg4 asm("r10") = arg4;

	asm volatile("syscall"
		     : "=a"(ret)
		     : "a"(num), "D"(arg1), "S"(arg2), "d"(arg3), "r"(_arg4)
		     : "memory", "r11", "rcx");
	return ret;
}

static inline long __syscall5(int num, uint64_t arg1, uint64_t arg2,
			      uint64_t arg3, uint64_t arg4, uint64_t arg5)
{
	long ret;

	register uint64_t _arg4 asm("r10") = arg4;
	register uint64_t _arg5 asm("r8") = arg5;

	asm volatile("syscall"
		     : "=a"(ret)
		     : "a"(num), "D"(arg1), "S"(arg2), "d"(arg3), "r"(_arg4),
		       "r"(_arg5)
		     : "memory", "r11", "rcx");
	return ret;
}

static inline long __syscall6(int num, uint64_t arg1, uint64_t arg2,
			      uint64_t arg3, uint64_t arg4, uint64_t arg5,
			      uint64_t arg6)
{
	long ret;

	register uint64_t _arg4 asm("r10") = arg4;
	register uint64_t _arg5 asm("r8") = arg5;
	register uint64_t _arg6 asm("r9") = arg6;

	asm volatile("syscall"
		     : "=a"(ret)
		     : "a"(num), "D"(arg1), "S"(arg2), "d"(arg3), "r"(_arg4),
		       "r"(_arg5), "r"(_arg6)
		     : "memory", "r11", "rcx");
	return ret;
}
