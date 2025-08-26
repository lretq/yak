init: init.o
	ld -Tlink.ld -o $@ $^

init.o: init.c
	gcc -c -o $@ -ffreestanding $^
