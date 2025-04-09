.text
.extern kentry

.global _start
_start:
cld
xor %rbp, %rbp
# noreturn
jmp kentry
ud2
