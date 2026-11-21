extern "C" int kmain();           // Точка входа ядра, вызываемая стартовой функцией
extern "C" void startup();        // Точка входа, вызываемая загрузчиком (стартовая функция ядра)
extern "C" void default_intr_handler();     // Обработчик прерываний по умолчанию (для всех неиспользуемых векторов)
extern "C" void keyb_handler();      // Обработчик прерывания клавиатуры

extern "C" __declspec(naked) void startup() // Точка входа, вызываемая загрузчиком (стартовая функция ядра)
{
    __asm {
		call kmain      // Вызов функции kmain, которая является основной функцией ядра
		hang :           /// Метка для бесконечного цикла, чтобы предотвратить возврат из функции kmain
		hlt         // Остановка процессора до следующего прерывания
			jmp hang       // Бесконечный цикл, чтобы процессор не выполнял случайные инструкции после завершения kmain
    }
}

#define VIDEO_BUF_PTR   ((unsigned char*)0xB8000)      // Адрес видеопамяти для текстового режима
#define VIDEO_WIDTH     80      // Ширина экрана в символах
#define VIDEO_HEIGHT    25         // Высота экрана в символах
#define CURSOR_PORT     0x3D4        // Порт управления курсором на VGA-совместимых видеокартах

#define BOOT_COLOR_INDEX_PTR ((unsigned char*)0x0500)      // Адрес, по которому загрузчик может передать индекс цвета для текста (0-5)
#define BOOT_COLOR_ATTR_PTR  ((unsigned char*)0x0501)  // Адрес, по которому загрузчик может передать атрибут цвета для текста (0-255, если индекс цвета не используется)

#define PIC1_PORT       0x20 // Порт управления первым контроллером прерываний (PIC1)
#define GDT_CS          0x08     // Селектор сегмента кода в GDT (обычно 0x08 для первого дескриптора после NULL)
#define IDT_TYPE_INTR   0x0E   // Тип дескриптора для прерываний (0x0E для 32-битного прерывания)
#define CMDLINE_MAX     40      // Максимальная длина команды, вводимой пользователем

#pragma pack(push, 1)
struct idt_entry    //структуры IDK
{
	unsigned short base_lo;       // Младшая часть адреса обработчика прерывания
	unsigned short segm_sel;        // Селектор сегмента в GDT, который будет использоваться при вызове обработчика
	unsigned char  always0;     // Всегда 0, зарезервировано для будущего использования
	unsigned char  flags;         // Флаги, определяющие тип и атрибуты дескриптора (например, тип прерывания, уровень привилегий)
	unsigned short base_hi;     // Старшая часть адреса обработчика прерывания
};

struct idt_ptr     // Структура, используемая для загрузки адреса и размера IDT в регистр IDTR
{
	unsigned short limit;    // Размер IDT в байтах минус 1 (максимальный индекс вектора прерывания)
	unsigned int   base;        // Адрес начала массива дескрипторов IDT
};
#pragma pack(pop)      //возвращаем выравнивание структур

typedef void (*intr_handler)();       // Тип для указателей на функции-обработчики прерываний

idt_entry g_idt[256];         // Массив дескрипторов IDT, по одному на каждый вектор прерывания
idt_ptr   g_idtp;          // Структура для загрузки IDT

unsigned char g_color = 0x07;     // Текущий цвет текста (по умолчанию светло-серый на черном фоне)
unsigned int  g_row = 0;      // Текущая строка курсора на экране (0-24)
unsigned int  g_col = 0;     // Текущий столбец курсора на экране (0-79)
char          g_cmd[CMDLINE_MAX + 1];    // Буфер для хранения текущей вводимой команды (макс. 40 символов + завершающий нулевой символ)
unsigned int  g_len = 0;       // Текущая длина вводимой команды (количество символов в g_cmd, не включая завершающий нулевой символ)
unsigned char g_shift = 0;        // Флаг, указывающий, нажата ли клавиша Shift (используется для определения регистра символов при вводе)

