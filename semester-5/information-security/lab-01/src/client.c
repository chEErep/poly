#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include "protocol.h"

#include "crypto.h"

// TCP -- поток байтов: один send() не обязан отправить всё сообщение
// Поэтому повторяем вызов, пока не отправим length байтов
static int send_all(SOCKET socket, const uint8_t *data, size_t length)
{
    size_t sent = 0;

    while (sent < length) {
        int result = send(socket,
                          (const char *)data + sent,
                          (int)(length - sent),
                          0);
        if (result == SOCKET_ERROR || result == 0) {
            return 0;
        }
        sent += (size_t)result;
    }
    return 1;
}

// Аналогично recv() может вернуть только часть заголовка или тела ответа
static int receive_all(SOCKET socket, uint8_t *data, size_t length)
{
    size_t received = 0;

    while (received < length) {
        int result = recv(socket,
                          (char *)data + received,
                          (int)(length - received),
                          0);
        if (result == SOCKET_ERROR || result == 0) {
            return 0;
        }
        received += (size_t)result;
    }
    return 1;
}

// Преобразуем адрес/имя в sockaddr, пробуем подключиться и возвращаем SOCKET
// INVALID_SOCKET означает, что ни один адрес не подошёл
static SOCKET connect_to_server(const char *host, const char *port)
{
    struct addrinfo hints;
    struct addrinfo *addresses = NULL;
    struct addrinfo *current;
    SOCKET socket_value = INVALID_SOCKET;
    int last_error = WSAECONNREFUSED;
    int lookup_error;

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    lookup_error = getaddrinfo(host, port, &hints, &addresses);
    if (lookup_error != 0) {
        WSASetLastError(lookup_error);
        return INVALID_SOCKET;
    }

    for (current = addresses; current != NULL; current = current->ai_next) {
        socket_value = socket(current->ai_family,
                              current->ai_socktype,
                              current->ai_protocol);
        if (socket_value == INVALID_SOCKET) {
            last_error = WSAGetLastError();
            continue;
        }
        if (connect(socket_value,
                    current->ai_addr,
                    (int)current->ai_addrlen) == 0) {
            break;
        }
        last_error = WSAGetLastError();
        closesocket(socket_value);
        socket_value = INVALID_SOCKET;
    }

    freeaddrinfo(addresses);
    if (socket_value == INVALID_SOCKET) {
        WSASetLastError(last_error);
    }
    return socket_value;
}
static int perform_key_exchange(SOCKET socket_value,
                                lab_crypto_context *crypto)
{
    uint8_t header_bytes[LAB_HEADER_SIZE];
    BYTE public_blob[LAB_MAX_KEY_BLOB];
    BYTE wrapped_blob[LAB_MAX_KEY_BLOB];
    DWORD public_length = sizeof(public_blob);
    lab_header request;
    lab_header response;

    // Клиент создаёт RSA-пару и получает её публичную часть
    if (!lab_crypto_open(crypto)
        || !lab_crypto_generate_exchange_key(crypto)
        || !lab_crypto_export_public_key(crypto,
                                         public_blob,
                                         &public_length)) {
        fprintf(stderr, "Cannot create public key: %lu\n",
                (unsigned long)GetLastError());
        return 0;
    }

    request.version = LAB_PROTOCOL_VERSION;
    request.kind = LAB_KIND_REQUEST;
    request.type = LAB_KEY_EXCHANGE;
    request.request_id = 0;
    request.flags = 0;
    request.payload_len = public_length;
    request.reserved = 0;
    lab_encode_header(header_bytes, &request);

    // Отправляем сначала заголовок, затем ровно public_length байтов
    if (!send_all(socket_value, header_bytes, sizeof(header_bytes))
        || !send_all(socket_value, public_blob, public_length)) {
        fprintf(stderr, "Cannot send public key.\n");
        return 0;
    }

    if (!receive_all(socket_value, header_bytes, sizeof(header_bytes))) {
        fprintf(stderr, "Cannot receive key exchange header.\n");
        return 0;
    }

    if (!lab_decode_header(&response, header_bytes)
        || response.version != LAB_PROTOCOL_VERSION
        || response.kind != LAB_KIND_RESPONSE
        || response.type != LAB_KEY_EXCHANGE
        || response.request_id != 0
        || response.flags != 0
        || response.reserved != 0
        || response.payload_len == 0
        || response.payload_len > sizeof(wrapped_blob)) {
        fprintf(stderr, "Invalid key exchange response header.\n");
        return 0;
    }

    if (!receive_all(socket_value,
                     wrapped_blob,
                     response.payload_len)) {
        fprintf(stderr, "Cannot receive wrapped session key.\n");
        return 0;
    }

    // Раскрываем AES-ключ приватным RSA-ключом клиента
    if (!lab_crypto_accept_session_key(crypto,
                                       wrapped_blob,
                                       response.payload_len)) {
        fprintf(stderr, "Cannot import session key: %lu\n",
                (unsigned long)GetLastError());
        return 0;
    }

    printf("Key exchange complete: public=%lu bytes, wrapped=%lu bytes.\n\n",
           (unsigned long)public_length,
           (unsigned long)response.payload_len);
    return 1;
}

