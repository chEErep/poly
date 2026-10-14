#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <iphlpapi.h>  //для проверок виртуальной машины надо будет
#include <cpuid.h>

#define XOR_KEY 0x5A
#define PASSWORD_SIZE 128 //выделяем 128 байт под пароль
#define SERIAL_SIZE 16
#define EXPECTED_CRC 0x8D686F4Fu  //заранее рассчитал сумму кода провереки пароля
#define PASSWORD_CODE_SIZE 0xA4u  //размер .pchk в этой сборке

typedef struct {
    unsigned long long size, lines, words, characters;
    unsigned long long digits, spaces, lower, upper;
} Stats;

//маскируем XOR
static const unsigned char E_DECOY[] = {0x3c,0x3b,0x31,0x3f,0x2a,0x3b,0x29,0x29};
static const unsigned char E_F_PASS[] = {0x2a,0x3b,0x29,0x29,0x2d,0x35,0x28,0x3e,0x74,0x2e,0x22,0x2e};
static const unsigned char E_F_INPUT[] = {0x33,0x34,0x2a,0x2f,0x2e,0x74,0x2e,0x22,0x2e};
static const unsigned char E_F_REPORT[] = {0x28,0x3f,0x2a,0x35,0x28,0x2e,0x74,0x2e,0x22,0x2e};
static const unsigned char E_F_SERIAL[] = {0x29,0x3f,0x28,0x33,0x3b,0x36,0x74,0x2e,0x22,0x2e};
static const unsigned char E_MODE_R[] = {0x28};
static const unsigned char E_MODE_RB[] = {0x28,0x38};
static const unsigned char E_MODE_W[] = {0x2d};
static const unsigned char E_ERR_PASS[] = {0x1f,0x28,0x28,0x35,0x28,0x60,0x7a,0x39,0x3b,0x34,0x34,0x35,0x2e,0x7a,0x28,0x3f,0x3b,0x3e,0x7a,0x2a,0x3b,0x29,0x29,0x2d,0x35,0x28,0x3e,0x74,0x2e,0x22,0x2e,0x74,0x50};
static const unsigned char E_ERR_WRONG[] = {0x1f,0x28,0x28,0x35,0x28,0x60,0x7a,0x2d,0x28,0x35,0x34,0x3d,0x7a,0x2a,0x3b,0x29,0x29,0x2d,0x35,0x28,0x3e,0x74,0x50};
static const unsigned char E_ERR_INPUT[] = {0x1f,0x28,0x28,0x35,0x28,0x60,0x7a,0x39,0x3b,0x34,0x34,0x35,0x2e,0x7a,0x28,0x3f,0x3b,0x3e,0x7a,0x33,0x34,0x2a,0x2f,0x2e,0x74,0x2e,0x22,0x2e,0x74,0x50};
static const unsigned char E_ERR_OUTPUT[] = {0x1f,0x28,0x28,0x35,0x28,0x60,0x7a,0x39,0x3b,0x34,0x34,0x35,0x2e,0x7a,0x2d,0x28,0x33,0x2e,0x3f,0x7a,0x35,0x2f,0x2e,0x2a,0x2f,0x2e,0x7a,0x3c,0x33,0x36,0x3f,0x29,0x74,0x50};
static const unsigned char E_ERR_PROTECT[] = {0x1f,0x28,0x28,0x35,0x28,0x60,0x7a,0x2a,0x28,0x35,0x2e,0x3f,0x39,0x2e,0x33,0x35,0x34,0x7a,0x39,0x32,0x3f,0x39,0x31,0x7a,0x3c,0x3b,0x33,0x36,0x3f,0x3e,0x74,0x50};
static const unsigned char E_SUCCESS[] = {0x09,0x2f,0x39,0x39,0x3f,0x29,0x29,0x60,0x7a,0x28,0x3f,0x2a,0x35,0x28,0x2e,0x74,0x2e,0x22,0x2e,0x7a,0x3b,0x34,0x3e,0x7a,0x29,0x3f,0x28,0x33,0x3b,0x36,0x74,0x2e,0x22,0x2e,0x7a,0x2d,0x3f,0x28,0x3f,0x7a,0x39,0x28,0x3f,0x3b,0x2e,0x3f,0x3e,0x74,0x50};
static const unsigned char E_FMT_NUMBER[] = {0x7f,0x29,0x7f,0x36,0x36,0x2f,0x50};
static const unsigned char E_FMT_SERIAL[] = {0x11,0x1f,0x03,0x7e,0x7f,0x6a,0x6b,0x6a,0x36,0x36,0x2f,0x7e};
static const unsigned char E_FMT_TEXT[] = {0x7f,0x29,0x50};
static const unsigned char E_TITLE[] = {0x1c,0x33,0x36,0x3f,0x7a,0x3b,0x34,0x3b,0x36,0x23,0x29,0x33,0x29,0x7a,0x28,0x3f,0x2a,0x35,0x28,0x2e,0x50};
static const unsigned char E_SIZE[] = {0x1c,0x33,0x36,0x3f,0x7a,0x29,0x33,0x20,0x3f,0x60,0x7a};
static const unsigned char E_LINES[] = {0x16,0x33,0x34,0x3f,0x29,0x60,0x7a};
static const unsigned char E_WORDS[] = {0x0d,0x35,0x28,0x3e,0x29,0x60,0x7a};
static const unsigned char E_CHARS[] = {0x19,0x32,0x3b,0x28,0x3b,0x39,0x2e,0x3f,0x28,0x29,0x60,0x7a};
static const unsigned char E_DIGITS[] = {0x1e,0x33,0x3d,0x33,0x2e,0x29,0x60,0x7a};
static const unsigned char E_SPACES[] = {0x09,0x2a,0x3b,0x39,0x3f,0x29,0x60,0x7a};
static const unsigned char E_LOWER[] = {0x16,0x35,0x2d,0x3f,0x28,0x39,0x3b,0x29,0x3f,0x7a,0x36,0x3f,0x2e,0x2e,0x3f,0x28,0x29,0x60,0x7a};
static const unsigned char E_UPPER[] = {0x0f,0x2a,0x2a,0x3f,0x28,0x39,0x3b,0x29,0x3f,0x7a,0x36,0x3f,0x2e,0x2e,0x3f,0x28,0x29,0x60,0x7a};
static const unsigned char E_VM1[] = {0x0c,0x33,0x28,0x2e,0x2f,0x3b,0x36,0x18,0x35,0x22};
static const unsigned char E_VM2[] = {0x0c,0x17,0x2d,0x3b,0x28,0x3f};
static const unsigned char E_VM3[] = {0x0b,0x1f,0x17,0x0f};
static const unsigned char E_VM4[] = {0x11,0x0c,0x17};
static const unsigned char E_VM5[] = {0x0c,0x18,0x15,0x02};
static const unsigned char E_VM6[] = {0x0c,0x33,0x28,0x2e,0x2f,0x3b,0x36,0x7a,0x17,0x3b,0x39,0x32,0x33,0x34,0x3f};