// Порты (возвращаем 1 байт, принимаем номер порта)
__inline unsigned char inb(unsigned short port)
{
    unsigned char data;
    __asm {
        push dx
        mov dx, port
        in  al, dx
        mov data, al
        pop dx
	}   //сохраняем регистр dx, загружаем в него номер порта, читаем байт из порта в al, сохраняем результат в переменную data, восстанавливаем dx
    return data;
}

__inline void outb(unsigned short port, unsigned char data)  //функия записи 1 байта в порт
{
    __asm {
        push dx
        mov dx, port
        mov al, data
        out dx, al
        pop dx
	} //сохраняем регистр dx, загружаем в него номер порта, загружаем в al данные для отправки, отправляем байт из al в порт, восстанавливаем dx
}

__inline void outw(unsigned short port, unsigned short data) //функция записи 2 байт (слова) в порт
{
    __asm {
        push dx
        mov dx, port
        mov ax, data
        out dx, ax
        pop dx
    }
}

// Экран и курсор

void write_cell(unsigned int row, unsigned int col, char ch, unsigned char color)//запись 1 символа в конкретную позицию на экране с заданным цветом
{
    unsigned int off = (row * VIDEO_WIDTH + col) * 2;  //считаем смещение в видеопамяти
	VIDEO_BUF_PTR[off] = (unsigned char)ch;  //записываем символ в видеопамять
	VIDEO_BUF_PTR[off + 1] = color;     //цвет в видеопамять (следующий байт после символа)
}

void cursor_moveto(unsigned int row, unsigned int col) //двигаем курсор
{
    unsigned short pos = (unsigned short)(row * VIDEO_WIDTH + col); //позиция ячейки

	outb(CURSOR_PORT, 0x0F); // Регистр 0x0F - младшая часть позиции курсора
	outb(CURSOR_PORT + 1, (unsigned char)(pos & 0xFF)); //пишем младшую часть позиции курсора в порт

	outb(CURSOR_PORT, 0x0E); // Регистр 0x0E - старшая часть позиции курсора
	outb(CURSOR_PORT + 1, (unsigned char)((pos >> 8) & 0xFF)); //пишем старшую часть позиции курсора в порт
}

void clear_screen(unsigned char color)  //полная очистка экрана
{
    unsigned int i;

    for (i = 0; i < VIDEO_WIDTH * VIDEO_HEIGHT; i++)
    {
        VIDEO_BUF_PTR[i * 2] = ' ';
        VIDEO_BUF_PTR[i * 2 + 1] = color;
    }

    g_row = 0;
    g_col = 0;
    cursor_moveto(g_row, g_col);
}//очищаем весь экран, заполняя его пробелами с заданным цветом, и перемещаем курсор в верхний левый угол

void next_line() //Переход на новую строку
{
	g_row++;     //переходим на следующую строку
	g_col = 0; //столбец возвращаем в начало

    if (g_row >= VIDEO_HEIGHT)
		clear_screen(g_color); //если экран заполнен, очищаем его и начинаем с верхнего левого угла
    else
        cursor_moveto(g_row, g_col); //если не переполнен то двигаем курсор
}

void put_char(char ch) //печать символа в текущую позицию
{
    if (ch == '\r')
        return;

    if (ch == '\n')
    {
        next_line();
        return;
    } //игнорируем спец.символы

    write_cell(g_row, g_col, ch, g_color); //печатаем символ

    g_col++;  //двигаем курсор
	if (g_col >= VIDEO_WIDTH) //если достигли конца строки, переходим на следующую
        next_line();
    else
        cursor_moveto(g_row, g_col);
}

void print_str(const char* s) //печать строки
{
    while (*s)
    {
        put_char(*s);
        s++;
    } //пока не дошли до нуля печатам символ
}

void print_line(const char* s)//вывод строки с переходом на новую строку
{
    print_str(s);
    put_char('\n');
}

void print_prompt() //печать приглашения к вводу команды
{
    print_str("# ");
}

