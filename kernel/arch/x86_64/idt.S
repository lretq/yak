.text

.set .L_NO_CODE, 1
.set .L_CODE, 2

.macro PUSHA64
push %rbp
push %r15
push %r14
push %r13
push %r12
push %r11
push %r10
push %r9
push %r8
push %rdi
push %rsi
push %rdx
push %rcx
push %rbx
push %rax
.endm

.macro POPA64
pop %rax
pop %rbx
pop %rcx
pop %rdx
pop %rsi
pop %rdi
pop %r8
pop %r9
pop %r10
pop %r11
pop %r12
pop %r13
pop %r14
pop %r15
pop %rbp
.endm

.macro ISR_STUB type, number=0
.align 16
isrStub\number:
.if \type == .L_NO_CODE
// push pseudo error code to keep struct layout
pushq $0
.endif
// push irq number
pushq $\number
// actual handling
jmp isr_common
.endm

ISR_STUB .L_NO_CODE, 0
ISR_STUB .L_NO_CODE, 1
ISR_STUB .L_NO_CODE, 2
ISR_STUB .L_NO_CODE, 3
ISR_STUB .L_NO_CODE, 4
ISR_STUB .L_NO_CODE, 5
ISR_STUB .L_NO_CODE, 6
ISR_STUB .L_NO_CODE, 7
ISR_STUB .L_CODE, 8
ISR_STUB .L_CODE, 10
ISR_STUB .L_CODE, 11
ISR_STUB .L_CODE, 12
ISR_STUB .L_CODE, 13
ISR_STUB .L_CODE, 14
ISR_STUB .L_NO_CODE, 16
ISR_STUB .L_CODE, 17
ISR_STUB .L_NO_CODE, 18
ISR_STUB .L_NO_CODE, 19

.altmacro
.set i, 32
.rept 224
	ISR_STUB .L_NO_CODE, %i
	.set i, i+1
.endr

.extern isr_c
.align 16
isr_common:

PUSHA64

mov %rsp, %rdi
xor %rbp, %rbp
call isr_c

POPA64

// pop code+number
add $2*8, %rsp
// return to context
iretq


.macro DEC_INT num
.quad isrStub\num
.endm

.rodata
.global itable
.align 16
itable:
DEC_INT 0
DEC_INT 1
DEC_INT 2
DEC_INT 3
DEC_INT 4
DEC_INT 5
DEC_INT 6
DEC_INT 7
DEC_INT 8
.quad 0x00000000
DEC_INT 10
DEC_INT 11
DEC_INT 12
DEC_INT 13
DEC_INT 14
.quad 0x00000000
DEC_INT 16
DEC_INT 17
DEC_INT 18
DEC_INT 19
.rept 12
.quad 0x00000000
.endr

.set i,32
.rept 224
DEC_INT %i
.set i,i+1
.endr