#define SAY(x) say(x, sizeof(x))
#define PATH(x, p) make_path(x, sizeof(x), p, sizeof(p))

//проходим по входному массиву байт, XOR делаем и записываем в out
static void decode(char *out, const unsigned char *data, size_t size) {
    size_t i;
    for (i = 0; i < size; i++) out[i] = (char)(data[i] ^ XOR_KEY);
    out[size] = '\0';
}

//создаем временный массив и выводим сообщение
static void say(const unsigned char *data, size_t size) {
    char text[128];
    if (size >= sizeof(text)) return; //чекаем что поместится сообщение
    decode(text, data, size); //расшифровываем
    fputs(text, stdout); //выводим
}
// задача найти файл рядом с exe
static int make_path(const unsigned char *name, size_t size,
                     char *path, size_t capacity) {
    char file_name[32];
    char *slash;
    DWORD length = GetModuleFileNameA(NULL, path, (DWORD)capacity);
    if (size >= sizeof(file_name) || length == 0 || length >= capacity) return 0;
    slash = strrchr(path, '\\');
    if (slash == NULL) return 0;
    slash[1] = '\0';
    decode(file_name, name, size);
    if (strlen(path) + size + 1 > capacity) return 0;
    strcat(path, file_name);
    return 1;
}
//поулчает путь и закодированный режим открытия, делаем обычный fopen
static FILE *open_file(const char *path, const unsigned char *mode, size_t size) {
    char text[4];
    if (size >= sizeof(text)) return NULL;
    decode(text, mode, size);
    return fopen(path, text);
}
//открываем файл с паролем, удаляем конечные символы и кайфуем
static int read_password(const char *path, char *password, size_t capacity) {
    FILE *file = open_file(path, E_MODE_R, sizeof(E_MODE_R));
    if (file == NULL) return 0;
    if (fgets(password, (int)capacity, file) == NULL) {
        fclose(file);
        return 0;
    }
    if (strchr(password, '\n') == NULL && !feof(file)) {
        fclose(file);
        return 0;
    }
    password[strcspn(password, "\r\n")] = '\0';
    return fclose(file) == 0;
}