void handle_backspace() //удаление последнего символа при вводе команды
{
	if (g_len == 0) //строка пустая, нечего удалять
        return;

    g_len--;

    if (g_col > 0)
    {
        g_col--;
        write_cell(g_row, g_col, ' ', g_color);
        cursor_moveto(g_row, g_col);
    }//уменьшаем длину, если курсор не в начале строки то двигаем его влево и затираем пробелом символ
}

// Строки, числа, печать

int is_digit(char ch)
{
    return (ch >= '0' && ch <= '9');
}

int str_equal(const char* a, const char* b)
{
    while (*a && *b)
    {
        if (*a != *b)
            return 0;
        a++;
        b++;
    }

    return (*a == 0 && *b == 0);
}

int str_starts_with(const char* s, const char* p) // Проверяет, начинается ли строка s с подстроки p
{
    while (*p)
    {
        if (*s != *p)
            return 0;
        s++;
        p++;
    }

    return 1;
}

const char* skip_spaces(const char* s)
{
    while (*s == ' ')
        s++;
    return s;
}

void print_uint(unsigned int value) //печать беззнакового целого числа
{
    char buf[16];
    unsigned int len = 0;

    if (value == 0)
    {
        put_char('0');
        return;
    }

    while (value > 0)
    {
        buf[len++] = (char)('0' + (value % 10));
        value /= 10;
    }

    while (len > 0)
        put_char(buf[--len]);
}//печатаем в обратном порядке, потому что записывал наоборот из-за деления на 10

void print_int(int value) //печат знакового числа
{
    if (value < 0)
    {
        put_char('-');
        print_uint((unsigned int)(-(value + 1)) + 1);
    }
    else
    {
        print_uint((unsigned int)value);
    }
}

int parse_int_checked(const char* s, int* value, unsigned int* used, int* overflow)//разобрать число из строки
{
    unsigned int i = 0; //позиция в строке
    unsigned int result = 0; //накопленное число
	unsigned int digit; //цифра текущая
    unsigned int limit = 2147483647u; //максимальное допустимое значение
	int negative = 0; //флаг отрицательного числа

    *overflow = 0;
    *used = 0; //сбрасываем параметры

    if (s[i] == '+' || s[i] == '-')
    {
        if (s[i] == '-')
            negative = 1;
        i++;
    } //разрешаем знак в начале

    if (!is_digit(s[i]))
        return 0; //обязательно цифра после знака

    if (negative)
        limit = 2147483648u; //для отрицательных чисел делаем лимит на 1 больше

	while (is_digit(s[i])) //цикл чтения цифр
    {
		digit = (unsigned int)(s[i] - '0'); //преобразуем символ в число

		if (result > (limit - digit) / 10) //проверяем, не произойдет ли переполнение при добавлении новой цифры
        {
            *overflow = 1;
            *used = i;
            return 1;
        }

		result = result * 10 + digit; //добавляем цифру к результату
		i++; //переходим к следующему символу
    }

    if (negative)
    {
        if (result == 2147483648u)
            *value = (-2147483647 - 1);
        else
            *value = -(int)result;
    }
    else
    {
        *value = (int)result;
    }

    *used = i;
    return 1;
}

unsigned int abs_int_to_uint(int value) //получить модуль числа
{
    if (value < 0)
        return (unsigned int)(-(value + 1)) + 1;
    return (unsigned int)value;
}

int safe_sub_int(int a, int b, int* result) //безопасное выполнение вычитания с проверкой на переполнение
{
    if (b > 0 && a < (-2147483647 - 1) + b)
        return 0;

    if (b < 0 && a > 2147483647 + b)
        return 0;

    *result = a - b;
    return 1;
}

// Параметр от загрузчика

unsigned char get_boot_color() //прочитать атрибут который сохранил загрузчик
{
    unsigned char color = *BOOT_COLOR_ATTR_PTR; //берем байт по адресу
    return color ? color : 0x07; //если байт нулевой то возвращаем стандартный цвет
}

