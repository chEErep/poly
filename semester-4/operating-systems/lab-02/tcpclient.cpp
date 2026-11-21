#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN

#include <windows.h> //базовые Windows типы и Sleep
#include <winsock2.h>  //сокеты под Windows
#include <ws2tcpip.h> //функции уровня IP (inet_pton)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>

#pragma comment(lib, "ws2_32.lib") //подлючение библтотеки сокетов винды, без этого функции могут не слинковаться (socket,connect,send...)

#define MAX_LINE_LEN 4096 //максимальный размер строки, которую программа готова читать из входного файла

int is_empty_line(const char *line) //проверяем состоит ли строка только из пробельных символов
{
    int i = 0;
    while (line[i] != '\0')
    {
        if (line[i] != ' ' && line[i] != '\t' && line[i] != '\r' && line[i] != '\n')
            return 0;
        i++;
    }
    return 1;
}

int is_valid_phone(const char *s) //проверяем правильность формата телефона (12 символов, первый с плюса, второй 7, остальные 10 - цифры
{
    int i;

    if (s == NULL)
        return 0;

    if (strlen(s) != 12)
        return 0;

    if (s[0] != '+' || s[1] != '7')
        return 0;

    for (i = 2; i < 12; i++)
    {
        if (!isdigit((unsigned char)s[i]))
            return 0;
    }

    return 1;
}

int is_valid_time(const char *s, unsigned char *hh, unsigned char *mm, unsigned char *ss)
{//проверяем формат времени, если всё ок то возвращаем разобранные часы, минуты и секунды
    int h, m, sec;

    if (s == NULL)
        return 0;

    if (strlen(s) != 8) //длина ровно 8
        return 0;

    if (!isdigit((unsigned char)s[0]) || !isdigit((unsigned char)s[1]) ||
        s[2] != ':' ||
        !isdigit((unsigned char)s[3]) || !isdigit((unsigned char)s[4]) ||
        s[5] != ':' ||
        !isdigit((unsigned char)s[6]) || !isdigit((unsigned char)s[7]))
    { //проверяем что во второй и пятой позициях стоят двоеточия
        return 0;
    }

    h = (s[0] - '0') * 10 + (s[1] - '0');
    m = (s[3] - '0') * 10 + (s[4] - '0');
    sec = (s[6] - '0') * 10 + (s[7] - '0');

    if (h < 0 || h > 23)
        return 0;
    if (m < 0 || m > 59) //чекаем что часы минуты и секугды в определенном даппазоне
        return 0;
    if (sec < 0 || sec > 59)
        return 0;

    *hh = (unsigned char)h;
    *mm = (unsigned char)m;
    *ss = (unsigned char)sec;

    return 1;
}


char *read_long_line(FILE *f) //читаем строку любой длины, потому что Message по условию может быть очень большим
{
    int c;
    int len = 0;
    int cap = MAX_LINE_LEN;
    char *buf = (char *)malloc(cap);

    if (buf == NULL) { errno = ENOMEM; return NULL; }

    while ((c = fgetc(f)) != EOF)
    {
        if (len + 1 >= cap)
        {
            char *new_buf;
            if (cap >= 16 * 1024 * 1024) { free(buf); errno = EOVERFLOW; return NULL; }
            cap *= 2;
            new_buf = (char *)realloc(buf, cap);
            if (new_buf == NULL)
            {
                free(buf);
                errno = ENOMEM;
                return NULL;
            }
            buf = new_buf;
        }

        buf[len++] = (char)c;

        if (c == '\n')
            break;
    }

    if (len == 0 && c == EOF)
    {
        free(buf);
        return NULL;
    }

    buf[len] = '\0';
    return buf;
}

int send_all(SOCKET s, const char *buf, int len)
{ //отправляем ровно len байт
    //смысл в том что send не может за 1 раз отправить весь буфер, поэтому в цикле вайл делаем
    int sent_total = 0;
    int sent_now;

    while (sent_total < len) //отправляем часть байт пока не выполнится условие
    {
        sent_now = send(s, buf + sent_total, len - sent_total, 0);
        if (sent_now <= 0)
            return 0;
        sent_total += sent_now;
    }

    return 1;
}

