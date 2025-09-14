CFLAGS := -O0 -g -Iinclude/
CFLAGS += --sysroot=../sysroot
CFLAGS += -fno-PIE

LDFLAGS := -no-pie

init: init.o
	clang -v \
		$(CFLAGS) \
		$(LDFLAGS) \
		-nostdlib \
		-Wl,--dynamic-linker=/lib/ld.so \
		-Wl,-rpath=/lib \
		-Wl,-rpath-link=../sysroot/lib \
		-L../sysroot/lib -lc \
		../sysroot/lib/crt0.o \
		../sysroot/lib/crti.o \
		$^ \
		../sysroot/lib/crtn.o \
		-o $@


init.o: init.c
	clang $(CFLAGS) -c -o $@ $^ 

.PHONY: copy-headers
copy-headers:
	@mkdir -p include/yak/
	cp -ar ../kernel/include/yak-abi/ include/
	cp ../kernel/arch/x86_64/include/yak/arch-syscall.h include/yak
