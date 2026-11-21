"""Собирает floppy-образ из загрузочного сектора и секций PE-ядра."""
from pathlib import Path
import struct


def main():
    build = Path(__file__).resolve().parent / 'build'
    pe = (build / 'kernel.exe').read_bytes()
    offset = struct.unpack_from('<I', pe, 0x3c)[0]
    if pe[offset:offset+4] != b'PE\0\0':
        raise ValueError('Ожидается PE-файл')
    sections = struct.unpack_from('<H', pe, offset+6)[0]
    optional_size = struct.unpack_from('<H', pe, offset+20)[0]
    opt = offset + 24
    entry, image_base = struct.unpack_from('<I', pe, opt+16)[0], struct.unpack_from('<I', pe, opt+28)[0]
    load_address = 0x11000
    if image_base + entry != load_address:
        raise ValueError('Точка входа ядра должна быть 0x11000')
    kernel = bytearray(64 * 512)
    for i in range(sections):
        at = opt + optional_size + i*40
        virtual_size, rva, raw_size, raw_at = struct.unpack_from('<IIII', pe, at+8)
        start = image_base + rva - load_address
        size = max(virtual_size, raw_size)
        if start < 0 or start + size > len(kernel):
            raise ValueError('Секции ядра не помещаются в 64 сектора')
        kernel[start:start+raw_size] = pe[raw_at:raw_at+raw_size]
    boot = (build/'bootsect.bin').read_bytes()
    if len(boot) != 512 or boot[-2:] != b'\x55\xaa':
        raise ValueError('Неверный загрузочный сектор')
    image = bytearray(1440 * 1024)
    image[:512] = boot
    image[512:512+len(kernel)] = kernel
    (build/'solveros.img').write_bytes(image)
    print('Built build/solveros.img (1.44 MiB)')


if __name__ == '__main__':
    main()
