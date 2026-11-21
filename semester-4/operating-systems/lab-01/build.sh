#!/bin/sh
set -eu
cd "$(dirname "$0")"
mkdir -p build
clang --target=i686-pc-windows-msvc -mno-sse -mno-sse2 -mno-mmx -fms-extensions -fasm-blocks -ffreestanding -fno-stack-protector -fno-exceptions -fno-rtti -fno-builtin -O1 -c kernel.cpp -o build/kernel.obj
lld-link /entry:startup /subsystem:native /base:0x10000 /fixed /nodefaultlib /safeseh:no /out:build/kernel.exe build/kernel.obj
nasm -f bin bootsect.asm -o build/bootsect.bin
python3 make_image.py
