#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <yak/queue.h>

struct dpc {
	int enqueued;
	void (*func)(struct dpc *self, void *context);
	void *context;
	LIST_ENTRY(dpc) list_entry;
};

void dpc_init(struct dpc *dpc, void (*func)(struct dpc *, void *));

void dpc_enqueue(struct dpc *dpc, void *context);
void dpc_dequeue(struct dpc *dpc);

struct cpu;
void dpc_queue_run(struct cpu *cpu);

#ifdef __cplusplus
}
#endif