const char* get_boot_color_name() //вернуть текстовое название цвета
{
    static const char* names[] =
    {
        "gray", "white", "yellow", "cian", "magenta", "green"
    };

    unsigned char idx = *BOOT_COLOR_INDEX_PTR;
    if (idx < 6)
        return names[idx];

    return "unknown";
}


// IDT и прерывания

void intr_reg_handler(int num, unsigned short sel, unsigned char flags, intr_handler h) //написать 1 обработкий в таблицу
{
	unsigned int addr = (unsigned int)h; //адрес функции-обработчика

	g_idt[num].base_lo = (unsigned short)(addr & 0xFFFF); //нижняя половина адреса обработчика
    g_idt[num].segm_sel = sel; //сегмент кода обработчика
	g_idt[num].always0 = 0;//всегда 0
	g_idt[num].flags = flags;//флаги дескриптора (тип и атрибуты)
	g_idt[num].base_hi = (unsigned short)(addr >> 16);//верхняя половина адреса обработчика
}

extern "C" __declspec(naked) void default_intr_handler() //пустой обработчик прерываний по умолчанию(для всех)
{
    __asm {
        pusha
        popa
        iretd
	}//сохраняем общие регистры, восстанавливаем их и возвращаемся из прерывания
}

void intr_init() //инициализация IDT, установка обработчика по умолчанию для всех векторов
{
    int i;
    for (i = 0; i < 256; i++)
        intr_reg_handler(i, GDT_CS, 0x80 | IDT_TYPE_INTR, default_intr_handler);
}//для всех записей ставим обработчик по умолчанию

void intr_start() //загрузка IDT в процессор
{
    g_idtp.base = (unsigned int)(&g_idt[0]);//адресс начала масства дескрипторов
	g_idtp.limit = (sizeof(g_idt) - 1); //размер таблицы в байтах минус 1

    __asm {
        lidt g_idtp  //команда загрузки регистра IDTR
    }
}

void intr_enable() //вкл прерывания
{
    __asm sti
}

void intr_disable() //выкл прерывания
{
    __asm cli
}


// Клавиатура

char translate_scan_code(unsigned char sc) //преобразовать скан код клавы в символ
{
    if (sc >= 2 && sc <= 11) return "1234567890"[sc - 2];
    if (sc == 12)             return '-';
    if (sc == 13)             return g_shift ? '+' : '=';
    if (sc >= 16 && sc <= 25) return "qwertyuiop"[sc - 16];
    if (sc >= 30 && sc <= 38) return "asdfghjkl"[sc - 30];
    if (sc >= 44 && sc <= 50) return "zxcvbnm"[sc - 44];
    if (sc == 53)             return '/';
    if (sc == 55)             return '*';
    if (sc == 57)             return ' ';
    if (sc == 78)             return '+';

    return 0;
}

// Команды

unsigned int gcd_impl(unsigned int a, unsigned int b) //алгоритм евклида для нод
{
    unsigned int r;

    while (b != 0)
    {
        r = a % b;
        a = b;
        b = r;
    }

    return a;
}

void cmd_info() //вывод инфы о системе
{
    print_line("SolverOS");
    print_line("Developer: Nikita");
    print_line("Bootloader: YASM (Intel)");
    print_line("Kernel: Microsoft C Compiler");
    print_str("Selected color: ");
    print_line(get_boot_color_name());
}

void cmd_shutdown() //завершение работы в системе
{
    print_line("Powering off...");
    outw(0x604, 0x2000);

    while (1)
        __asm hlt;
}

