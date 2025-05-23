#!/bin/bash

if [ "$#" -ne 2 ]; then
    echo "Illegal number of parameters"
    exit 1
fi

arch="$1"
broot="$2"

limine="./dependencies/limine/limine"
iso="$broot/yak-$arch.iso"
isodir="$(mktemp -d)"

mkdir -pv "$isodir/boot"
cp "$broot/yak.elf" "$isodir/boot/"
mkdir -pv "$isodir/boot/limine"
cp "misc/limine-$arch.conf" "$isodir/boot/limine/limine.conf"
mkdir -pv "$isodir/EFI/BOOT"

# build the iso
case "$arch" in
    "x86_64")
        cp "$limine/limine-bios.sys" "$limine/limine-bios-cd.bin" "$limine/limine-uefi-cd.bin" "$isodir/boot/limine"
        cp -v "$limine/BOOTX64.EFI" "$isodir/EFI/BOOT"
        cp -v "$limine/BOOTIA32.EFI" "$isodir/EFI/BOOT"
        make -C "$limine"
        xorriso -as mkisofs -R -r -J \
                -b boot/limine/limine-bios-cd.bin \
		-no-emul-boot -boot-load-size 4 \
                -boot-info-table -hfsplus \
                -apm-block-size 2048 \
		--efi-boot boot/limine/limine-uefi-cd.bin \
                -efi-boot-part --efi-boot-image \
		--protective-msdos-label \
		"$isodir" -o "$iso" &&
	"$limine/limine" bios-install "$iso"
        ;;
    "riscv64")
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
