.global __trap
.global trapHandler

.text
.align 8
__trap:
	/* sscratch = 0 when in supervisor context, check for that later */

	/* store sp in localdata */
	sd sp, 8(tp)

	/* CURRENT: sp=trapped sp, tp=kernel */
	sd sp, 0(tp) /* store user sp */
	ld sp, 8(tp) /* load kernel sp */

	addi sp, sp, -35*8

	/* save all registers on stack */
	sd ra, 0*8(sp)
	sd gp, 2*8(sp)
	sd t0, 4*8(sp)
	sd t1, 5*8(sp)
	sd t2, 6*8(sp)
	sd s0, 7*8(sp)
	sd s1, 8*8(sp)
	sd a0, 9*8(sp)
	sd a1, 10*8(sp)
	sd a2, 11*8(sp)
	sd a3, 12*8(sp)
	sd a4, 13*8(sp)
	sd a5, 14*8(sp)
	sd a6, 15*8(sp)
	sd a7, 16*8(sp)
	sd s2, 17*8(sp)
	sd s3, 18*8(sp)
	sd s4, 19*8(sp)
	sd s5, 20*8(sp)
	sd s6, 21*8(sp)
	sd s7, 22*8(sp)
	sd s8, 23*8(sp)
	sd s9, 24*8(sp)
	sd s10, 25*8(sp)
	sd s11, 26*8(sp)
	sd t3, 27*8(sp)
	sd t4, 28*8(sp)
	sd t5, 29*8(sp)
	sd t6, 30*8(sp)

	csrr t0, sepc
	csrr t1, sstatus
	csrr t2, scause
	csrr t3, stval
	csrr t4, sscratch
	ld t5, 0(tp)

	sd t0, 31*8(sp)
	sd t1, 32*8(sp)
	sd t2, 33*8(sp)
	sd t3, 34*8(sp)
	sd t4, 3*8(sp)
	sd t5, 1*8(sp)

	csrw sscratch, x0

	mv a0, sp
	call trapHandler

	ld t0, 31*8(sp)
	ld t1, 32*8(sp)

	csrw sepc, t0
	csrw sstatus, t1

	/* check if usermode */

	ld ra, 0*8(sp)
	ld gp, 2*8(sp)
	ld t0, 4*8(sp)
	ld t1, 5*8(sp)
	ld t2, 6*8(sp)
	ld s0, 7*8(sp)
	ld s1, 8*8(sp)
	ld a0, 9*8(sp)
	ld a1, 10*8(sp)
	ld a2, 11*8(sp)
	ld a3, 12*8(sp)
	ld a4, 13*8(sp)
	ld a5, 14*8(sp)
	ld a6, 15*8(sp)
	ld a7, 16*8(sp)
	ld s2, 17*8(sp)
	ld s3, 18*8(sp)
	ld s4, 19*8(sp)
	ld s5, 20*8(sp)
	ld s6, 21*8(sp)
	ld s7, 22*8(sp)
	ld s8, 23*8(sp)
	ld s9, 24*8(sp)
	ld s10, 25*8(sp)
	ld s11, 26*8(sp)
	ld t3, 27*8(sp)
	ld t4, 28*8(sp)
	ld t5, 29*8(sp)
	ld t6, 30*8(sp)
	ld tp, 3*8(sp)
	ld sp, 1*8(sp)

	sret