void cmd_gcd(const char* cmd) //полный перебор комадны gcd
{
	const char* p = skip_spaces(cmd + 3); // скипаем gcd
    int a, b, overflow;
    unsigned int used;

	if (*p == 0 || !parse_int_checked(p, &a, &used, &overflow)) //если первое число неккоректно то ошибки
    {
        print_line("Error: command incorrect");
        return;
    }

    if (overflow)
    {
        print_line("Error: Integer overflow");//переполнение - ошибка
        return;
    }

    p = skip_spaces(p + used); //переходим к след числу

    if (*p == 0 || !parse_int_checked(p, &b, &used, &overflow))
    {
        print_line("Error: command incorrect");
        return;
    }

    if (overflow)
    {
        print_line("Error: Integer overflow");
        return;
    }

    p = skip_spaces(p + used);

    if (*p != 0 || a <= 0 || b <= 0) //если после числе есть лишние символы или числа отрицательные то ошибка
    {
        print_line("Error: command incorrect");
        return;
    }

    print_str("Result: ");
    print_uint(gcd_impl((unsigned int)a, (unsigned int)b));
    put_char('\n');
}

int parse_equation(const char* s, int* a, int* b, int* c, int* overflow)
{
    const char* p = skip_spaces(s);
    unsigned int used = 0;
    *overflow = 0;
    if (*p == 'x') { *a = 1; ++p; }
    else if ((*p == '-' || *p == '+') && p[1] == 'x') {
        *a = *p == '-' ? -1 : 1; p += 2;
    } else {
        if (!parse_int_checked(p, a, &used, overflow)) return 0;
        if (*overflow) return 1;
        p += used;
        if (*p != 'x') return 0;
        ++p;
    }
    p = skip_spaces(p);
    *b = 0;
    if (*p != '=') {
        if (*p != '+' && *p != '-') return 0;
        if (!parse_int_checked(p, b, &used, overflow)) return 0;
        if (*overflow) return 1;
        p = skip_spaces(p + used);
        if (*p != '=') return 0;
    }
    p = skip_spaces(p + 1);
    if (!parse_int_checked(p, c, &used, overflow)) return 0;
    if (*overflow) return 1;
    return *skip_spaces(p + used) == 0;
}

void cmd_solve(const char* cmd) //решение уравнения вида ax+b=c
{
	const char* p = skip_spaces(cmd + 5); // скипаем solve
    int a, b, c, num, den, overflow, ok; //коэфф a, b, c из уравнения ax+b=c, числитель и знаменатель результата, флаг переполнения, флаг успешного разбора
	unsigned int g, num_abs, den_abs; //для сокращения дроби
	int neg = 0; //флаг для определения знака результата

    if (*p == 0)
    {
        print_line("Error: command incorrect");
        return; //если после solve ничего нету то плохо очень
    }

    ok = parse_equation(p, &a, &b, &c, &overflow);
    if (!ok)
    {
        print_line("Error: command incorrect"); //разбираем уравнение, елси не получилось то ошибка
        return;
    }

    if (overflow)
    {
        print_line("Error: Integer overflow");
        return;
    }

    if (a == 0)
    {
        print_line("Error: command incorrect");
        return;
    }

	if (!safe_sub_int(c, b, &num)) //вычисляем числитель num = c - b с проверкой на переполнение
    {
        print_line("Error: Integer overflow");
        return;
    }

	den = a;//знаменатель это a

    print_str("x=");

    if (num == (-2147483647 - 1) && den == -1)
    {
        print_line("Error: Integer overflow");
        return;
    }

    if ((num % den) == 0)
    {
        print_int(num / den);//если делится без остатка то прсото печатаем результат
        put_char('\n');
        return;
    }

	num_abs = abs_int_to_uint(num); //для сокращения дроби берем абсолютные значения числителя и знаменателя
    den_abs = abs_int_to_uint(den);
	g = gcd_impl(num_abs, den_abs); //находим их нод для сокращения дроби

    num_abs /= g;
    den_abs /= g;

    if ((num < 0 && den > 0) || (num > 0 && den < 0))
        neg = 1;

    if (neg)
        put_char('-');

    print_uint(num_abs);
    put_char('/');
    print_uint(den_abs);
    put_char('\n');
}

