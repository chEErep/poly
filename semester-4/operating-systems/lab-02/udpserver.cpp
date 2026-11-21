#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

#pragma comment(lib, "ws2_32.lib")

#define MAX_PORTS 32
#define BUF_SIZE 8192

#define MAX_CLIENTS 64
#define MAX_MSGS_PER_CLIENT 512
#define MAX_REPLY_MSGS 20

struct udp_socket_ctx //массив сокетов (событий для портов)
{
    SOCKET s;
    WSAEVENT ev;
    unsigned short port;
};

struct client_ctx  //массив клиентов с историей номеров сообщений
{
    int used;
    unsigned int ip;
    unsigned short port;

    unsigned int msg_indices[MAX_MSGS_PER_CLIENT];
    int msg_count;
};

static struct udp_socket_ctx g_sockets[MAX_PORTS];
static WSAEVENT g_events[MAX_PORTS];
static struct client_ctx g_clients[MAX_CLIENTS];

int set_non_block_mode(SOCKET s)
{
    unsigned long mode = 1;
    return ioctlsocket(s, FIONBIO, &mode);
}

void close_ctx(struct udp_socket_ctx *ctx) //функция закрывает ресурсы одного UDP-порта
{
    if (ctx->s != INVALID_SOCKET)
    {
        closesocket(ctx->s);
        ctx->s = INVALID_SOCKET;
    }

    if (ctx->ev != WSA_INVALID_EVENT)
    {
        WSACloseEvent(ctx->ev);
        ctx->ev = WSA_INVALID_EVENT;
    }

    ctx->port = 0;
}

void print_ip(unsigned int ip)
{
    printf("%u.%u.%u.%u",
           (ip >> 24) & 0xFF,
           (ip >> 16) & 0xFF,
           (ip >> 8) & 0xFF,
           ip & 0xFF);
}

void init_client(struct client_ctx *c) //инициализация клиента
{
    c->used = 0;
    c->ip = 0;
    c->port = 0;
    c->msg_count = 0;
}

int create_udp_socket_on_port(struct udp_socket_ctx *ctx, unsigned short port) //создание сокета
{
    struct sockaddr_in addr;

    ctx->s = INVALID_SOCKET;
    ctx->ev = WSA_INVALID_EVENT;
    ctx->port = 0;

    ctx->s = socket(AF_INET, SOCK_DGRAM, 0); //создает сокет
    if (ctx->s == INVALID_SOCKET)
        return 0;

    if (set_non_block_mode(ctx->s) != 0) //переводим в неблок режим
    {
        close_ctx(ctx);
        return 0;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(ctx->s, (struct sockaddr *)&addr, sizeof(addr)) != 0)
    {
        close_ctx(ctx);
        return 0;
    }

    ctx->ev = WSACreateEvent();
    if (ctx->ev == WSA_INVALID_EVENT)
    {
        close_ctx(ctx);
        return 0;
    }

    if (WSAEventSelect(ctx->s, ctx->ev, FD_READ) != 0) //связывает сокет и событие
    {
        close_ctx(ctx);
        return 0;
    }

    ctx->port = port; //сохраняем номер порта
    return 1;
}

int append_message_to_file(const char *filename, unsigned int ip, unsigned short port, const char *phone1, const char *phone2, unsigned char hh, unsigned char mm, unsigned char ss, const char *text)
{
    FILE *f;
    f = fopen(filename, "a"); //открываем файл в режиме append
    if (f == NULL)
        return 0;
    int written = fprintf(f, //дописываем строку в нужном на формате
            "%u.%u.%u.%u:%u %s %s %02u:%02u:%02u %s\n",
            (ip >> 24) & 0xFF,
            (ip >> 16) & 0xFF,
            (ip >> 8) & 0xFF, ip & 0xFF, port, phone1, phone2, (unsigned int)hh, (unsigned int)mm, (unsigned int)ss, text);
    int closed = fclose(f);
    return written >= 0 && closed == 0;
}

/*
Формат:
4 байта index
12 байт phone1
12 байт phone2
1 байт hh
1 байт mm
1 байт ss
N байт text
1 байт '\0'
*/
int parse_message(const char *buf, int len, unsigned int *msg_index, char *phone1, char *phone2, unsigned char *hh, unsigned char *mm, unsigned char *ss, char *text)
{ //функция разбора udp пакета
    unsigned int idx_net;
    int text_len;
    if (len < 32) //чекаем длину чтобы не меньше 32
        return 0;
    if (buf[len - 1] != '\0') //чекаем последний байт
        return 0;

    memcpy(&idx_net, buf, 4);
    *msg_index = ntohl(idx_net); //читаем номер телефона и переводим его

    memcpy(phone1, buf + 4, 12); //копируем телефоны
    phone1[12] = '\0';

    memcpy(phone2, buf + 16, 12);
    phone2[12] = '\0';

    for (int p = 4; p <= 16; p += 12) {
        if (buf[p] != '+' || buf[p+1] != '7') return 0;
        for (int j = 2; j < 12; ++j) if (!isdigit((unsigned char)buf[p+j])) return 0;
    }
    if ((unsigned char)buf[28] > 23 || (unsigned char)buf[29] > 59 || (unsigned char)buf[30] > 59) return 0;
    if (memchr(buf + 31, 0, len - 32) != NULL) return 0;

    *hh = (unsigned char)buf[28]; //вычисляем время
    *mm = (unsigned char)buf[29];
    *ss = (unsigned char)buf[30];

    text_len = len - 32; //вычисляем длину текста
    if (text_len < 0 || text_len >= BUF_SIZE)
        return 0;

    for (int j = 0; j < text_len; ++j) if (buf[31+j] == '\n' || buf[31+j] == '\r') return 0;
    memcpy(text, buf + 31, text_len);
    text[text_len] = '\0'; //копируем текст и ставим 0 в конце

    return 1;
}

