#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include "protocol.h"
#include "system_info.h"

typedef enum io_operation {
    IO_OPERATION_RECV,
    IO_OPERATION_SEND
} io_operation;

typedef enum receive_stage {
    RECEIVE_HEADER,
    RECEIVE_PAYLOAD
} receive_stage;

typedef struct connection {
    SOCKET socket;
    OVERLAPPED overlapped;
    WSABUF wsabuf;
    io_operation operation;
    receive_stage receive_stage;
    uint8_t header_bytes[LAB_HEADER_SIZE];
    size_t header_received;
    lab_header request;
    uint8_t payload[LAB_MAX_PAYLOAD + 1u];
    size_t payload_received;
    uint8_t send_buffer[LAB_HEADER_SIZE + LAB_MAX_PAYLOAD];
    size_t send_length;
    size_t send_completed;
    char peer[64];
} connection;

static HANDLE completion_port = NULL;

static void close_connection(connection *client, const char *reason)
{
    if (client == NULL) {
        return;
    }

    printf("[disconnect] peer=%s reason=%s\n", client->peer, reason);
    closesocket(client->socket);
    free(client);
}

static int post_receive(connection *client)
{
    DWORD flags = 0;
    int result;

    ZeroMemory(&client->overlapped, sizeof(client->overlapped));
    client->operation = IO_OPERATION_RECV;

    if (client->receive_stage == RECEIVE_HEADER) {
        client->wsabuf.buf = (CHAR *)client->header_bytes + client->header_received;
        client->wsabuf.len = (ULONG)(LAB_HEADER_SIZE - client->header_received);
    } else {
        client->wsabuf.buf = (CHAR *)client->payload + client->payload_received;
        client->wsabuf.len = (ULONG)(client->request.payload_len
                                     - client->payload_received);
    }

    result = WSARecv(client->socket,
                     &client->wsabuf,
                     1,
                     NULL,
                     &flags,
                     &client->overlapped,
                     NULL);
    if (result == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING) {
        return 0;
    }
    return 1;
}

static int post_send(connection *client)
{
    int result;

    ZeroMemory(&client->overlapped, sizeof(client->overlapped));
    client->operation = IO_OPERATION_SEND;
    client->wsabuf.buf = (CHAR *)client->send_buffer + client->send_completed;
    client->wsabuf.len = (ULONG)(client->send_length - client->send_completed);

    result = WSASend(client->socket,
                     &client->wsabuf,
                     1,
                     NULL,
                     0,
                     &client->overlapped,
                     NULL);
    if (result == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING) {
        return 0;
    }
    return 1;
}

static int prepare_response(connection *client)
{
    lab_header response;
    char response_text[LAB_MAX_PAYLOAD];
    size_t response_length;
    int is_error;

    lab_build_system_response(client->request.type,
                              response_text,
                              sizeof(response_text),
                              &is_error);
    response_length = strlen(response_text);

    response.version = LAB_PROTOCOL_VERSION;
    response.kind = (uint8_t)(is_error ? LAB_KIND_ERROR : LAB_KIND_RESPONSE);
    response.type = client->request.type;
    response.request_id = client->request.request_id;
    response.flags = 0;
    response.payload_len = (uint32_t)response_length;
    response.reserved = 0;

    lab_encode_header(client->send_buffer, &response);
    memcpy(client->send_buffer + LAB_HEADER_SIZE,
           response_text,
           response_length);
    client->send_length = LAB_HEADER_SIZE + response_length;
    client->send_completed = 0;

    printf("[request] peer=%s id=%lu type=%s payload=%lu\n",
           client->peer,
           (unsigned long)client->request.request_id,
           lab_request_name(client->request.type),
           (unsigned long)client->request.payload_len);

    return post_send(client);
}

static int complete_receive(connection *client, DWORD transferred)
{
    if (client->receive_stage == RECEIVE_HEADER) {
        client->header_received += transferred;
        if (client->header_received < LAB_HEADER_SIZE) {
            return post_receive(client);
        }

        if (!lab_decode_header(&client->request, client->header_bytes)) {
            return 0;
        }
        if (client->request.version != LAB_PROTOCOL_VERSION
            || client->request.kind != LAB_KIND_REQUEST
            || client->request.payload_len > LAB_MAX_PAYLOAD
            || client->request.reserved != 0) {
            return 0;
        }

        client->payload_received = 0;
        if (client->request.payload_len == 0) {
            return prepare_response(client);
        }

        client->receive_stage = RECEIVE_PAYLOAD;
        return post_receive(client);
    }

    client->payload_received += transferred;
    if (client->payload_received < client->request.payload_len) {
        return post_receive(client);
    }

    client->payload[client->request.payload_len] = '\0';
    return prepare_response(client);
}

static int complete_send(connection *client, DWORD transferred)
{
    client->send_completed += transferred;
    if (client->send_completed < client->send_length) {
        return post_send(client);
    }

    client->receive_stage = RECEIVE_HEADER;
    client->header_received = 0;
    client->payload_received = 0;
    ZeroMemory(&client->request, sizeof(client->request));
    return post_receive(client);
}

