.text

.extern handleIrq
.extern handleFault

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

.macro FAULT_STUB type, number=0
irqStub\number:
.if \type == .L_NO_CODE
# 8*15 = 120 + 8
movq $0, -128(%rsp)
.elseif \type == .L_CODE
# NOTE: if something breaks after an exception occurred,
#       look at this code

# currently error code is on bottom of stack
# store rax after it
movq %rax, -8(%rsp)
# pop error code into rax
popq %rax
# store error code after the context struct
movq %rax, -128(%rsp)
# restore rax stored after error code,
# which is now popped so -8 offset
movq -16(%rsp), %rax
.endif
# TODO: swapgs

PUSHA64

movq -8(%rsp), %rdx
mov %rsp, %rdi
mov $\number, %rsi
xor %rbp, %rbp
call handleFault

POPA64

iretq
.endm

.macro SPECIAL_STUB function, number
.extern \function
irqStub\number:
# TODO: swapgs
PUSHA64
# call function
mov %rsp, %rdi
xor %rbp, %rbp
call \function

POPA64

iretq
.endm

.macro IRQ_STUB number
irqStub\number:
# TODO: swapgs
PUSHA64
mov %rsp, %rdi
mov $\number, %rsi
xor %rbp, %rbp
call handleIrq
POPA64
iretq
.endm

FAULT_STUB .L_NO_CODE, 0
FAULT_STUB .L_NO_CODE, 1
FAULT_STUB .L_NO_CODE, 2
FAULT_STUB .L_NO_CODE, 3
FAULT_STUB .L_NO_CODE, 4
FAULT_STUB .L_NO_CODE, 5
FAULT_STUB .L_NO_CODE, 6
FAULT_STUB .L_NO_CODE, 7
FAULT_STUB .L_CODE, 8
FAULT_STUB .L_CODE, 10
FAULT_STUB .L_CODE, 11
FAULT_STUB .L_CODE, 12
FAULT_STUB .L_CODE, 13
FAULT_STUB .L_CODE, 14
FAULT_STUB .L_NO_CODE, 16
FAULT_STUB .L_CODE, 17
FAULT_STUB .L_NO_CODE, 18
FAULT_STUB .L_NO_CODE, 19

# XXX: hypervisor faults

.altmacro
.set i,32
.rept 224
  IRQ_STUB %i
  .set i,i+1
.endr

.macro DEC_INT num
.quad irqStub\num
.endm

.rodata
.global itable
.align 8
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
