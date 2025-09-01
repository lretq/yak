init: init.o
	ld -Tlink.ld -o $@ $^

init.o: init.c
	gcc -Iinclude/ -fno-stack-protector -fno-stack-check -O0 -g -c -o $@ -ffreestanding $^

.PHONY: copy-headers
copy-headers:
	@mkdir -p include/yak/
	cp -ar ../kernel/include/yak-abi/ include/
	cp ../kernel/arch/x86_64/include/yak/arch-syscall.h include/yak
