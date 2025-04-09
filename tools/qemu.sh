#!/bin/bash

if [ "$#" -ne 1 ]; then
    echo "Illegal number of parameters"
    exit 1
fi

ARCH="$1"
BUILDDIR="zig-out/bin-${ARCH}"

qemu_args="${QEMU_OPTARGS}"
print_command=0

case "$ARCH" in
	x86_64)
		qemu_command="qemu-system-x86_64"
		qemu_mem="${QEMU_MEM:-256M}"
		qemu_cores="${QEMU_CORES:-2}"
		qemu_args="$qemu_args -M q35"
	;;
	*)
		echo "Arch unsupported by qemu.sh"
		exit 1
	;;
esac

iso="$BUILDDIR/yak-${ARCH}.iso"

while getopts "skPDGVp" optc; do
	case $optc in
	s) qemu_args="$qemu_args -serial stdio" ;;
	k) qemu_args="$qemu_args -accel kvm -cpu host,+invtsc" ;;
	P) qemu_args="$qemu_args -S" ;;
	D) 
		qemu_args="$qemu_args -M smm=off -d int -D qemulog.txt" 
		;;
	G) 
		qemu_args="$qemu_args -nographic"
		echo "---- Exit QEMU with Ctrl+A then X ----"
		;;
	p) qemu_args="$qemu_args -debugcon stdio" ;;
	V) print_command=1 ;;
	*)
		echo "unknown option"
		exit 1
		;;
	esac
done

qemu_args="$qemu_args -s -no-shutdown -no-reboot"
qemu_args="$qemu_args -cdrom ${iso}"
qemu_args="$qemu_args -smp $qemu_cores"
qemu_args="$qemu_args -m $qemu_mem"

if [[ $print_command -eq 1 ]]; then
	echo "${qemu_command}" "${qemu_args}"
	exit
fi

${qemu_command} ${qemu_args}
