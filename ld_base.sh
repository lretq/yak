#!/bin/fish
printf "0x%x\n" (math (readelf -WS sysroot/lib/ld.so | rg .text | awk '{ print "0x"$5 }') + 0x7ffff7dd7000)
