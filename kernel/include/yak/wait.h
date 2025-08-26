#pragma once

#include <yak/status.h>
#include <yak/types.h>

#define TIMEOUT_INFINITE 0
#define POLL_ONCE 0

status_t sched_wait_single(void *object, int wait_mode, int wait_type,
			   nstime_t timeout);
