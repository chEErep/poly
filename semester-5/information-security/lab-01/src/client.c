#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include "protocol.h"

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

static SOCKET connect_to_server(const char *host, const char *port)
{
    struct addrinfo hints;
    struct addrinfo *addresses = NULL;
    struct addrinfo *current;
    SOCKET socket_value = INVALID_SOCKET;

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    if (getaddrinfo(host, port, &hints, &addresses) != 0) {
        return INVALID_SOCKET;
    }

    for (current = addresses; current != NULL; current = current->ai_next) {
        socket_value = socket(current->ai_family,
                              current->ai_socktype,
                              current->ai_protocol);
        if (socket_value == INVALID_SOCKET) {
            continue;
        }
        if (connect(socket_value,
                    current->ai_addr,
                    (int)current->ai_addrlen) == 0) {
            break;
        }
        closesocket(socket_value);
        socket_value = INVALID_SOCKET;
    }

    freeaddrinfo(addresses);
    return socket_value;
}

static int perform_request(SOCKET socket_value,
                           uint16_t request_type,
                           uint32_t request_id)
{
    uint8_t header_bytes[LAB_HEADER_SIZE];
    lab_header request;
    lab_header response;
    uint8_t *payload;

    request.version = LAB_PROTOCOL_VERSION;
    request.kind = LAB_KIND_REQUEST;
    request.type = request_type;
    request.request_id = request_id;
    request.flags = 0;
    request.payload_len = 0;
    request.reserved = 0;
    lab_encode_header(header_bytes, &request);

    if (!send_all(socket_value, header_bytes, sizeof(header_bytes))) {
        fprintf(stderr, "Cannot send request: %d\n", WSAGetLastError());
        return 0;
    }
    if (!receive_all(socket_value, header_bytes, sizeof(header_bytes))) {
        fprintf(stderr, "Cannot receive response header: %d\n",
                WSAGetLastError());
        return 0;
    }
    if (!lab_decode_header(&response, header_bytes)
        || response.version != LAB_PROTOCOL_VERSION
        || (response.kind != LAB_KIND_RESPONSE
            && response.kind != LAB_KIND_ERROR)
        || response.type != request_type
        || response.request_id != request_id
        || response.payload_len > LAB_MAX_PAYLOAD
        || response.reserved != 0) {
        fprintf(stderr, "The server returned an invalid protocol header.\n");
        return 0;
    }

    payload = (uint8_t *)malloc((size_t)response.payload_len + 1u);
    if (payload == NULL) {
        fprintf(stderr, "Cannot allocate response buffer.\n");
        return 0;
    }

    if (!receive_all(socket_value, payload, response.payload_len)) {
        fprintf(stderr, "Cannot receive response payload: %d\n",
                WSAGetLastError());
        free(payload);
        return 0;
    }
    payload[response.payload_len] = '\0';

    printf("\n--- %s response, request_id=%lu ---\n%s",
           lab_request_name(request_type),
           (unsigned long)request_id,
           (const char *)payload);
    if (response.payload_len == 0
        || payload[response.payload_len - 1] != '\n') {
        putchar('\n');
    }
    putchar('\n');

    free(payload);
    return 1;
}

static void print_menu(void)
{
    printf("Choose request:\n");
    printf("  1 - operating system\n");
    printf("  2 - current time\n");
    printf("  3 - system uptime\n");
    printf("  4 - memory information\n");
    printf("  0 - disconnect and exit\n");
    printf("> ");
}

int main(int argc, char **argv)
{
    const char *host = argc >= 2 ? argv[1] : "127.0.0.1";
    const char *port = argc >= 3 ? argv[2] : LAB_DEFAULT_PORT;
    WSADATA wsa_data;
    SOCKET socket_value;
    char input[64];
    uint32_t request_id = 1;

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

    for (;;) {
        char *end;
        long choice;

        print_menu();
        if (fgets(input, sizeof(input), stdin) == NULL) {
            break;
        }

        choice = strtol(input, &end, 10);
        if (end == input) {
            printf("Please enter a number from 0 to 4.\n\n");
            continue;
        }
        if (choice == 0) {
            break;
        }
        if (choice < LAB_GET_OS || choice > LAB_GET_MEMORY) {
            printf("Unknown menu item.\n\n");
            continue;
        }

        if (!perform_request(socket_value,
                             (uint16_t)choice,
                             request_id++)) {
            closesocket(socket_value);
            WSACleanup();
            return 1;
        }
    }

    shutdown(socket_value, SD_BOTH);
    closesocket(socket_value);
    WSACleanup();
    printf("Disconnected.\n");
    return 0;
}