// Один полный обмен: собрать запрос -> отправить -> прочитать заголовок
// ответа -> проверить его -> прочитать и напечатать тело
static int perform_request(SOCKET socket_value,
                           lab_crypto_context *crypto,
                           uint16_t request_type,
                           uint32_t request_id,
                           const char *request_path)
{
    uint8_t inner_bytes[LAB_HEADER_SIZE + 512u];
    uint8_t encrypted[LAB_MAX_PAYLOAD];
    uint8_t plain_response[LAB_MAX_PAYLOAD];
    uint8_t header_bytes[LAB_HEADER_SIZE];
    lab_header inner_request;
    lab_header outer_request;
    lab_header outer_response;
    lab_header inner_response;
    DWORD encrypted_length = 0;
    DWORD plain_length = 0;
    size_t path_length = request_path != NULL ? strlen(request_path) : 0;

    if (path_length > 512u) {
        fprintf(stderr, "Request path is too long.\n");
        return 0;
    }

    // Внутренний запрос: привычный заголовок GET_OS/GET_ACL + путь
    inner_request.version = LAB_PROTOCOL_VERSION;
    inner_request.kind = LAB_KIND_REQUEST;
    inner_request.type = request_type;
    inner_request.request_id = request_id;
    inner_request.flags = 0;
    inner_request.payload_len = (uint32_t)path_length;
    inner_request.reserved = 0;

    lab_encode_header(inner_bytes, &inner_request);
    if (path_length > 0) {
        memcpy(inner_bytes + LAB_HEADER_SIZE, request_path, path_length);
    }

    if (!lab_crypto_encrypt_message(
            crypto,
            inner_bytes,
            (DWORD)(LAB_HEADER_SIZE + path_length),
            encrypted,
            sizeof(encrypted),
            &encrypted_length)) {
        fprintf(stderr, "Cannot encrypt request: %lu\n",
                (unsigned long)GetLastError());
        return 0;
    }

    // Внешний заголовок открыт: видны тип кадра, request_id и длина
    outer_request.version = LAB_PROTOCOL_VERSION;
    outer_request.kind = LAB_KIND_REQUEST;
    outer_request.type = LAB_SECURE_FRAME;
    outer_request.request_id = request_id;
    outer_request.flags = 0;
    outer_request.payload_len = encrypted_length;
    outer_request.reserved = 0;

    lab_encode_header(header_bytes, &outer_request);
    if (!send_all(socket_value, header_bytes, sizeof(header_bytes))
        || !send_all(socket_value, encrypted, encrypted_length)) {
        fprintf(stderr, "Cannot send encrypted request.\n");
        return 0;
    }

    if (!receive_all(socket_value, header_bytes, sizeof(header_bytes))) {
        fprintf(stderr, "Cannot receive encrypted response header.\n");
        return 0;
    }

    if (!lab_decode_header(&outer_response, header_bytes)
        || outer_response.version != LAB_PROTOCOL_VERSION
        || outer_response.kind != LAB_KIND_RESPONSE
        || outer_response.type != LAB_SECURE_FRAME
        || outer_response.request_id != request_id
        || outer_response.flags != 0
        || outer_response.reserved != 0
        || outer_response.payload_len < 2u * LAB_AES_IV_BYTES
        || outer_response.payload_len > LAB_MAX_PAYLOAD) {
        fprintf(stderr, "Invalid encrypted response header.\n");
        return 0;
    }

    if (!receive_all(socket_value,
                     encrypted,
                     outer_response.payload_len)) {
        fprintf(stderr, "Cannot receive encrypted response body.\n");
        return 0;
    }

    if (!lab_crypto_decrypt_message(
            crypto,
            encrypted,
            outer_response.payload_len,
            plain_response,
            sizeof(plain_response),
            &plain_length)) {
        fprintf(stderr, "Cannot decrypt response: %lu\n",
                (unsigned long)GetLastError());
        return 0;
    }

    if (plain_length < LAB_HEADER_SIZE
        || !lab_decode_header(&inner_response, plain_response)
        || inner_response.version != LAB_PROTOCOL_VERSION
        || (inner_response.kind != LAB_KIND_RESPONSE
            && inner_response.kind != LAB_KIND_ERROR)
        || inner_response.type != request_type
        || inner_response.request_id != request_id
        || inner_response.flags != 0
        || inner_response.reserved != 0
        || inner_response.payload_len != plain_length - LAB_HEADER_SIZE) {
        fprintf(stderr, "Invalid decrypted response.\n");
        return 0;
    }

    printf("\n--- %s response, request_id=%lu ---\n",
           lab_request_name(request_type),
           (unsigned long)request_id);

    // Тело — текст, но длину всё равно берём из заголовка, не из strlen()
    fwrite(plain_response + LAB_HEADER_SIZE,
           1, inner_response.payload_len, stdout);
    if (inner_response.payload_len == 0
        || plain_response[LAB_HEADER_SIZE
                          + inner_response.payload_len - 1] != '\n') {
        putchar('\n');
    }
    putchar('\n');

    return 1;
}