int find_client(unsigned int ip, unsigned short port)
{ //реализация базы клиентов
    int i;

    for (i = 0; i < MAX_CLIENTS; i++)
    { //просто линейно ищем записб по полям айпи и порта
        if (g_clients[i].used &&
            g_clients[i].ip == ip &&
            g_clients[i].port == port)
        {
            return i;
        }
    }

    return -1;
}

int create_client(unsigned int ip, unsigned short port)
{ //ищем свободный слот и заполняем поля
    int i;

    for (i = 0; i < MAX_CLIENTS; i++)
    {
        if (!g_clients[i].used)
        {
            g_clients[i].used = 1;
            g_clients[i].ip = ip;
            g_clients[i].port = port;
            g_clients[i].msg_count = 0;
            return i;
        }
    }

    return -1;
}

int get_or_create_client(unsigned int ip, unsigned short port)
{ //функция ищет существуюзешл клиента, елси такой есть то норм если нету то создает нового
    int idx;

    idx = find_client(ip, port);
    if (idx != -1)
        return idx;

    return create_client(ip, port);
}

int client_has_message(struct client_ctx *c, unsigned int msg_index)
{//проходим по msg_index и ищем, встречался ли уже такой msg_index, нужна для тргр чтобы не было повторных сообщений
    int i;

    for (i = 0; i < c->msg_count; i++)
    {
        if (c->msg_indices[i] == msg_index)
            return 1;
    }

    return 0;
}

int client_add_message(struct client_ctx *c, unsigned int msg_index)
{//если у клиента есть ещё место то функция добавляет номер в конец массива и увеличивает msg_count
    if (c->msg_count >= MAX_MSGS_PER_CLIENT)
        return 0;

    c->msg_indices[c->msg_count] = msg_index;
    c->msg_count++;
    return 1;
}

int build_reply_datagram(struct client_ctx *c, char *out_buf, int *out_len)
{//функция реализует ответный протокол сервера
    int start;
    int count;
    int i;
    int pos = 0;

    if (c->msg_count > MAX_REPLY_MSGS)
        start = c->msg_count - MAX_REPLY_MSGS; //определяем с какого индекса начать
    else
        start = 0;

    count = c->msg_count - start;

    for (i = 0; i < count; i++)
    {
        unsigned int idx_net = htonl(c->msg_indices[start + i]); //копируем каждый номер в ответ
        memcpy(out_buf + pos, &idx_net, 4);
        pos += 4; //считаем общую длину ответа
    }

    *out_len = pos;
    return 1;
}

int send_reply_datagram(SOCKET s, struct sockaddr_in *to, int tolen, struct client_ctx *c)
{//функция строит ответный буфер, отправляет и првоеряет что отправилась вся длина
    char reply[4 * MAX_REPLY_MSGS];
    int reply_len;
    int r;

    build_reply_datagram(c, reply, &reply_len);

    r = sendto(s, reply,reply_len,0, (struct sockaddr *)to, tolen);

    if (r != reply_len)
        return 0;

    return 1;
}

