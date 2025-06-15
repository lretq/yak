#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define __percpu [[gnu::section(".percpu")]] __seg_gs

#ifdef __cplusplus
}
#endif
