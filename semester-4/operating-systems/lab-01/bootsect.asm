[BITS 16]  ;начало код 16 битный ( потому что в реальном режиме процессор стартует как 16 битный)
[ORG 0x7C00] ;код будет лежать в памяти по адресу 0x7C00

BOOT_PARAM_INDEX equ 0x0500 ;по адресу 0x0500 мы будем хранить номер выбранного цветa
BOOT_PARAM_ATTR  equ 0x0501 ; по адресу 0x0501 мы будем хранить готовый VGA-атрибут цвета

start:
    cli
    xor ax, ax
    mov ds, ax
    mov ss, ax
    mov sp, 0x7C00
    sti
    cld
    mov [boot_drive], dl

    mov byte [selected_idx], 0 ;задаёт начальное состояние меню

menu_loop: ;Главный цикл меню
    call draw_menu ;каждый раз перед ожиданием клавиши экран перерисовывается заново

    xor ah, ah  ;обнуление регистра AH
    int 0x16 ;вызов для клавиатуры  AL-ASCII-код AH- код клавиши

    cmp al, 0x0D ;сравниваем al с кодом клавиши Enter
    je menu_done ;если нажата клавиша Enter, то переходим к завершению меню

    cmp ah, 0x48 ;проверяем scancode клавиши вверх
    je key_up

    cmp ah, 0x50 ;проверяем scancode клавиши вниз
    je key_down

    jmp menu_loop ;если нажата не одна из этих клавиш, то просто продолжаем ждать

key_up: ;Обработка стрелки вверх
    cmp byte [selected_idx], 0 ;Проверяем, выбран ли сейчас самый верхний пункт
    jne .dec_sel ;    Если нет, то просто уменьшаем индекс выбранного пункта
    mov byte [selected_idx], 5   ;Если да, то переходим к самому нижнему пункту
    jmp menu_loop ;возвращаемся в главный цикл меню
.dec_sel:
    dec byte [selected_idx]  ;Уменьшаем индекс выбранного пункта
    jmp menu_loop ;возвращаемся в главный цикл меню

key_down: ;обработка стрелки вниз
    cmp byte [selected_idx], 5
    jne .inc_sel
    mov byte [selected_idx], 0
    jmp menu_loop
.inc_sel:
    inc byte [selected_idx]
    jmp menu_loop

menu_done: ;Завершение выбора
    xor ax, ax
    mov es, ax  ;установка сегмента ES в 0, так как мы будем использовать его для доступа к адресам 0x0500 и 0x0501

    mov al, [selected_idx] ;Берём номер выбранного пункта
    mov [es:BOOT_PARAM_INDEX], al ;Сохраняем его по адресу 0x0500

    xor bx, bx
    mov bl, al ;Кладём индекс цвета в BL
    mov al, [color_attrs + bx] ;Берём атрибут цвета из массива color_attrs по индексу выбранного пункта
    mov [es:BOOT_PARAM_ATTR], al ;Сохраняем атрибут цвета по адресу 0x0501


    ;Подготовка к загрузке ядра
    mov ax, 0x0003        ;установка видеорежима 3 (текстовый режим 80x25)
    int 0x10               ;вызов для видеокарты


    mov si, loading_str  ;вывод строки "Loading..." на экран
    call puts  ;puts - это функция, которая выводит строку на экран, она определена ниже в коде

    ; 64 сектора образа ядра; каждый читается отдельно, включая переходы дорожек.
    mov ax, 0x1100
    mov es, ax
    mov si, 1
    mov bp, 64
.read_sector:
    mov ax, si
    xor dx, dx
    mov cx, 18
    div cx
    mov cl, dl
    inc cl
    xor dx, dx
    mov bx, 2
    div bx
    mov ch, al
    mov dh, dl
    mov dl, [boot_drive]
    xor bx, bx
    mov ax, 0x0201
    int 0x13
    jc disk_error
    mov ax, es
    add ax, 0x20
    mov es, ax
    inc si
    dec bp
    jnz .read_sector

    ;Переход в protected mode
    cli ;отключаем прерывания перед переключением в защищённый режим
    lgdt [gdt_info]     ;загружаем GDT, который мы определим ниже в коде (таблица дескрипторов сегментов)

    in al, 0x92     ;читаем системный порт 0x92, который используется для управления некоторыми функциями на уровне аппаратуры, включая A20
    or al, 0x02  ;установка бита A20
    out 0x92, al   ;записываем обратно в порт 0x92, чтобы включить A20

    mov eax, cr0  ;читаем текущее значение регистра CR0, который управляет режимами работы процессора
    or eax, 0x00000001  ;установка бита PE (Protection Enable) в CR0, который включает защищённый режим
    mov cr0, eax   ;записываем обратно в CR0, чтобы включить защищённый режим

    jmp 0x08:protected_mode   ;прыжок в защищённый режим, 0x08 - это селектор кода в GDT, который мы определим ниже

