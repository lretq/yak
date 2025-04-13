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
    "riscv64")
        mkdir -pv "$isodir/boot/limine"
        cp -v "misc/limine-riscv64.conf" "$isodir/boot/limine/limine.conf"
        mkdir -pv "$isodir/EFI/BOOT"
        cp -v "$limine/limine-uefi-cd.bin" "$isodir/boot/limine"
	cp -v "$limine/BOOTRISCV64.EFI" "$isodir/EFI/BOOT/"
	xorriso -as mkisofs -R -r -J \
		-hfsplus -apm-block-size 2048 \
		--efi-boot boot/limine/limine-uefi-cd.bin \
		-efi-boot-part --efi-boot-image --protective-msdos-label \
		"$isodir" -o "$iso"
        ;;
    *)
        echo "mkiso.sh does not support '$arch'"
        rm -rf "$isodir"
        exit 1
        ;;
esac

rm -rf "$isodir"
