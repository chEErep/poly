[BITS 16]
[ORG 0x7C00]

start:

    ; Инициализация сегментов
    mov ax, cs
    mov ds, ax
    mov ss, ax
    mov sp, start

    ; BIOS: вывод символа на экран
    mov ah, 0x0E

    mov al, 'H'
    int 0x10

    mov al, 'e'
    int 0x10

    mov al, 'l'
    int 0x10
    int 0x10

    mov al, 'o'
    int 0x10

inf_loop:
    jmp inf_loop

times (512 - ($ - start) - 2) db 0
db 0x55, 0xAA