draw_menu: ;Рисование меню на экране
    mov ax, 0x0003      ;установка видеорежима 3 (текстовый режим 80x25)
    int 0x10   ;вызов для видеокарты

    mov si, title_str      ;указатель на заголовок меню
    call puts            ;puts - это функция, которая выводит строку на экран, она определена ниже в коде

    xor bx, bx    ;будет использоваться как смещение для доступа к массиву menu_ptrs
    xor dx, dx          ; dl = номер пункта

.draw_next:  ;Цикл рисования всех пунктов
    mov al, ' '
    cmp dl, [selected_idx]
    jne .prefix_done    ;Если текущий пункт не выбран, то просто рисуем пробел, иначе рисуем символ '>' перед пунктом
    mov al, '>'
.prefix_done:
    call putc     ;печатаем либо пробел либо >

    mov al, ' '
    call putc

    mov si, [menu_ptrs + bx]   ;получаем указатель на строку пункта меню из массива menu_ptrs по индексу dl и выводим её на экран
    call puts  ;печатаем строку пункта меню

    mov al, 13
    call putc        ;печатаем символ возвратa, чтобы перейти на следующую строку
    mov al, 10
    call putc  ;печатаем символ новой строки

    add bx, 2      ;переходим к след указателю
    inc dl
    cmp dl, 6
    jb .draw_next      ;сравниваем с количеством пунктов меню, если не дошли то продолжаем рисовать, иначе

    ret

putc:           ;Печать символа и строки
    mov ah, 0x0E       ;функция BIOS для печати символа в текстовом режиме, AL - символ для печати, BH - страница (обычно 0), BL - атрибут цвета
    int 0x10      ; вызов для видеокарты
    ret

puts:   ;Печать строки, на которую указывает SI, до нулевого байта
.next: ;
    lodsb     ;читает следующий символ строки
    test al, al       ;проверяет, не достигли ли мы конца строки (нулевого байта)
    jz .done      ;если ноль то строка закончена
    call putc
    jmp .next
.done:
    ret

disk_error:       ;обработка ошибки при чтении с диска(если биос не смог прочитать сектор)
    mov si, error_str
    call puts
.hang:    ;зацикливаемся чтобы мусор дальше не выполнять
    jmp .hang

gdt:      ;Глобальная таблица дескрипторов (GDT) для защищённого режима (1 2 и 3 записи)
    dq 0x0000000000000000
    dq 0x00CF9A000000FFFF
    dq 0x00CF92000000FFFF

gdt_info:   ;описание GDT для загрузки с помощью инструкции LGDT
    dw gdt_end - gdt - 1 ;размер GDT минус 1
    dd gdt     ;указатель на начало GDT
gdt_end:

[BITS 32]    ;32 битная часть
protected_mode:     ;сюда загрузчик делает дальний прижок после включения защищённого режима
    mov ax, 0x10   ;селектор сегмента данных из GDT
    mov ds, ax
    mov es, ax
    mov ss, ax       ;все сегменты указывают на один и тот же дескриптор в GDT, который мы определили как дескриптор данных
    mov fs, ax
    mov gs, ax

    mov esp, 0x90000     ;стек для протектед мод
    jmp 0x11000      ;прыгаем в ядро

[BITS 16]

boot_drive db 0
selected_idx db 0    ;переменная, где хранится текущий выбранный пункт.

color_attrs db 0x07, 0x0F, 0x0E, 0x0B, 0x0D, 0x0A      ;VGA-атрибуты текста

menu_ptrs dw str_gray, str_white, str_yellow, str_cian, str_magenta, str_green    ;Таблица указателей на строки названий цветов

title_str    db "Color: Up/Down, Enter", 13, 10, 13, 10, 0      ;заголовок меню
str_gray     db "gray", 0
str_white    db "white", 0
str_yellow   db "yellow", 0
str_cian     db "cian", 0
str_magenta  db "magenta", 0
str_green    db "green", 0
loading_str  db "Loading...", 0
error_str    db "Disk!", 0

times 510 - ($ - $$) db 0     ;заполняем оставшееся место нулями, чтобы загрузочный сектор был ровно 512 байт
dw 0xAA55       ;стандартная сигнатура загрузочного сектора, которая говорит BIOS, что этот сектор можно загружать как загрузочный.
