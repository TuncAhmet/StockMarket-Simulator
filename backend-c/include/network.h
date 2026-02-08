#ifndef NETWORK_H
#define NETWORK_H

#include "protocol.h"
#include "engine.h"

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    typedef SOCKET socket_t;
    #define INVALID_SOCK INVALID_SOCKET
    #define CLOSE_SOCKET closesocket
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <fcntl.h>
    typedef int socket_t;
    #define INVALID_SOCK -1
    #define CLOSE_SOCKET close
#endif

#define MAX_CLIENTS 32
#define RECV_BUFFER_SIZE 4096

// Client connection
typedef struct {
    socket_t socket;
    struct sockaddr_in addr;
    char recv_buffer[RECV_BUFFER_SIZE];
    int buffer_len;
    bool connected;
    uint64_t last_activity;
} ClientConnection;

// Network server
typedef struct {
    socket_t listen_socket;
    int port;
    bool running;
    
    ClientConnection clients[MAX_CLIENTS];
    int num_clients;
    mutex_t clients_lock;
    
    ExchangeEngine* engine;
} NetworkServer;

// Server lifecycle
NetworkServer* network_server_create(int port, ExchangeEngine* engine);
void network_server_destroy(NetworkServer* server);

// Server operations
bool network_server_start(NetworkServer* server);
void network_server_stop(NetworkServer* server);
void network_server_poll(NetworkServer* server);  // Non-blocking poll for activity

// Client operations
void network_accept_client(NetworkServer* server);
void network_handle_client_data(NetworkServer* server, int client_idx);
void network_disconnect_client(NetworkServer* server, int client_idx);

// Broadcasting
void network_broadcast_market_data(NetworkServer* server, const MarketDataUpdate* data);
void network_send_to_client(NetworkServer* server, int client_idx, const char* json);

// Platform initialization
bool network_init(void);
void network_cleanup(void);

#endif // NETWORK_H