void execute_command(const char* cmd) //распознаем и выполняем команду, введенную пользователем
{
    cmd = skip_spaces(cmd);

    if (*cmd == 0) return;

    if (str_equal(cmd, "info")) { cmd_info(); return; }
    if (str_equal(cmd, "shutdown")) { cmd_shutdown(); return; }

    if (str_equal(cmd, "gcd") || str_starts_with(cmd, "gcd "))
    {
        cmd_gcd(cmd);
        return;
    }

    if (str_equal(cmd, "solve") || str_starts_with(cmd, "solve "))
    {
        cmd_solve(cmd);
        return;
    }

    print_line("Error: command not recognized");
}

// Ввод команды


void on_key(unsigned char sc) //принимает скан код нажатой клавищи
{
    char ch;

    if (sc == 14)
    {
        handle_backspace();
        return;
    }

    if (sc == 28) //ентер
    {
        g_cmd[g_len] = 0;
        put_char('\n');
        execute_command(g_cmd);
        g_len = 0;
        g_cmd[0] = 0;
        print_prompt();
        return;
    }//докидваем ноль, переносим строку, выполняем команду, сбрасываем буфер и печатаем приглашение

    ch = translate_scan_code(sc); //скан код в символ
    if (ch == 0) return;
    if (g_len >= CMDLINE_MAX) return;

    g_cmd[g_len++] = ch;
    g_cmd[g_len] = 0;
	put_char(ch); //печатаем символ и добавляем его в буфер команды, если не превышен лимит
}

void keyb_process_keys() //прочитать скан код клавиши из порта клавиатуры и обработать его
{
    unsigned char sc;

    if ((inb(0x64) & 0x01) == 0) //читаем статусный порт клавиатуры, если данных нету то всё гг
        return;

	sc = inb(0x60); //если данные есть то читаем скан код из порта данных клавиатуры

    if (sc == 42 || sc == 54) { g_shift = 1; return; }
    if (sc == 170 || sc == 182) { g_shift = 0; return; }
    if (sc >= 128)              return; //обрабатываем нажатие шифта и отпускания тоже

	on_key(sc); //передаём скан код в функцию обработки нажатия клавиши
}

extern "C" void keyb_dispatch()
{
    keyb_process_keys();
    outb(PIC1_PORT, 0x20);
}

extern "C" __declspec(naked) void keyb_handler()
{
    __asm {
        pusha
        cld
        call keyb_dispatch
        popa
        iretd
    }
}

void keyb_init() //установка обработчика прерывания клавиатуры и разрешение прерываний от клавиатуры в PIC
{
	intr_reg_handler(0x09, GDT_CS, 0x80 | IDT_TYPE_INTR, keyb_handler); //устанавливаем обработчик для вектора 0x09 (клавиатура)
	outb(PIC1_PORT + 1, 0xFF ^ 0x02); //   разрешаем прерывание от клавиатуры (IRQ1) в маске разрешенных прерываний первого контроллера PIC (0xFF - 0x02 = 0xFD, где 0x02 - это бит для IRQ1)
}

// Точка входа ядра


extern "C" int kmain() //основная функция ядра, выполняющаяся после инициализации процессора и переключения в 32-битный режим
{
	g_color = get_boot_color(); //получаем цвет от загрузчика

	clear_screen(g_color); //очищаем экран и устанавливаем цвет фона для текста
    print_line("Welcome to SolverOS!");
    print_line("");

	intr_disable(); //выключаем прерывания
	intr_init();//инициализируем IDT и ставим обработчик по умолчанию для всех векторов
	keyb_init(); //устанавливаем обработчик для клавиатуры и разрешаем её прерывания
	intr_start(); //загружаем IDT в процессор
	intr_enable(); //включаем прерывания, теперь обработчик клавиатуры будет вызываться при нажатии клавиш

	print_prompt(); //печатаем приглашение к вводу команды

    while (1)
		__asm hlt; //останавливаем процессор до следующего прерывания, чтобы не загружать его бесконечным циклом, и позволяем обрабатывать события (нажатия клавиш)

    return 0;
}
