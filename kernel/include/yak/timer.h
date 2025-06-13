#pragma once

#include <heap.h>
#include <yak/spinlock.h>
#include <yak/object.h>
#include <yak/types.h>
#include <yak/dpc.h>
#include <yak/status.h>

enum {
	TIMER_STATE_UNUSED = 1,
	TIMER_STATE_QUEUED,
	TIMER_STATE_CANCELED,
	TIMER_STATE_FIRED,
};

#define TIMER_INFINITE UINT64_MAX

struct timer;

// get nanoseconds from a monotonic ns clock
nstime_t plat_getnanos();
// arm to deadline relative to clock from above
void plat_arm_timer(nstime_t deadline);

struct timer {
	struct kobject_header hdr;

	struct cpu *cpu;

	nstime_t deadline;
	short state;

	HEAP_ENTRY(timer) entry;
};

void timer_init(struct timer *timer);
void timer_reset(struct timer *timer);

status_t timer_install(struct timer *timer, nstime_t ns_delta);
void timer_uninstall(struct timer *timer);