//помещаем машинный код функйии в отдельную секцию PE .pchk
//noline значит не встраивать команды внутрь main
//used значит сохранить функцию в сборке
__attribute__((section(".pchk"), noinline, used))
//функция проверки пароля
//в массиве лежит пароль lab1pass, каждый ьайи заранее обработан XOR
//сравниваем в цикле восемь байтов с восстановленными байтами ожидаемого
static int check_password(const char *password) {
    const unsigned char expected[8] = {0x36,0x3b,0x38,0x6b,0x2a,0x3b,0x29,0x29};
    size_t i;
    unsigned difference = 0;
    for (i = 0; i < sizeof(expected); i++) {
        if (password[i] == '\0') return 0;
        difference |= (unsigned char)password[i] ^ (expected[i] ^ XOR_KEY);
    }
    return difference == 0 && password[i] == '\0';
}
//функция второй проверки
//считает другую характеристику, заставляем столкнуться со вторым сравнением
static int check_password_sum(const char *password) {
    unsigned sum = 0;
    size_t i;
    for (i = 0; password[i] != '\0'; i++)
        sum += (unsigned)(i + 3) * (unsigned char)password[i];
    return sum == 0x1479u; //сравниваем с 5241 в десятичной
}
// функуия ложной проверки
//сравниваем пароль с отвлекающим значением fakepass
//в дизассемблере можно принять ее за норм функцию
static int false_check(const char *password) {
    size_t i;
    if (strlen(password) != sizeof(E_DECOY)) return 0;
    for (i = 0; i < sizeof(E_DECOY); i++)
        if ((unsigned char)password[i] != (E_DECOY[i] ^ XOR_KEY)) return 0;
    return 1;
}
//получает адрес начала массива байтов и его длину
//для каждого байта обновляем 32 битное число
static uint32_t crc32(const unsigned char *data, size_t size) {
    uint32_t value = 0xFFFFFFFFu;
    size_t i;
    unsigned bit;
    for (i = 0; i < size; i++) {
        value ^= data[i];
        for (bit = 0; bit < 8; bit++)
            value = (value >> 1) ^ ((value & 1u) ? 0xEDB88320u : 0u);
    }
    return ~value;
}
//сверяем байты функции проверки пароля в текущем состоянии и при сборке
static int check_crc(void) {
    // UPX меняет таблицу секций, но восстанавливает код по прежнему адресу.
    return crc32((const unsigned char *)(uintptr_t)check_password,
                 PASSWORD_CODE_SIZE) == EXPECTED_CRC;
}
//четыре проверки отладки

// проверяет не занял ли цикл >2 сек
static int debug_time(void) {
    ULONGLONG start = GetTickCount64();
    volatile unsigned long sum = 0;
    unsigned long i;
    for (i = 0; i < 100000; i++) sum += i;
    (void)sum;
    return GetTickCount64() - start > 2000;
}
//проверяет нету ли байта 0xCC среди первых 16 байтов проверки пароля
//0xCC — машинная инструкция INT3, которую отладчик часто ставит вместо исходного байта кода при программной точке останова
static int debug_breakpoint(void) {
    const unsigned char *code = (const unsigned char *)(const void *)check_password;
    size_t i;
    for (i = 0; i < 16; i++) if (code[i] == 0xCC) return 1;
    return 0;
}
//собирает рехультаты четырех приемов, если хоть 1 найден то возвращает 1
static int debug_detected(void) {
    BOOL remote = FALSE;
    if (IsDebuggerPresent()) return 1;
    if (!CheckRemoteDebuggerPresent(GetCurrentProcess(), &remote)) return 1;
    if (remote) return 1;
    if (debug_time()) return 1;
    return debug_breakpoint();
}
// против ДИзассемблирования