// Пока пользователь выбирает запрос из меню, адрес задаётся при запуске
static void print_menu(void)
{
    printf("Choose request:\n");
    printf("  1 - operating system\n");
    printf("  2 - current time\n");
    printf("  3 - system uptime\n");
    printf("  4 - memory information\n");
    printf("  5 - connected drives\n");
    printf("  6 - free space on local/removable drives\n");
    printf("  7 - owner of a file, folder or registry key\n");
    printf("  8 - access rights of a file, folder or registry key\n");
    printf("  0 - disconnect and exit\n");
    printf("> ");
}

// Инициализация WinSock -> TCP-подключение -> цикл меню -> освобождение
// сокета и ресурсов WinSock. Несколько запросов идут по одному соединению
int main(int argc, char **argv)
{
    const char *host = argc >= 2 ? argv[1] : "127.0.0.1";
    const char *port = argc >= 3 ? argv[2] : LAB_DEFAULT_PORT;
    WSADATA wsa_data;
    SOCKET socket_value;
    lab_crypto_context crypto = {0};
    char input[64];
    char path[512];
    uint32_t request_id = 1;

    if (!SetConsoleOutputCP(CP_UTF8)) {
        fprintf(stderr, "Warning: cannot enable UTF-8 console output (%lu).\n",
                (unsigned long)GetLastError());
    }

    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        fprintf(stderr, "WSAStartup failed.\n");
        return 1;
    }

    printf("Connecting to %s:%s...\n", host, port);
    socket_value = connect_to_server(host, port);
    if (socket_value == INVALID_SOCKET) {
        fprintf(stderr, "Cannot connect: %d\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }
    printf("Connected.\n\n");
    if (!perform_key_exchange(socket_value, &crypto)) {
        lab_crypto_close(&crypto);
        closesocket(socket_value);
        WSACleanup();
        return 1;
    }
    for (;;) {
        char *end;
        long choice;
        const char *request_path = NULL;
        print_menu();
        if (fgets(input, sizeof(input), stdin) == NULL) {
            break;
        }

        choice = strtol(input, &end, 10);
        if (end == input) {
            printf("Please enter a number from 0 to 8.\n\n");
            continue;
        }
        if (choice == 0) {
            break;
        }
        if (choice < LAB_GET_OS || choice > LAB_GET_ACL) {
            printf("Unknown menu item.\n\n");
            continue;
        }
        if (choice == LAB_GET_OWNER || choice == LAB_GET_ACL) {
            size_t length;
            int ch;

            printf("Path on server: ");
            if (fgets(path, sizeof(path), stdin) == NULL) {
                break;
            }

            length = strcspn(path, "\r\n");
            if (path[length] == '\0') {
                while ((ch = getchar()) != '\n' && ch != EOF) {
                    // Убираем остаток слишком длинной строки из ввода
                }
                printf("Path is too long.\n\n");
                continue;
            }

            path[length] = '\0';
            if (length == 0) {
                printf("Path cannot be empty.\n\n");
                continue;
            }

            request_path = path;
        }
        if (!perform_request(socket_value,
                             &crypto,
                             (uint16_t)choice,
                             request_id++,
                             request_path)) {
            lab_crypto_close(&crypto);
            closesocket(socket_value);
            WSACleanup();
            return 1;
        }
    }
    lab_crypto_close(&crypto);
    shutdown(socket_value, SD_BOTH);
    closesocket(socket_value);
    WSACleanup();
    printf("Disconnected.\n");
    return 0;
}
