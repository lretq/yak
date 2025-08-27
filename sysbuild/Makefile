init: init.o
	ld -Tlink.ld -o $@ $^

init.o: init.c
	gcc -Iinclude/ -c -o $@ -ffreestanding $^

.PHONY: copy-headers
copy-headers:
	@mkdir -p include/yak/
	cp ../kernel/include/yak/syscall.h include/yak
	cp ../kernel/arch/x86_64/include/yak/arch-syscall.h include/yak
