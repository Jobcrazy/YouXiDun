#define _WIN32_WINNT 0x0601
#define WIN32_LEAN_AND_MEAN

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include "tcp_client.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TCP_MAX_FRAME_SIZE (16u * 1024u)
#define TCP_RECEIVE_BUFFER_SIZE (64u * 1024u)
#define TCP_CONNECT_TIMEOUT_MS 5000u
#define TCP_POLL_INTERVAL_MS 100u

struct TcpClient {
    HANDLE thread;
    volatile LONG stopping;
    char host[256];
    unsigned short port;
    TcpEventCallback callback;
    void *context;
};

static void emit_event(TcpClient *client, TcpEventType type, const char *message) {
    if (client->callback) client->callback(type, message ? message : "", client->context);
}

static void socket_error_text(char *buffer, size_t size, const char *prefix) {
    snprintf(buffer, size, "%s (Winsock error %d)", prefix, WSAGetLastError());
}

static int wait_for_connection(TcpClient *client, SOCKET socket) {
    DWORD started = GetTickCount();
    for (;;) {
        if (InterlockedCompareExchange(&client->stopping, 0, 0)) return 0;
        if (GetTickCount() - started >= TCP_CONNECT_TIMEOUT_MS) {
            WSASetLastError(WSAETIMEDOUT);
            return 0;
        }

        fd_set write_set;
        fd_set error_set;
        FD_ZERO(&write_set);
        FD_ZERO(&error_set);
        FD_SET(socket, &write_set);
        FD_SET(socket, &error_set);
        struct timeval timeout = {0, TCP_POLL_INTERVAL_MS * 1000};
        int result = select(0, NULL, &write_set, &error_set, &timeout);
        if (result == SOCKET_ERROR) return 0;
        if (result == 0) continue;

        int error = 0;
        int length = sizeof(error);
        if (getsockopt(socket, SOL_SOCKET, SO_ERROR, (char *)&error, &length) == SOCKET_ERROR) return 0;
        if (error != 0) {
            WSASetLastError(error);
            return 0;
        }
        return FD_ISSET(socket, &write_set) != 0;
    }
}

static SOCKET connect_socket(TcpClient *client) {
    char port_text[8];
    snprintf(port_text, sizeof(port_text), "%u", (unsigned)client->port);
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    struct addrinfo *addresses = NULL;
    int lookup = getaddrinfo(client->host, port_text, &hints, &addresses);
    if (lookup != 0) {
        WSASetLastError(lookup);
        return INVALID_SOCKET;
    }

    SOCKET connected = INVALID_SOCKET;
    for (struct addrinfo *address = addresses; address; address = address->ai_next) {
        if (InterlockedCompareExchange(&client->stopping, 0, 0)) break;
        SOCKET candidate = socket(address->ai_family, address->ai_socktype, address->ai_protocol);
        if (candidate == INVALID_SOCKET) continue;
        u_long nonblocking = 1;
        if (ioctlsocket(candidate, FIONBIO, &nonblocking) == SOCKET_ERROR) {
            closesocket(candidate);
            continue;
        }
        int result = connect(candidate, address->ai_addr, (int)address->ai_addrlen);
        if (result == 0 || (WSAGetLastError() == WSAEWOULDBLOCK && wait_for_connection(client, candidate))) {
            connected = candidate;
            break;
        }
        closesocket(candidate);
    }
    freeaddrinfo(addresses);
    return connected;
}

static int send_all(TcpClient *client, SOCKET socket, const unsigned char *data, size_t length) {
    size_t offset = 0;
    while (offset < length) {
        if (InterlockedCompareExchange(&client->stopping, 0, 0)) return 0;
        fd_set write_set;
        FD_ZERO(&write_set);
        FD_SET(socket, &write_set);
        struct timeval timeout = {0, TCP_POLL_INTERVAL_MS * 1000};
        int ready = select(0, NULL, &write_set, NULL, &timeout);
        if (ready == SOCKET_ERROR) return 0;
        if (ready == 0) continue;
        int sent = send(socket, (const char *)data + offset, (int)(length - offset), 0);
        if (sent == SOCKET_ERROR) {
            if (WSAGetLastError() == WSAEWOULDBLOCK) continue;
            return 0;
        }
        if (sent == 0) return 0;
        offset += (size_t)sent;
    }
    return 1;
}

static int send_ping(TcpClient *client, SOCKET socket, uint64_t sequence) {
    SYSTEMTIME now;
    GetLocalTime(&now);
    char json[256];
    int payload_length = snprintf(json, sizeof(json),
        "{\"cmd\":\"ping\",\"id\":%lu,\"time\":\"%02u:%02u:%02u.%03u\"}",
        (unsigned long)sequence, now.wHour, now.wMinute, now.wSecond, now.wMilliseconds);
    if (payload_length <= 0 || (size_t)payload_length + 4u > TCP_MAX_FRAME_SIZE) return 0;

    unsigned char frame[260];
    uint32_t length = (uint32_t)payload_length;
    frame[0] = (unsigned char)(length & 0xffu);
    frame[1] = (unsigned char)((length >> 8) & 0xffu);
    frame[2] = (unsigned char)((length >> 16) & 0xffu);
    frame[3] = (unsigned char)((length >> 24) & 0xffu);
    memcpy(frame + 4, json, (size_t)payload_length);
    if (!send_all(client, socket, frame, (size_t)payload_length + 4u)) return 0;
    emit_event(client, TCP_EVENT_SENT, json);
    return 1;
}