int recv_all(SOCKET s, char *buf, int len) //симметрично прошлой функции, читаем ровно len байт из сокета
{
    int recv_total = 0;
    int recv_now;

    while (recv_total < len)
    {
        recv_now = recv(s, buf + recv_total, len - recv_total, 0);
        if (recv_now <= 0)
            return 0;
        recv_total += recv_now;
    }

    return 1;
}

int parse_server_address(const char *arg, char *ip, int *port) //разбираем аргумент командной строки (вид Ip:port)
{//раскладываем на строку айпи и на число порта
    const char *p;
    int ip_len;

    p = strchr(arg, ':'); //ищем двоеточик
    if (p == NULL)
        return 0;

    ip_len = (int)(p - arg); // до двоеточия это айпи
    if (ip_len <= 0 || ip_len >= 64)
        return 0;

    memcpy(ip, arg, ip_len);
    ip[ip_len] = '\0';

    char *end;
    errno = 0;
    long parsed_port = strtol(p + 1, &end, 10);
    if (p[1] == 0 || *end != 0 || errno || parsed_port < 1 || parsed_port > 65535)
        return 0;
    *port = (int)parsed_port; // //все что после это порт, с помощью atoi переводим строку в число
    if (*port <= 0 || *port > 65535)
        return 0;

    return 1;
}

int send_message_packet(SOCKET s, unsigned int msg_index, const char *phone1, const char *phone2, unsigned char hh,
unsigned char mm, unsigned char ss, const char *text) //номер сообщения, два телефона, время по байтам, текст
{ //поля одного сообщения
    unsigned int net_index;
    int text_len;
    int packet_len;
    char *packet;
    int pos;
    char ok_buf[2];

    net_index = htonl(msg_index); //функция для преобразования 32-битного значения из порядка байтов хоста в сетевой порядок байтов
    text_len = (int)strlen(text); //длина текстовой части бещ завершающего нуля

    packet_len = 4 + 12 + 12 + 1 + 1 + 1 + text_len + 1; //расчет полного размера пакета
    packet = (char *)malloc(packet_len); //создаем буфер под полный бинарный пакет
    if (packet == NULL)
        return 0;

    pos = 0; //позиция внутри пакета

    memcpy(packet + pos, &net_index, 4);
    pos += 4;

    memcpy(packet + pos, phone1, 12);
    pos += 12; //копируем поля

    memcpy(packet + pos, phone2, 12);
    pos += 12;

    packet[pos++] = (char)hh;
    packet[pos++] = (char)mm;
    packet[pos++] = (char)ss;

    memcpy(packet + pos, text, text_len);
    pos += text_len;

    packet[pos++] = '\0';

    if (!send_all(s, packet, packet_len)) //отправка пакета
    {
        free(packet); //делаем так потому что пакет больше не нужен
        return 0;
    }

    free(packet);

    if (!recv_all(s, ok_buf, 2))
        return 0;  //ждем ок

    if (ok_buf[0] != 'o' || ok_buf[1] != 'k')
        return 0;

    return 1;
}

int main(int argc, char *argv[])
{
    WSADATA wsa; //структура для WSAStartup
    SOCKET s;  //сокет клиента
    struct sockaddr_in addr; //адрес сервера
    FILE *f;   //файл с сооьщением
    char ip[64];   //строковой айпи
    int port; //номер порта
    int i;
    int connected; //флаг подключения
    char *line;  //буфер для строки файла, теперь выделяется динамически под реальную длину строки
    unsigned int msg_index = 0;  //номер сообщения, начинаем с нуля

    if (argc != 3)
    { //проверяме аргументы
        printf("Usage: tcpclient IP:Port file.txt\n");
        return 1;
    }

    if (!parse_server_address(argv[1], ip, &port))  //разбираем айпи:порт
    {
        printf("Invalid server address. Use IP:Port\n");
        return 1;
    }

    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
    {
        printf("WSAStartup() error\n");
        return 1;
    }

    s = socket(AF_INET, SOCK_STREAM, 0); //создаем сокет
    if (s == INVALID_SOCKET)
    {
        printf("socket() error\n");
        WSACleanup();
        return 1;
    }
    DWORD timeout_ms = 10000;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout_ms, sizeof(timeout_ms));
    setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout_ms, sizeof(timeout_ms));