//команда JMP, перескакиваем 5 байтов
__attribute__((noinline)) static void disasm_jump(void) {
    __asm__ __volatile__("jmp 1f\n\t.byte 0xE8,0xFF,0xFF,0xFF,0x7F\n1:");
}
//XOR eax eax делаем ax равным 0, срабатывает jz и мы перепрыгиваем блок UD2 и ложным call
__attribute__((noinline)) static void disasm_branch(void) {
    __asm__ __volatile__("xor %%eax,%%eax\n\ttest %%eax,%%eax\n\tjz 1f\n\t.byte 0x0F,0x0B,0xE8,0xFF,0xFF,0xFF,0x7F\n1:"
                         : : : "eax", "cc");
}
//
__attribute__((noinline)) static void disasm_overlap(void) {
    __asm__ __volatile__(".byte 0xEB,0x01,0xE9,0x90,0x90,0x90,0x90");
}

//3 признака виртуалки

//выполняет CPUID и проверяет бит 31 ECX - признак наличия гипервизора
static int vm_cpuid(void) {
    unsigned eax, ebx, ecx, edx;
    if (!__get_cpuid(1, &eax, &ebx, &ecx, &edx)) return 0;
    return (ecx & (1u << 31)) != 0;
}
//просматриваем сетевые адаптеры
static int vm_mac(void) {
    ULONG size = 0;
    IP_ADAPTER_INFO *first, *adapter;
    DWORD result = GetAdaptersInfo(NULL, &size);
    int found = 0;
    if (result != ERROR_BUFFER_OVERFLOW || size == 0) return 0;
    first = (IP_ADAPTER_INFO *)malloc(size);
    if (first == NULL) return 0;
    if (GetAdaptersInfo(first, &size) == NO_ERROR) {
        for (adapter = first; adapter != NULL; adapter = adapter->Next) {
            const BYTE *m = adapter->Address;
            if (adapter->AddressLength < 3) continue;
            if ((m[0] == 0x08 && m[1] == 0x00 && m[2] == 0x27) ||
                (m[0] == 0x52 && m[1] == 0x54 && m[2] == 0x00) ||
                (m[0] == 0x00 && m[1] == 0x15 && m[2] == 0x5D) ||
                (m[0] == 0x00 && m[1] == 0x05 && m[2] == 0x69) ||
                (m[0] == 0x00 && m[1] == 0x0C && m[2] == 0x29) ||
                (m[0] == 0x00 && m[1] == 0x50 && m[2] == 0x56)) found = 1;
        }
    }
    free(first);
    return found;
}
//функция помогатор, ищет в поолученных байтах названия вроде виртуал бокса, вм вер кему
static int has_marker(const unsigned char *data, size_t size,
                      const unsigned char *marker, size_t length) {
    size_t i, j;
    if (length > size) return 0;
    for (i = 0; i <= size - length; i++) {
        for (j = 0; j < length; j++) {
            unsigned char a = data[i + j];
            unsigned char b = marker[j] ^ XOR_KEY;
            if (a >= 'a' && a <= 'z') a -= 32;
            if (b >= 'a' && b <= 'z') b -= 32;
            if (a != b) break;
        }
        if (j == length) return 1;
    }
    return 0;
}
//читает таблицу SMBIOS через GetSystemFirmwareTable
static int vm_firmware(void) {
    DWORD size = GetSystemFirmwareTable(0x52534D42u, 0, NULL, 0);
    unsigned char *data;
    int found;
    if (size == 0 || size > 1024 * 1024) return 0;
    data = (unsigned char *)malloc(size);
    if (data == NULL) return 0;
    if (GetSystemFirmwareTable(0x52534D42u, 0, data, size) != size) {
        free(data);
        return 0;
    }
    found = has_marker(data, size, E_VM1, sizeof(E_VM1)) ||
            has_marker(data, size, E_VM2, sizeof(E_VM2)) ||
            has_marker(data, size, E_VM3, sizeof(E_VM3)) ||
            has_marker(data, size, E_VM4, sizeof(E_VM4)) ||
            has_marker(data, size, E_VM5, sizeof(E_VM5)) ||
            has_marker(data, size, E_VM6, sizeof(E_VM6));
    free(data);
    return found;
}
//Гипервизор и виртуальный адаптер могут быть и на обычном компьютере.
//Поэтому для отказа нужен также признак виртуальной машины в SMBIOS.
static int vm_detected(void) {
    int cpu = vm_cpuid();
    int mac = vm_mac();
    int firmware = vm_firmware();
    return firmware && (cpu || mac);
}

//самомодифицирующийся код

