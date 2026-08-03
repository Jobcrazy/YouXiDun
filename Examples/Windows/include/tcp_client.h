#ifndef WINDEMO_TCP_CLIENT_H
#define WINDEMO_TCP_CLIENT_H

typedef enum TcpEventType {
    TCP_EVENT_CONNECTED,
    TCP_EVENT_DISCONNECTED,
    TCP_EVENT_ERROR,
    TCP_EVENT_SENT,
    TCP_EVENT_RECEIVED
} TcpEventType;

typedef void (*TcpEventCallback)(TcpEventType type, const char *message, void *context);

typedef struct TcpClient TcpClient;

TcpClient *tcp_client_create(TcpEventCallback callback, void *context);
int tcp_client_start(TcpClient *client, const char *host, unsigned short port);
void tcp_client_stop(TcpClient *client);
void tcp_client_destroy(TcpClient *client);

#endif
