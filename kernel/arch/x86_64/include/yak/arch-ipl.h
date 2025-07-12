#pragma once

#ifdef __cplusplus
extern "C" {
#endif

enum {
	IPL_PASSIVE = 0,
	IPL_APC, /* unused */
	IPL_DPC,
	IPL_DEVICE,
	IPL_CLOCK = 14,
	IPL_HIGH = 15,
};

#define IPL_TO_VEC(ipl) (((ipl) << 4) - 32)

// 256 - 32 exceptions
#define IRQ_SLOTS (224)

#define IRQ_SLOTS_PER_IPL 16

#define VEC_TO_IRQ(vec) ((vec) + 32)

#ifdef __cplusplus
}
#endif
