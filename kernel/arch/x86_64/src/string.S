.text

.globl memset
.type memset, @function
# void *memset(void *p, int c, size_t n)
memset:
	cld
	push %rdi
	mov %rdx, %rcx
	mov %sil, %al
	rep stosb
	pop %rax
	ret