//создаем и меняем маленькую программу в выделенной памяти
//virtualalloc выделяет страницу памяти с правом записи
//memcpy кладет 6 байтов машинного кода
//указатель позволяет вызвать байты как функцию, первый резульатт равен 7
//права снова переключать на запись, code[1] =9 меняем число 7 на 9
//Страница снова становится исполняемой; FlushInstructionCache сообщает процессору об обновлении
//Второй вызов возвращает 9. Если получено именно 7 и 9, функция возвращает успех
static int change_code(void) {
    unsigned char bytes[] = {0xB8, 7, 0, 0, 0, 0xC3}; //байты типа mov eax, 7; ret
    unsigned char *code = VirtualAlloc(NULL, 4096, MEM_COMMIT | MEM_RESERVE,
                                       PAGE_READWRITE);
    DWORD old_protection;
    int (*run)(void);
    int first, second;
    int ok = 0;
    if (code == NULL) return 0;
    memcpy(code, bytes, sizeof(bytes));
    if (!VirtualProtect(code, 4096, PAGE_EXECUTE_READ, &old_protection)) goto done;
    FlushInstructionCache(GetCurrentProcess(), code, sizeof(bytes));
    run = (int (*)(void))code;
    first = run();
    if (!VirtualProtect(code, 4096, PAGE_READWRITE, &old_protection)) goto done;
    code[1] = 9;
    if (!VirtualProtect(code, 4096, PAGE_EXECUTE_READ, &old_protection)) goto done;
    FlushInstructionCache(GetCurrentProcess(), code, sizeof(bytes));
    second = run();
    ok = first == 7 && second == 9;
done:
    VirtualFree(code, 0, MEM_RELEASE);
    return ok;
}
//анализ инпута (rb-бинарное чтение)
static int analyze_file(const char *path, Stats *s) {
    FILE *file = open_file(path, E_MODE_RB, sizeof(E_MODE_RB));
    int ch, last = 0, has_data = 0, in_word = 0;
    if (file == NULL) return 0;
    while ((ch = fgetc(file)) != EOF) {
        unsigned char c = (unsigned char)ch;
        s->size++;
        has_data = 1;
        last = ch;
        if (ch == '\n') s->lines++;
        if (ch != '\r' && ch != '\n') s->characters++;
        if (isdigit(c)) s->digits++;
        if (ch == ' ') s->spaces++;
        if (islower(c)) s->lower++;
        if (isupper(c)) s->upper++;
        if (isspace(c)) in_word = 0;
        else if (!in_word) { s->words++; in_word = 1; }
    }
    if (ferror(file)) { fclose(file); return 0; }
    if (has_data && last != '\n') s->lines++;
    return fclose(file) == 0;
}

typedef struct {
    const char *password;
    char *path;
    size_t path_size;
    Stats *stats;
    unsigned *stages;
} FlowData;

typedef int (*FlowStep)(FlowData *);

static int step_input(FlowData *flow) {
    if (!make_path(E_F_INPUT, sizeof(E_F_INPUT), flow->path, flow->path_size) ||
        !analyze_file(flow->path, flow->stats)) return -1;
    return 2;
}

static int step_finish(FlowData *flow) {
    (void)flow;
    disasm_branch();
    return 3;
}

static int step_password(FlowData *flow) {
    int ok = check_password_sum(flow->password);
    *flow->stages += (unsigned)ok * 4;
    if (!ok) return -2;
    return 1;
}

// Порядок шагов скрыт в таблице: 0 -> 2 -> 1 -> конец.
static int run_flow(const char *password, char *path, size_t path_size,
                    Stats *stats, unsigned *stages) {
    FlowData flow = {password, path, path_size, stats, stages};
    FlowStep steps[3] = {step_input, step_finish, step_password};
    unsigned state = 0 ^ XOR_KEY;
    unsigned count;
    for (count = 0; count < 3; count++) {
        unsigned index = state ^ XOR_KEY;
        int next;
        if (index >= 3) return 3;
        next = steps[index](&flow);
        if (next < 0) return -next;
        if (next == 3) return 0;
        state = (unsigned)next ^ XOR_KEY;
    }
    return 3;
}

static int write_number(FILE *file, const unsigned char *label,
                        size_t size, unsigned long long number) {
    char text[32], format[16];
    if (size >= sizeof(text)) return 0;
    decode(text, label, size);
    decode(format, E_FMT_NUMBER, sizeof(E_FMT_NUMBER));
    return fprintf(file, format, text, number) >= 0;
}

