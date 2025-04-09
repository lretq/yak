#!/bin/bash

if [ "$#" -ne 1 ]; then
    echo "Illegal number of parameters"
    exit 1
fi

arch="$1"
broot="zig-out/bin-$arch/"
limine="./vendor/limine"
iso="$broot/yak-$arch.iso"
isodir="$(mktemp -d)"

cp "$broot/kernel" "$isodir"

# build the iso
case "$arch" in
    "x86_64")
        cp "$limine/limine-bios.sys" "$limine/limine-bios-cd.bin" "$limine/limine-uefi-cd.bin" "$isodir"
        cp "misc/limine-x86_64.conf" "$isodir/limine.conf"
        make -C "$limine"
        xorriso -as mkisofs -b limine-bios-cd.bin \
		-no-emul-boot -boot-load-size 4 -boot-info-table \
		--efi-boot limine-uefi-cd.bin -efi-boot-part --efi-boot-image \
		--protective-msdos-label \
		"$isodir" -o "$iso" &&
	"$limine/limine" bios-install "$iso"
        ;;
    *)
        ;;
esac
