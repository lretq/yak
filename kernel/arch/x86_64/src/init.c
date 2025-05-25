#include <stdint.h>
#include <stddef.h>

#define COM1 0x3F8

static inline void outb(uint16_t port, uint8_t data)
{
	asm volatile("out %%al, %%dx" : : "a"(data), "d"(port));
}

static inline uint8_t inb(uint16_t port)
{
	uint8_t ret;
	asm volatile("in %%dx, %%al" : "=a"(ret) : "d"(port));
	return ret;
}

static inline void serial_putc(uint8_t c)
{
	while (!(inb(COM1 + 5) & 0x20))
		asm volatile("pause");
	outb(COM1, c);
}

void serial_puts(const char *str)
{
	while (*str != '\0') {
		serial_putc(*str++);
	}
}

void idt_init();
void idt_reload();
void gdt_init();
void gdt_reload();

void plat_boot()
{
	serial_puts("Yak/" ARCH " booting\r\n");
	serial_puts("Hai :3\r\n");
	idt_init();
	idt_reload();
	gdt_init();
	gdt_reload();
	asm volatile("int $200");
	serial_puts("end of init reached");
	asm volatile("hlt");
}
