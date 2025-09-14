#include <stddef.h>
#include <yak-abi/syscall.h>
#include <yak/arch-syscall.h>
#include <yak-abi/fcntl.h>

void _start()
{
	__syscall2(SYS_ARCHCTL, ARCHCTL_SET_FSBASE, 0xB00B5);

	int fd0 = __syscall3(SYS_OPEN, (uint64_t)"/test", O_CREAT, O_RDWR);
	__syscall1(SYS_CLOSE, fd0);
	fd0 = __syscall3(SYS_OPEN, (uint64_t)"/test", O_CREAT, O_RDWR);
	int fd1 = __syscall3(SYS_OPEN, (uint64_t)"/test", O_CREAT, O_RDWR);
	int fd2 = __syscall3(SYS_OPEN, (uint64_t)"/test", O_CREAT, O_RDWR);

	__syscall3(SYS_WRITE, fd1, (uint64_t)"two", 3);
	__syscall3(SYS_WRITE, fd0, (uint64_t)"one", 3);

	char buf[10];
	long read = __syscall3(SYS_READ, fd2, (uint64_t)buf, sizeof(buf) - 1);
	buf[read] = '\0';
	__syscall1(SYS_DEBUG_LOG, (uint64_t)"read some bytes");
	__syscall1(SYS_DEBUG_LOG, (uint64_t)buf);

	for (;;) {
		__syscall1(SYS_DEBUG_LOG, (uint64_t)"Hello World!");
		__syscall1(SYS_DEBUG_SLEEP, 5UL * 1000 * 1000 * 1000);
	}
}