static int process_frames(TcpClient *client, unsigned char *buffer, size_t *used) {
    size_t offset = 0;
    while (*used - offset >= 4u) {
        const unsigned char *header = buffer + offset;
        uint32_t length = (uint32_t)header[0]
                        | ((uint32_t)header[1] << 8)
                        | ((uint32_t)header[2] << 16)
                        | ((uint32_t)header[3] << 24);
        if (length == 0 || (size_t)length + 4u > TCP_MAX_FRAME_SIZE) {
            char error[96];
            snprintf(error, sizeof(error), "Invalid packet length: %lu", (unsigned long)length);
            emit_event(client, TCP_EVENT_ERROR, error);
            return 0;
        }
        size_t frame_length = (size_t)length + 4u;
        if (*used - offset < frame_length) break;
        char *message = (char *)malloc((size_t)length + 1u);
        if (!message) {
            emit_event(client, TCP_EVENT_ERROR, "Out of memory while receiving data");
            return 0;
        }
        memcpy(message, buffer + offset + 4u, length);
        message[length] = '\0';
        emit_event(client, TCP_EVENT_RECEIVED, message);
        free(message);
        offset += frame_length;
    }
    if (offset > 0) {
        memmove(buffer, buffer + offset, *used - offset);
        *used -= offset;
    }
    return 1;
}

static DWORD WINAPI tcp_worker(LPVOID parameter) {
    TcpClient *client = (TcpClient *)parameter;
    SOCKET socket = connect_socket(client);
    if (socket == INVALID_SOCKET) {
        if (!InterlockedCompareExchange(&client->stopping, 0, 0)) {
            char error[128];
            socket_error_text(error, sizeof(error), "Connection failed");
            emit_event(client, TCP_EVENT_ERROR, error);
        }
        return 0;
    }

    emit_event(client, TCP_EVENT_CONNECTED, "");
    unsigned char receive_buffer[TCP_RECEIVE_BUFFER_SIZE];
    size_t received = 0;
    uint64_t sequence = 0;
    DWORD next_ping = GetTickCount();
    int terminal_event_sent = 0;

    while (!InterlockedCompareExchange(&client->stopping, 0, 0)) {
        DWORD now = GetTickCount();
        if ((LONG)(now - next_ping) >= 0) {
            if (!send_ping(client, socket, sequence++)) {
                if (!InterlockedCompareExchange(&client->stopping, 0, 0)) {
                    char error[128];
                    socket_error_text(error, sizeof(error), "Send failed");
                    emit_event(client, TCP_EVENT_ERROR, error);
                    terminal_event_sent = 1;
                }
                break;
            }
            next_ping = now + 1000u;
        }

        fd_set read_set;
        FD_ZERO(&read_set);
        FD_SET(socket, &read_set);
        struct timeval timeout = {0, TCP_POLL_INTERVAL_MS * 1000};
        int ready = select(0, &read_set, NULL, NULL, &timeout);
        if (ready == SOCKET_ERROR) break;
        if (ready == 0) continue;
        if (received == sizeof(receive_buffer)) {
            emit_event(client, TCP_EVENT_ERROR, "Receive buffer overflow");
            terminal_event_sent = 1;
            break;
        }
        int count = recv(socket, (char *)receive_buffer + received,
                         (int)(sizeof(receive_buffer) - received), 0);
        if (count == 0) break;
        if (count == SOCKET_ERROR) {
            if (WSAGetLastError() == WSAEWOULDBLOCK) continue;
            break;
        }
        received += (size_t)count;
        if (!process_frames(client, receive_buffer, &received)) {
            terminal_event_sent = 1;
            break;
        }
    }

    shutdown(socket, SD_BOTH);
    closesocket(socket);
    if (!terminal_event_sent && !InterlockedCompareExchange(&client->stopping, 0, 0)) {
        emit_event(client, TCP_EVENT_DISCONNECTED, "Connection closed");
    }
    return 0;
}

TcpClient *tcp_client_create(TcpEventCallback callback, void *context) {
    TcpClient *client = (TcpClient *)calloc(1, sizeof(*client));
    if (!client) return NULL;
    client->callback = callback;
    client->context = context;
    return client;
}

int tcp_client_start(TcpClient *client, const char *host, unsigned short port) {
    if (!client || !host || !host[0] || client->thread) return 0;
    snprintf(client->host, sizeof(client->host), "%s", host);
    client->port = port;
    InterlockedExchange(&client->stopping, 0);
    client->thread = CreateThread(NULL, 0, tcp_worker, client, 0, NULL);
    return client->thread != NULL;
}

void tcp_client_stop(TcpClient *client) {
    if (!client || !client->thread) return;
    InterlockedExchange(&client->stopping, 1);
    WaitForSingleObject(client->thread, INFINITE);
    CloseHandle(client->thread);
    client->thread = NULL;
}

void tcp_client_destroy(TcpClient *client) {
    if (!client) return;
    tcp_client_stop(client);
    free(client);
}
