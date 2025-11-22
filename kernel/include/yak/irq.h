#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include <yak/ipl.h>
#include <yak/queue.h>
#include <yak/status.h>

#define IRQ_SHAREABLE (1 << 0)
#define IRQ_MOVEABLE (1 << 1)

#define IRQ_FORCE (1 << 2)
#define IRQ_MIN_IPL (1 << 3)

#define IRQ_NACK 0
#define IRQ_ACK 1

typedef unsigned int irq_vec_t;

struct irq_object;

typedef int(irq_object_handler)(void *arg);
typedef void(irq_handler)(void *frame, irq_vec_t vector);

TAILQ_HEAD(irq_obj_list, irq_object);

struct pin_config {
	enum {
		PIN_TRG_ANY,
		PIN_TRG_EDGE,
		PIN_TRG_LEVEL,
	} trigger;
	enum {
		PIN_POL_ANY,
		PIN_POL_HIGH,
		PIN_POL_LOW,
	} polarity;
};

#ifdef __cplusplus
#define PIN_CONFIG_ANY                              \
	(struct pin_config)                         \
	{                                           \
		.trigger = pin_config::PIN_TRG_ANY, \
		.polarity = pin_config::PIN_POL_ANY \
	}
#else
#define PIN_CONFIG_ANY                                          \
	(struct pin_config)                                     \
	{                                                       \
		.trigger = PIN_TRG_ANY, .polarity = PIN_POL_ANY \
	}
#endif

struct irq_slot {
	irq_vec_t vector;
	unsigned int slot_flags;
	size_t refcount;
	struct pin_config pinconf;
	struct irq_obj_list objs;
};

struct irq_object {
	struct irq_slot *slot;

	unsigned int obj_flags;

	irq_object_handler *handler;
	void *arg;

	TAILQ_ENTRY(irq_object) entry;
};

struct interrupt_override {
	uint8_t gsi;
	int edge;
	int low;
};

extern struct interrupt_override isr_overrides[];

void irq_object_init(struct irq_object *obj, irq_object_handler *handler,
		     void *arg);

// alloc with a specific vector
status_t irq_alloc_vec(struct irq_object *obj, irq_vec_t vec,
		       unsigned int flags, struct pin_config pinconf);
// alloc with ipl>=requested ipl
status_t irq_alloc_ipl(struct irq_object *obj, ipl_t ipl, unsigned int flags,
		       struct pin_config pinconf);

void plat_set_irq_handler(irq_vec_t vec, irq_handler *fn);

void arch_program_intr(uint8_t irq, irq_vec_t vector, int masked);

#ifdef __cplusplus
}
#endif
