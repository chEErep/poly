#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>

#define EXPECTED_PASSWORD "lab1pass"
#define PASSWORD_SIZE 128
#define SERIAL_SIZE 16

typedef struct {
    unsigned long long size;
    unsigned long long lines;
    unsigned long long words;
    unsigned long long characters;
    unsigned long long digits;
    unsigned long long spaces;
    unsigned long long lower;
    unsigned long long upper;
} Stats;
//формирует полный путь к файлу
int make_path(const char *name, char *path, size_t path_size) {
    DWORD length = GetModuleFileNameA(NULL, path, (DWORD) path_size);
    char *slash;

    if (length == 0 || length >= path_size) {
        return 0;
    }

    slash = strrchr(path, '\\'); //ищем последний обратный слешик
    if (slash == NULL) {
        return 0;
    }

    slash[1] = '\0';
    if (strlen(path) + strlen(name) + 1 > path_size) {
        return 0;
    }

    strcat(path, name);
    return 1;
}
//открывает password.txt
int read_password(const char *path, char *password, size_t password_size) {
    FILE *file = fopen(path, "r");

    if (file == NULL) {
        return 0;
    }

    if (fgets(password, (int) password_size, file) == NULL) {
        fclose(file);
        return 0;
    }

    if (strchr(password, '\n') == NULL && !feof(file)) {
        fclose(file);
        return 0;
    }

    password[strcspn(password, "\r\n")] = '\0';
    fclose(file);
    return 1;
}

int analyze_file(const char *path, Stats *stats) { //открывает input в бинарном режиме
    FILE *file = fopen(path, "rb");
    int ch;
    int last = 0;
    int has_data = 0;
    int in_word = 0;

    if (file == NULL) {
        return 0;
    }

    while ((ch = fgetc(file)) != EOF) { //читаем файл по 1 байту
        unsigned char c = (unsigned char) ch;

        stats->size++;
        has_data = 1;
        last = ch;

        if (ch == '\n') { //подсчет строк
            stats->lines++;
        }
        if (ch != '\r' && ch != '\n') { //подсчет символов
            stats->characters++;
        }
        if (isdigit(c)) { //подсчет цифр
            stats->digits++;
        }
        if (ch == ' ') { //подсчет пробелов
            stats->spaces++;
        }
        if (islower(c)) { //подсчет нижнего регистра
            stats->lower++;
        }
        if (isupper(c)) { //верхнего регистра
            stats->upper++;
        }

        if (isspace(c)) {
            in_word = 0; //находимся ли мы внутри слова
        } else if (!in_word) { //подсчет слов
            stats->words++;
            in_word = 1;
        }
    }

    if (ferror(file)) {
        fclose(file);
        return 0;
    }

    if (has_data && last != '\n') {//доп проверка на последнюю строку тк там нету \n
        stats->lines++;
    }

    fclose(file);
    return 1;
}

int write_report(const char *path, const Stats *stats) { //создает report.txt
    FILE *file = fopen(path, "w");

    if (file == NULL) {
        return 0;
    }

    fprintf(file, "File analysis report\n");
    fprintf(file, "File size: %llu bytes\n", stats->size);
    fprintf(file, "Lines: %llu\n", stats->lines);
    fprintf(file, "Words: %llu\n", stats->words);
    fprintf(file, "Characters: %llu\n", stats->characters);
    fprintf(file, "Digits: %llu\n", stats->digits);
    fprintf(file, "Spaces: %llu\n", stats->spaces);
    fprintf(file, "Lowercase letters: %llu\n", stats->lower);
    fprintf(file, "Uppercase letters: %llu\n", stats->upper);

    if (fclose(file) != 0) {
        return 0;
    }
    return 1;
}
//генератор serial
int make_serial(const Stats *stats, char *serial, size_t serial_size) {
    unsigned long long value;
    int written;
//умножаем каждый счетчик на свое число
    value = stats->size * 31ULL;
    value += stats->lines * 37ULL;
    value += stats->words * 41ULL;
    value += stats->characters * 43ULL;
    value += stats->digits * 47ULL;
    value += stats->spaces * 53ULL;
    value += stats->lower * 59ULL;
    value += stats->upper * 61ULL;
    value %= 10000000000ULL; //ограничиваем результат

    written = snprintf(serial, serial_size, "KEY$%010llu$", value); //форматируем
    return written > 0 && (size_t) written < serial_size;
}

int write_serial(const char *path, const char *serial) {
    FILE *file = fopen(path, "w");

    if (file == NULL) {
        return 0;
    }

    if (fprintf(file, "%s\n", serial) < 0) {
        fclose(file);
        return 0;
    }

    if (fclose(file) != 0) {
        return 0;
    }
    return 1;
}

int main(void) {
    char path[MAX_PATH];
    char password[PASSWORD_SIZE];
    char serial[SERIAL_SIZE];
    Stats stats = {0};

    if (!make_path("password.txt", path, sizeof(path)) ||
        !read_password(path, password, sizeof(password))) {
        printf("Error: cannot read password.txt.\n");
        return 1;
    }

    if (strcmp(password, EXPECTED_PASSWORD) != 0) { //проверка пароля
        printf("Error: wrong password.\n");
        return 1;
    }

    if (!make_path("input.txt", path, sizeof(path)) ||
        !analyze_file(path, &stats)) {
        printf("Error: cannot read input.txt.\n");
        return 1;
    }

    if (!make_path("report.txt", path, sizeof(path)) ||
        !write_report(path, &stats)) {
        printf("Error: cannot write report.txt.\n");
        return 1;
    }

    if (!make_serial(&stats, serial, sizeof(serial))) {
        printf("Error: cannot create serial number.\n");
        return 1;
    }

    if (!make_path("serial.txt", path, sizeof(path)) ||
        !write_serial(path, serial)) {
        printf("Error: cannot write serial.txt.\n");
        return 1;
    }

    printf("Success: report.txt and serial.txt were created.\n");
    return 0;
}
