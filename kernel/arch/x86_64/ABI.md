# Syscalls
The syscall number is passed in %rax. <br>
Arguments are passed in the following registers: %rdi, %rsi, %rdx, %r10, %r8 and %r9. <br>
A syscall is performed via the `syscall` instruction, clobbering %rcx, %r11. <br>
Return values are passed in %rax, error codes ranging from -1 to -4095.