int main(int argc, char *argv[])
{
    WSADATA wsa;
    int first_port;
    int last_port;
    int count;
    int i;
    int stop_server = 0;

    if (argc != 3)
    {
        printf("Usage: udpserver <first_port> <last_port>\n");
        return 1;
    }

    char *end1, *end2;
    errno = 0;
    long first = strtol(argv[1], &end1, 10), last = strtol(argv[2], &end2, 10);
    if (!argv[1][0] || !argv[2][0] || *end1 || *end2 || errno || first < 1 || last > 65535 || first > last) {
        fprintf(stderr, "Invalid port range\n"); return 1;
    }
    first_port = (int)first;
    last_port = (int)last;

    if (first_port <= 0 || first_port > 65535 ||
        last_port <= 0 || last_port > 65535 ||
        first_port > last_port) //проверяем размер диаппазона
    {
        printf("Invalid port range\n");
        return 1;
    }

    count = last_port - first_port + 1;
    if (count > MAX_PORTS)
    {
        printf("Too many ports in range, max = %d\n", MAX_PORTS);
        return 1;
    }

    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
    {
        printf("WSAStartup() error\n");
        return 1;
    }

    for (i = 0; i < MAX_PORTS; i++)
    { //инициализируем все слоты сокетов
        g_sockets[i].s = INVALID_SOCKET;
        g_sockets[i].ev = WSA_INVALID_EVENT;
        g_sockets[i].port = 0;
    }

    for (i = 0; i < MAX_CLIENTS; i++) //все слоты клиентов
        init_client(&g_clients[i]);

    for (i = 0; i < count; i++)
    {
        unsigned short p = (unsigned short)(first_port + i);

        if (!create_udp_socket_on_port(&g_sockets[i], p)) //создаем все порты
        {
            printf("Cannot open UDP port %u\n", p);

            for (i = 0; i < count; i++)
                close_ctx(&g_sockets[i]);

            WSACleanup();
            return 1;
        }

        g_events[i] = g_sockets[i].ev; //при успехе сохраняем и печатаем
        printf("Listening UDP port: %u\n", p);
    }

    while (!stop_server)
    {
        DWORD wait_res;

        wait_res = WSAWaitForMultipleEvents(count, g_events, FALSE, 1000, FALSE);

        if (wait_res == WSA_WAIT_TIMEOUT) //обрабатываем таймаут
            continue;

        if (wait_res == WSA_WAIT_FAILED)
        { //обрабатываем ошибку
            printf("WSAWaitForMultipleEvents() error\n");
            break;
        }

        for (i = 0; i < count; i++)
        {
            WSANETWORKEVENTS ne;

            if (WSAEnumNetworkEvents(g_sockets[i].s, g_sockets[i].ev, &ne) != 0)
                continue;

            if (ne.lNetworkEvents & FD_READ) //если у сокета есть fd_read
            {
                while (1)
                {
                    char buf[BUF_SIZE];
                    char phone1[13];
                    char phone2[13];
                    char text[BUF_SIZE];

                    struct sockaddr_in from;
                    int fromlen;
                    int r;

                    unsigned int ip;
                    unsigned short port;
                    unsigned int msg_index;
                    unsigned char hh, mm, ss;

                    int client_idx;
                    int is_duplicate;

                    fromlen = sizeof(from); // то высчитываем дейтарграммы до тех пор пока не получим WSAEWOULDBLOCK
                    r = recvfrom(g_sockets[i].s, buf, sizeof(buf), 0, (struct sockaddr *)&from, &fromlen);

                    if (r == SOCKET_ERROR)
                    {
                        int err = WSAGetLastError();

                        if (err == WSAEWOULDBLOCK)
                            break;

                        printf("recvfrom() error on port %u, err=%d\n",
                               g_sockets[i].port, err);
                        break;
                    }

                    ip = ntohl(from.sin_addr.s_addr); //после recvfrom сервер получит айпишку
                    port = ntohs(from.sin_port); //порт

                    if (!parse_message(buf, r, &msg_index, phone1, phone2, &hh, &mm, &ss, text))
                    { //парсим пакет
                        printf("Bad datagram on port %u from ", g_sockets[i].port); //проверка на плохую дейтаграмму
                        print_ip(ip);
                        printf(":%u\n", port);
                        continue;
                    }

                    client_idx = get_or_create_client(ip, port); //далее ищем или создаем клиента
                    if (client_idx == -1)
                    {
                        printf("Too many clients in memory\n"); //обработка переполнения клиентской базы
                        continue;
                    }


                    is_duplicate = client_has_message(&g_clients[client_idx], msg_index);

                    if (!is_duplicate)
                    { //если пакет не дубликат то
                        if (g_clients[client_idx].msg_count >= MAX_MSGS_PER_CLIENT)
                        {//добавляем номер в память
                            printf("Client message storage overflow for ");
                            print_ip(ip);
                            printf(":%u\n", port);
                            continue;
                        }


                        if (!append_message_to_file("msg.txt", ip, port, phone1, phone2, hh, mm, ss, text))
                        {//пишем сообщение в МсГ.тхт
                            printf("Cannot write msg.txt\n");
                            continue;
                        }
                        client_add_message(&g_clients[client_idx], msg_index);
                    }

                    if (!send_reply_datagram(g_sockets[i].s, &from, fromlen, &g_clients[client_idx]))
                    {//пытаемся отправить ответную дейтаграмму
                        printf("Cannot send reply to ");
                        print_ip(ip);
                        printf(":%u\n", port);
                        continue;
                    }

                    if (strcmp(text, "stop") == 0)
                    {//обрабатываем стоп
                        printf("'stop' message arrived. Terminating...\n");
                        stop_server = 1;
                        break;
                    }
                }
            }

            if (stop_server)
                break;
        }
    }

    for (i = 0; i < count; i++)
        close_ctx(&g_sockets[i]);

    WSACleanup();
    return 0;
}
