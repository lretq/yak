#include <stdint.h>
#include <uacpi/resources.h>
#include <yak/dpc.h>
#include <yak/irq.h>
#include <yak/log.h>
#include <yak/io/acpi/AcpiDevice.hh>
#include <yak/io/acpi/AcpiPersonality.hh>
#include <yak/io/pci/Pci.hh>
#include <yak/io/base.hh>
#include <yak/io/Device.hh>
#include <yak/io/Dictionary.hh>
#include <yak/io/String.hh>
#include <yak/io/pci/PciPersonality.hh>

#include "../arch/x86_64/src/asm.h"

#define RINGBUF_SIZE 32

static const char codes[128] = {
	'\0', '\e', '1',  '2',	'3',  '4',  '5',  '6',	'7',  '8', '9', '0',
	'-',  '=',  '\b', '\t', 'q',  'w',  'e',  'r',	't',  'y', 'u', 'i',
	'o',  'p',  '[',  ']',	'\n', '\0', 'a',  's',	'd',  'f', 'g', 'h',
	'j',  'k',  'l',  ';',	'\'', '`',  '\0', '\\', 'z',  'x', 'c', 'v',
	'b',  'n',  'm',  ',',	'.',  '/',  '\0', '\0', '\0', ' ', '\0'
};

static const char codes_shifted[] = {
	'\0', '\e', '!',  '@',	'#',  '$',  '%',  '^',	'&',  '*', '(', ')',
	'_',  '+',  '\b', '\t', 'Q',  'W',  'E',  'R',	'T',  'Y', 'U', 'I',
	'O',  'P',  '{',  '}',	'\n', '\0', 'A',  'S',	'D',  'F', 'G', 'H',
	'J',  'K',  'L',  ':',	'"',  '~',  '\0', '|',	'Z',  'X', 'C', 'V',
	'B',  'N',  'M',  '<',	'>',  '?',  '\0', '\0', '\0', ' '
};

class Ps2Kbd final : public Device {
	IO_OBJ_DECLARE(Ps2Kbd);

    public:
	int probe([[maybe_unused]] Device *provider) override
	{
		return 100;
	}

	static uacpi_iteration_decision cb(void *user, uacpi_resource *resource)
	{
		Ps2Kbd *self = (Ps2Kbd *)user;
		switch (resource->type) {
		case UACPI_RESOURCE_TYPE_IRQ:
			self->gsi_ = resource->irq.irqs[0];
			break;
		case UACPI_RESOURCE_TYPE_FIXED_IO:
			if (self->data_port_ == (uint16_t)-1) {
				self->data_port_ = resource->fixed_io.address;
			} else {
				self->cmd_port_ = resource->fixed_io.address;
			}
			break;
		case UACPI_RESOURCE_TYPE_IO:
			if (self->data_port_ == (uint16_t)-1) {
				self->data_port_ = resource->io.minimum;
			} else {
				self->cmd_port_ = resource->io.minimum;
			}
			break;
		}
		return UACPI_ITERATION_DECISION_CONTINUE;
	}

	void handler()
	{
		while (tail != head) {
			auto sc = buf[tail++];
			tail %= RINGBUF_SIZE;

			if (sc == 0x2a || sc == 0xaa || sc == 0x36 ||
			    sc == 0xb6) {
				shifted = (sc & 0x80) == 0;
			}

			// released
			if (sc & 0x80)
				continue;

			if (shifted)
				printk(0, "%c", codes_shifted[sc]);
			else
				printk(0, "%c", codes[sc]);
		}
	}

	static void dpc_handler([[maybe_unused]] struct dpc *dpc, void *arg)
	{
		auto kbd = (Ps2Kbd *)arg;
		kbd->handler();
	}

	static int ps2_irq_handler(void *arg)
	{
		auto kbd = (Ps2Kbd *)arg;

		if ((inb(kbd->cmd_port_) & 0x1) == 0) {
			return IRQ_NACK;
		}

		while (inb(kbd->cmd_port_) & 0x1) {
			auto sc = inb(kbd->data_port_);
			if ((kbd->head + 1) % RINGBUF_SIZE == kbd->tail) {
				pr_warn("dropping scancode\n");
				return IRQ_ACK;
			}
			kbd->buf[kbd->head++] = sc;
			kbd->head %= RINGBUF_SIZE;
		}

		dpc_enqueue(&kbd->dpc, arg);

		return IRQ_ACK;
	}

	bool start(Device *provider) override
	{
		if (!Device::start(provider))
			return false;

		pr_info("start ps2 keyboard driver\n");
		auto acpidev = provider->safe_cast<AcpiDevice>();
		auto node = acpidev->node_;

		dpc_init(&dpc, dpc_handler);

		uacpi_resources *kb_res;
		uacpi_status ret = uacpi_get_current_resources(node, &kb_res);
		if (uacpi_unlikely_error(ret)) {
			pr_error("unable to retrieve PS2K resources: %s",
				 uacpi_status_to_string(ret));
			return false;
		}

		uacpi_for_each_resource(kb_res, cb, this);

		pr_info("ps2 kbd cmd port: 0x%x, data port: 0x%x, gsi: %d\n",
			cmd_port_, data_port_, gsi_);

		irq_object_init(&irqobj_, ps2_irq_handler, this);
		irq_alloc_ipl(&irqobj_, IPL_DEVICE, IRQ_MIN_IPL,
			      { .trigger = pin_config::PIN_TRG_LEVEL,
				.polarity = pin_config::PIN_POL_LOW });
		arch_program_intr(gsi_, irqobj_.slot->vector, false);

		uacpi_free_resources(kb_res);

		pr_info("ps2 ready :3\n");

		return true;
	}

	void stop(Device *provider) override
	{
		(void)provider;
	};

    private:
	irq_object irqobj_;
	dpc dpc;
	uint8_t gsi_ = -1;
	uint16_t data_port_ = -1;
	uint16_t cmd_port_ = -1;

	uint16_t head = 0;
	uint16_t tail = 0;
	uint8_t buf[RINGBUF_SIZE];

	bool shifted = false;
};

IO_OBJ_DEFINE(Ps2Kbd, Device);

AcpiPersonality ps2kbdPers = AcpiPersonality(&Ps2Kbd::classInfo, "PNP0303");

extern "C" void iotest_fn()
{
	auto dict = new Dictionary();
	dict->initWithSize(4);
	dict->insert("testKey1", String::fromCStr("Hello"));
	dict->insert("testKey2", String::fromCStr("World"));
	dict->insert("testKey3", String::fromCStr("From"));
	dict->insert("testKey4", String::fromCStr("Dict"));
	for (auto v : *dict) {
		auto str = v.value->safe_cast<String>();
		if (str) {
			pr_info("%s -> %s\n", v.key->getCStr(), str->getCStr());
		}
	}

	{
		auto str = String::fromCStr("test");
		pr_debug("str %p\n", str);
		str->release();
		pr_debug("Free\n");
	}

	pr_info("lookup %s: %s\n", "testKey1",
		dict->lookup("testKey1")->safe_cast<String>()->getCStr());

	auto &reg = IoRegistry::getRegistry();
	reg.getExpert().start(nullptr);
	//reg.dumpTree();

	reg.registerPersonality(&ps2kbdPers);

	reg.matchAll();
}
