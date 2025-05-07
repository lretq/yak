#!/bin/bash

usage() { 
	echo "Usage: $0 -[skPDGpV] <ARCH>"
	exit 1
}

native=0
enable_kvm=0

qemu_args="${QEMU_OPTARGS}"
print_command=0

while getopts "skPDGVp" optc; do
	case "${optc}" in
	s) qemu_args="$qemu_args -serial stdio" ;;
	k) enable_kvm=1 ;;
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
		usage
		;;
	esac
done
shift $((OPTIND-1))

if [ "$#" -eq 0 ]; then
    echo "Usage: $0 <ARCH> ..."
    exit 1
fi

ARCH="$1"
BUILDDIR="zig-out/bin-${ARCH}"
iso="$BUILDDIR/yak-${ARCH}.iso"

if [ "$(uname -m)" = "$ARCH" ]; then
	native=1
fi

ovmf_dir="zig-out/ovmf/$ARCH"
ovmf_file="$ovmf_dir/ovmf-code-$ARCH.fd"

ensure_ovmf() {
	mkdir -p "$ovmf_dir"
	if [[ ! -f "$ovmf_file" ]]; then
		echo "downloading ovmf for $ARCH ..."
		curl -Lo "$ovmf_file" "https://github.com/osdev0/edk2-ovmf-nightly/releases/latest/download/ovmf-code-$ARCH.fd"
		case "$ARCH" in
			aarch64) dd if=/dev/zero of="$ovmf_file" bs=1 count=0 seek=67108864 2>/dev/null ;;
			riscv64) dd if=/dev/zero of="$ovmf_file" bs=1 count=0 seek=33554432 2>/dev/null ;;
		esac
		
	fi
}

ensure_ovmf "$ARCH"

case "$ARCH" in
	x86_64)
		qemu_command="qemu-system-x86_64"
		qemu_mem="${QEMU_MEM:-256M}"
		qemu_cores="${QEMU_CORES:-2}"
		qemu_args="$qemu_args -M q35"
	;;
	riscv64)
		qemu_command="qemu-system-riscv64"
		qemu_mem="${QEMU_MEM:-256M}"
		qemu_cores="${QEMU_CORES:-2}"
		qemu_args="$qemu_args -M virt"
		qemu_args="$qemu_args -device ramfb"
		qemu_args="$qemu_args -cpu rv64"
	;;
	*)
		echo "Arch unsupported by qemu.sh"
		exit 1
	;;
esac

if [[ $enable_kvm -eq 1 ]]; then
	if [[ $native -eq 1 ]]; then
		qemu_args="$qemu_args -accel kvm" 
		if [ "$ARCH" = "x86_64" ]; then
			qemu_args="$qemu_args -cpu host,+invtsc"
		fi
	fi
fi

qemu_args="$qemu_args -drive if=pflash,unit=0,format=raw,file=$ovmf_file,readonly=on"
qemu_args="$qemu_args -s -no-shutdown -no-reboot"
qemu_args="$qemu_args -cdrom ${iso}"
qemu_args="$qemu_args -smp $qemu_cores"
qemu_args="$qemu_args -m $qemu_mem"

if [[ $print_command -eq 1 ]]; then
	echo "${qemu_command}" "${qemu_args}"
	exit
fi

${qemu_command} ${qemu_args}