//тут подготовка адреса сервера
    memset(&addr, 0, sizeof(addr)); //зануляем структуру
    addr.sin_family = AF_INET; //задаем семейство адресов
    addr.sin_port = htons((unsigned short)port);  //записываем порт в сетевом порядке

    if (inet_pton(AF_INET, ip, &addr.sin_addr) != 1) //перевод айпи из текста в бинарник
    {
        printf("Invalid IPv4 address\n");
        closesocket(s);
        WSACleanup();
        return 1;
    }

    connected = 0;
    for (i = 0; i < 10; i++)
    {
        if (connect(s, (struct sockaddr *)&addr, sizeof(addr)) == 0)
        {
            connected = 1;
            break;
        }  //по заданию если не удалось подлючитьяс то ждем 100 мс и траим зановодо 10 раз
        Sleep(100);
    }

    if (!connected)
    {
        printf("connect() error: cannot connect to %s:%d\n", ip, port);
        closesocket(s);
        WSACleanup();
        return 1;
    }

    if (!send_all(s, "put", 3)) //отправка пут
    {
        printf("send() error while sending 'put'\n");
        closesocket(s);
        WSACleanup();
        return 1;
    }

    f = fopen(argv[2], "r");
    if (f == NULL)
    {
        printf("Cannot open file: %s\n", argv[2]);
        closesocket(s);
        WSACleanup();
        return 1;
    }

    errno = 0;
    while ((line = read_long_line(f)) != NULL) //цикл чтения строк
    {
        char phone1[32]; //тут готовим буфер для разбора строки
        char phone2[32];
        char time_str[32];
        char *text; //указатель на текст сообщения внутри строки, чтобы не обрезать длинный Message
        unsigned char hh, mm, ss;
        char *p;
        char *token_start;

        // Убираем \r и \n с конца строки
        while (1)
        {
            int len = (int)strlen(line);
            if (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
                line[len - 1] = '\0';
            else
                break;
        }

        if (is_empty_line(line))
        {
            free(line);
            continue; //скипаем пустые строки
        }

        phone1[0] = '\0';
        phone2[0] = '\0';
        time_str[0] = '\0';

        /*
         * Читаем:
         * телефон1 телефон2 время текст_до_конца_строки
         * sscanf здесь больше не используем для текста, потому что раньше %4095 обрезал длинные сообщения.
         */
        p = line;
        while (*p == ' ' || *p == '\t')
            p++;

        token_start = p;
        while (*p != '\0' && *p != ' ' && *p != '\t')
            p++;
        if (*p == '\0' || p - token_start >= (int)sizeof(phone1))
        {
            free(line);
            continue;
        }
        memcpy(phone1, token_start, p - token_start);
        phone1[p - token_start] = '\0';
        while (*p == ' ' || *p == '\t')
            p++;

        token_start = p;
        while (*p != '\0' && *p != ' ' && *p != '\t')
            p++;
        if (*p == '\0' || p - token_start >= (int)sizeof(phone2))
        {
            free(line);
            continue;
        }
        memcpy(phone2, token_start, p - token_start);
        phone2[p - token_start] = '\0';
        while (*p == ' ' || *p == '\t')
            p++;

        token_start = p;
        while (*p != '\0' && *p != ' ' && *p != '\t')
            p++;
        if (*p == '\0' || p - token_start >= (int)sizeof(time_str))
        {
            free(line);
            continue;
        }
        memcpy(time_str, token_start, p - token_start);
        time_str[p - token_start] = '\0';
        while (*p == ' ' || *p == '\t')
            p++;

        text = p;
        if (text[0] == '\0')
        {
            free(line);
            continue;
        }

        if (!is_valid_phone(phone1))
        {
            free(line);
            continue;
        }

        if (!is_valid_phone(phone2))
        {
            free(line);
            continue;   //валидация полей
        }

        if (!is_valid_time(time_str, &hh, &mm, &ss))
        {
            free(line);
            continue;
        }

        if (!send_message_packet(s, msg_index, phone1, phone2, hh, mm, ss, text))  //отправка 1 сообшения
        {
            printf("Error while sending message %u\n", msg_index);
            free(line);
            fclose(f);
            closesocket(s);
            WSACleanup();
            return 1;
        }

        msg_index++;
        free(line);
    }

    int read_error = ferror(f) || errno == ENOMEM || errno == EOVERFLOW;
    fclose(f);
    closesocket(s);
    WSACleanup();

    if (read_error) { fprintf(stderr, "Input read error or line exceeds 16 MiB\n"); return 1; }
    return 0;
}