static DWORD WINAPI worker_thread(void *parameter)
{
    (void)parameter;

    for (;;) {
        DWORD transferred = 0;
        ULONG_PTR completion_key = 0;
        OVERLAPPED *overlapped = NULL;
        BOOL success;
        connection *client;

        success = GetQueuedCompletionStatus(completion_port,
                                            &transferred,
                                            &completion_key,
                                            &overlapped,
                                            INFINITE);
        client = (connection *)completion_key;

        if (client == NULL) {
            continue;
        }
        if (!success) {
            close_connection(client, "asynchronous I/O error");
            continue;
        }
        if (transferred == 0) {
            close_connection(client, "remote side closed the connection");
            continue;
        }

        if (client->operation == IO_OPERATION_RECV) {
            if (!complete_receive(client, transferred)) {
                close_connection(client, "invalid request or receive error");
            }
        } else {
            if (!complete_send(client, transferred)) {
                close_connection(client, "send error");
            }
        }
    }

    return 0;
}

static SOCKET create_listen_socket(const char *port)
{
    struct addrinfo hints;
    struct addrinfo *addresses = NULL;
    struct addrinfo *current;
    SOCKET listen_socket = INVALID_SOCKET;
    BOOL exclusive_address = TRUE;

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    if (getaddrinfo(NULL, port, &hints, &addresses) != 0) {
        return INVALID_SOCKET;
    }

    for (current = addresses; current != NULL; current = current->ai_next) {
        listen_socket = WSASocketW(current->ai_family,
                                   current->ai_socktype,
                                   current->ai_protocol,
                                   NULL,
                                   0,
                                   WSA_FLAG_OVERLAPPED);
        if (listen_socket == INVALID_SOCKET) {
            continue;
        }

        if (setsockopt(listen_socket,
                       SOL_SOCKET,
                       SO_EXCLUSIVEADDRUSE,
                       (const char *)&exclusive_address,
                       sizeof(exclusive_address)) == SOCKET_ERROR) {
            closesocket(listen_socket);
            listen_socket = INVALID_SOCKET;
            continue;
        }

        if (bind(listen_socket,
                 current->ai_addr,
                 (int)current->ai_addrlen) == 0
            && listen(listen_socket, SOMAXCONN) == 0) {
            break;
        }

        closesocket(listen_socket);
        listen_socket = INVALID_SOCKET;
    }

    freeaddrinfo(addresses);
    return listen_socket;
}

static void set_peer_name(connection *client,
                          const struct sockaddr_in *address)
{
    char ip[INET_ADDRSTRLEN] = "unknown";

    inet_ntop(AF_INET, &address->sin_addr, ip, sizeof(ip));
    snprintf(client->peer,
             sizeof(client->peer),
             "%s:%u",
             ip,
             (unsigned int)ntohs(address->sin_port));
}

int main(int argc, char **argv)
{
    const char *port = argc >= 2 ? argv[1] : LAB_DEFAULT_PORT;
    WSADATA wsa_data;
    SOCKET listen_socket;
    SYSTEM_INFO system_info;
    HANDLE *workers;
    DWORD worker_count;
    DWORD i;

    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        fprintf(stderr, "WSAStartup failed.\n");
        return 1;
    }

    completion_port = CreateIoCompletionPort(INVALID_HANDLE_VALUE,
                                             NULL,
                                             0,
                                             0);
    if (completion_port == NULL) {
        fprintf(stderr, "CreateIoCompletionPort failed: %lu\n",
                (unsigned long)GetLastError());
        WSACleanup();
        return 1;
    }

    GetSystemInfo(&system_info);
    worker_count = system_info.dwNumberOfProcessors;
    if (worker_count < 2) {
        worker_count = 2;
    }

    workers = (HANDLE *)calloc(worker_count, sizeof(*workers));
    if (workers == NULL) {
        fprintf(stderr, "Cannot allocate worker handle array.\n");
        CloseHandle(completion_port);
        WSACleanup();
        return 1;
    }

    for (i = 0; i < worker_count; ++i) {
        workers[i] = CreateThread(NULL, 0, worker_thread, NULL, 0, NULL);
        if (workers[i] == NULL) {
            fprintf(stderr, "CreateThread failed: %lu\n",
                    (unsigned long)GetLastError());
            free(workers);
            CloseHandle(completion_port);
            WSACleanup();
            return 1;
        }
    }

    listen_socket = create_listen_socket(port);
    if (listen_socket == INVALID_SOCKET) {
        fprintf(stderr, "Cannot listen on TCP port %s: %d\n",
                port,
                WSAGetLastError());
        free(workers);
        CloseHandle(completion_port);
        WSACleanup();
        return 1;
    }

    printf("BSIT Lab 1 IOCP server\n");
    printf("Listening on 0.0.0.0:%s\n", port);
    printf("Worker threads: %lu\n", (unsigned long)worker_count);
    printf("Stop the server with Ctrl+C.\n\n");

    for (;;) {
        struct sockaddr_in peer_address;
        int peer_length = sizeof(peer_address);
        SOCKET client_socket;
        connection *client;

        client_socket = accept(listen_socket,
                               (struct sockaddr *)&peer_address,
                               &peer_length);
        if (client_socket == INVALID_SOCKET) {
            fprintf(stderr, "accept failed: %d\n", WSAGetLastError());
            continue;
        }

        client = (connection *)calloc(1, sizeof(*client));
        if (client == NULL) {
            closesocket(client_socket);
            continue;
        }

        client->socket = client_socket;
        client->receive_stage = RECEIVE_HEADER;
        set_peer_name(client, &peer_address);

        if (CreateIoCompletionPort((HANDLE)client_socket,
                                   completion_port,
                                   (ULONG_PTR)client,
                                   0) == NULL) {
            close_connection(client, "cannot associate socket with IOCP");
            continue;
        }

        printf("[connect] peer=%s\n", client->peer);
        if (!post_receive(client)) {
            close_connection(client, "cannot start WSARecv");
        }
    }
}