#define NUMBER(label, value) write_number(file, label, sizeof(label), value)

static int write_report(const char *path, const Stats *s) {
    FILE *file = open_file(path, E_MODE_W, sizeof(E_MODE_W));
    char title[32];
    int ok;
    if (file == NULL) return 0;
    decode(title, E_TITLE, sizeof(E_TITLE));
    ok = fputs(title, file) != EOF;
    ok = ok && NUMBER(E_SIZE, s->size);
    ok = ok && NUMBER(E_LINES, s->lines);
    ok = ok && NUMBER(E_WORDS, s->words);
    ok = ok && NUMBER(E_CHARS, s->characters);
    ok = ok && NUMBER(E_DIGITS, s->digits);
    ok = ok && NUMBER(E_SPACES, s->spaces);
    ok = ok && NUMBER(E_LOWER, s->lower);
    ok = ok && NUMBER(E_UPPER, s->upper);
    if (fclose(file) != 0) ok = 0;
    return ok;
}

static int make_serial(const Stats *s, unsigned stages,
                       char *serial, size_t capacity) {
    unsigned long long value;
    char format[32];
    int written;
    if (stages != 31) return 0;
    value = s->size * 31ULL + s->lines * 37ULL + s->words * 41ULL;
    value += s->characters * 43ULL + s->digits * 47ULL;
    value += s->spaces * 53ULL + s->lower * 59ULL + s->upper * 61ULL;
    value %= 10000000000ULL;
    decode(format, E_FMT_SERIAL, sizeof(E_FMT_SERIAL));
    written = snprintf(serial, capacity, format, value);
    return written > 0 && (size_t)written < capacity;
}

static int write_serial(const char *path, const char *serial) {
    FILE *file = open_file(path, E_MODE_W, sizeof(E_MODE_W));
    char format[8];
    int ok;
    if (file == NULL) return 0;
    decode(format, E_FMT_TEXT, sizeof(E_FMT_TEXT));
    ok = fprintf(file, format, serial) >= 0;
    if (fclose(file) != 0) ok = 0;
    return ok;
}
/*
 *порядок такой:
 *1)Первая CRC-проверка .pchk
 2)Anti-debug и anti-VM, затем чтение и точная проверка пароля
  3)Первая anti-disassembly-вставка, затем run_flow: анализ файла и вторая проверка
 4)Вторая вставка и самомодифицирующийся код
 5)Третья вставка и повторная CRC-проверка
 6)Отчёт и serial*/

int main(void) {
    char path[MAX_PATH], password[PASSWORD_SIZE], serial[SERIAL_SIZE];
    Stats stats = {0};
    unsigned stages = 0;
    int flow_result;
    int ok = check_crc();
    stages += (unsigned)ok;
    if (!ok) { SAY(E_ERR_PROTECT); return 1; }
    if (debug_detected() || vm_detected()) { SAY(E_ERR_PROTECT); return 1; }
    if (!PATH(E_F_PASS, path) || !read_password(path, password, sizeof(password))) {
        SAY(E_ERR_PASS); return 1;
    }
    ok = check_password(password);
    stages += (unsigned)ok * 2;
    if (false_check(password) || !ok) {
        SAY(E_ERR_WRONG); return 1;
    }
    disasm_jump();
    flow_result = run_flow(password, path, sizeof(path), &stats, &stages);
    if (flow_result == 1) { SAY(E_ERR_INPUT); return 1; }
    if (flow_result == 2) { SAY(E_ERR_WRONG); return 1; }
    if (flow_result != 0) { SAY(E_ERR_PROTECT); return 1; }
    ok = change_code();
    stages += (unsigned)ok * 8;
    if (!ok) { SAY(E_ERR_PROTECT); return 1; }
    disasm_overlap();
    ok = check_crc();
    stages += (unsigned)ok * 16;
    if (!ok) { SAY(E_ERR_PROTECT); return 1; }
    memset(password, 0, sizeof(password));
    if (!make_serial(&stats, stages, serial, sizeof(serial)) ||
        !PATH(E_F_REPORT, path) || !write_report(path, &stats) ||
        !PATH(E_F_SERIAL, path) || !write_serial(path, serial)) {
        SAY(E_ERR_OUTPUT); return 1;
    }
    SAY(E_SUCCESS);
    return 0;
}
