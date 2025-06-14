#include <yak/spinlock.h>
#include <yak/status.h>
#include <yak/queue.h>
#include <yak/irq.h>
#include <yak/arch-irq.h>
#include <yak/arch-ipl.h>
#include <yak/log.h>

static struct irq_slot slots[IRQ_SLOTS];
static SPINLOCK(irq_lock);

void irq_init()
{
	for (int i = 0; i < IRQ_SLOTS; i++) {
		struct irq_slot *slot = &slots[i];
		slot->vector = i;
		slot->slot_flags = 0;
		slot->refcount = 0;
		TAILQ_INIT(&slot->objs);
	}
}

void irq_object_init(struct irq_object *obj, irq_object_handler handler,
		     void *private)
{
	obj->slot = NULL;
	obj->obj_flags = 0;
	obj->handler = handler;
	obj->private = private;
}

static void dispatch_handler([[maybe_unused]] void *frame, irq_vec_t vec)
{
	struct irq_slot *slot = &slots[vec];
	struct irq_object *obj;
	int acked = 0;

	TAILQ_FOREACH(obj, &slot->objs, entry)
	{
		if (obj->handler(obj->private) == IRQ_ACK)
			acked = 1;
	}

	if (!acked) {
		pr_warn("spurious interrupt: no acks were returned for vec %d\n",
			vec);
	}
}

static int pins_compatible(struct pin_config *a, struct pin_config *b)
{
	return (a->polarity == b->polarity || a->polarity == PIN_POL_ANY ||
		b->polarity == PIN_POL_ANY) &&
	       (a->trigger == b->trigger || b->trigger == PIN_TRG_ANY ||
		a->trigger == PIN_TRG_ANY);
}

status_t irq_alloc_ipl(struct irq_object *obj, ipl_t ipl, unsigned int flags,
		       struct pin_config pinconf)
{
	int state = spinlock_lock_interrupts(&irq_lock);
	struct irq_slot *slot, *min_count = NULL;
	irq_vec_t start = IPL_TO_VEC(ipl);
	irq_vec_t end = start + IRQ_SLOTS_PER_IPL;
	if (flags & IRQ_MIN_IPL) {
		// IPL_HIGH reserved for critical delivery
		end = IPL_TO_VEC(IPL_CLOCK);
	}

	for (irq_vec_t vec = start; vec < end; vec++) {
		slot = &slots[vec];

		if (slot->refcount) {
			if (!(slot->slot_flags & IRQ_SHAREABLE))
				continue;

			if (!(flags & IRQ_SHAREABLE))
				continue;

			if ((flags & IRQ_MOVEABLE) !=
			    (slot->slot_flags & IRQ_MOVEABLE))
				continue;

			if (!pins_compatible(&slot->pinconf, &pinconf))
				continue;
		}

		if (!min_count) {
			min_count = slot;
		} else if (min_count->refcount > slot->refcount) {
			min_count = slot;
		}
	}

	if (!min_count) {
		spinlock_unlock_interrupts(&irq_lock, state);
		return YAK_NOENT;
	}

	slot->refcount += 1;
	slot->slot_flags = flags & (IRQ_SHAREABLE | IRQ_MOVEABLE);
	slot->pinconf = pinconf;

	obj->slot = slot;
	TAILQ_INSERT_TAIL(&slot->objs, obj, entry);

	plat_set_irq_handler(slot->vector, dispatch_handler);

	spinlock_unlock_interrupts(&irq_lock, state);
	return YAK_SUCCESS;
}

status_t irq_alloc_vec(struct irq_object *obj, irq_vec_t vec,
		       unsigned int flags, struct pin_config pinconf)
{
	status_t ret = YAK_SUCCESS;
	int state = spinlock_lock_interrupts(&irq_lock);
	struct irq_slot *slot = &slots[vec];
	if (slot->refcount != 0) {
		if ((flags & IRQ_SHAREABLE) == 0 ||
		    (slot->slot_flags & IRQ_SHAREABLE) == 0) {
			pr_error("implement IRQ_FORCE\n");
			ret = YAK_NOENT;
			goto exit;
		}

		if (!pins_compatible(&slot->pinconf, &pinconf)) {
			ret = YAK_NOENT;
			goto exit;
		}
	}

	slot->refcount += 1;
	slot->slot_flags = flags & (IRQ_SHAREABLE | IRQ_MOVEABLE);
	slot->pinconf = pinconf;
	obj->slot = slot;
	TAILQ_INSERT_TAIL(&slot->objs, obj, entry);

exit:
	spinlock_unlock_interrupts(&irq_lock, state);
	return ret;
}
