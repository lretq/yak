#pragma once

enum {
	IPL_PASSIVE = 0,
	IPL_APC, /* unused */
	IPL_DPC,
	IPL_DEVICE,
	IPL_CLOCK = 14,
	IPL_HIGH = 15,
};

#define IPL_TO_VEC(ipl) (((ipl) << 4) - 32)
