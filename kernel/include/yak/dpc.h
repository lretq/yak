#pragma once

#include <yak/list.h>

struct dpc {
	int enqueued;
	void (*func)(struct dpc *self, void *context);
	void *context;
	struct list_head dpc_list;
};

void dpc_init(struct dpc *dpc, void (*func)(struct dpc *, void *));

void dpc_enqueue(struct dpc *dpc, void *context);
void dpc_dequeue(struct dpc *dpc);

struct cpu;
void dpc_queue_run(struct cpu *cpu);
