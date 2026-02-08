#include "network.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef _WIN32
    #pragma comment(lib, "ws2_32.lib")
    static void net_mutex_init(mutex_t* m) { InitializeCriticalSection(m); }
    static void net_mutex_destroy(mutex_t* m) { DeleteCriticalSection(m); }
    static void net_mutex_lock(mutex_t* m) { EnterCriticalSection(m); }
    static void net_mutex_unlock(mutex_t* m) { LeaveCriticalSection(m); }
#else
    static void net_mutex_init(mutex_t* m) { pthread_mutex_init(m, NULL); }
    static void net_mutex_destroy(mutex_t* m) { pthread_mutex_destroy(m); }
    static void net_mutex_lock(mutex_t* m) { pthread_mutex_lock(m); }
    static void net_mutex_unlock(mutex_t* m) { pthread_mutex_unlock(m); }
#endif

bool network_init(void) {
#ifdef _WIN32
    WSADATA wsa_data;
    return WSAStartup(MAKEWORD(2, 2), &wsa_data) == 0;
#else
    return true;
#endif
}

void network_cleanup(void) {
#ifdef _WIN32
    WSACleanup();
#endif
}

NetworkServer* network_server_create(int port, ExchangeEngine* engine) {
    NetworkServer* server = (NetworkServer*)calloc(1, sizeof(NetworkServer));
    if (!server) return NULL;
    
    server->port = port;
    server->engine = engine;
    server->running = false;
    server->num_clients = 0;
    server->listen_socket = INVALID_SOCK;
    
    net_mutex_init(&server->clients_lock);
    
    for (int i = 0; i < MAX_CLIENTS; i++) {
        server->clients[i].connected = false;
        server->clients[i].socket = INVALID_SOCK;
    }
    
    return server;
}

void network_server_destroy(NetworkServer* server) {
    if (!server) return;
    
    network_server_stop(server);
    net_mutex_destroy(&server->clients_lock);
    free(server);
}

bool network_server_start(NetworkServer* server) {
    if (!server) return false;
    
    // Create socket
    server->listen_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server->listen_socket == INVALID_SOCK) {
        perror("socket");
        return false;
    }
    
    // Allow address reuse
    int opt = 1;
#ifdef _WIN32
    setsockopt(server->listen_socket, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
#else
    setsockopt(server->listen_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif
    
    // Set non-blocking
#ifdef _WIN32
    u_long mode = 1;
    ioctlsocket(server->listen_socket, FIONBIO, &mode);
#else
    int flags = fcntl(server->listen_socket, F_GETFL, 0);
    fcntl(server->listen_socket, F_SETFL, flags | O_NONBLOCK);
#endif
    
    // Bind
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(server->port);
    
    if (bind(server->listen_socket, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        CLOSE_SOCKET(server->listen_socket);
        return false;
    }
    
    // Listen
    if (listen(server->listen_socket, 10) < 0) {
        perror("listen");
        CLOSE_SOCKET(server->listen_socket);
        return false;
    }
    
    server->running = true;
    printf("Server listening on port %d\n", server->port);
    
    return true;
}

void network_server_stop(NetworkServer* server) {
    if (!server) return;
    
    server->running = false;
    
    // Close all client connections
    net_mutex_lock(&server->clients_lock);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (server->clients[i].connected) {
            CLOSE_SOCKET(server->clients[i].socket);
            server->clients[i].connected = false;
        }
    }
    server->num_clients = 0;
    net_mutex_unlock(&server->clients_lock);
    
    // Close listen socket
    if (server->listen_socket != INVALID_SOCK) {
        CLOSE_SOCKET(server->listen_socket);
        server->listen_socket = INVALID_SOCK;
    }
}

void network_accept_client(NetworkServer* server) {
    if (!server || !server->running) return;
    
    struct sockaddr_in client_addr;
    int addr_len = sizeof(client_addr);
    
#ifdef _WIN32
    socket_t client_sock = accept(server->listen_socket, (struct sockaddr*)&client_addr, &addr_len);
#else
    socket_t client_sock = accept(server->listen_socket, (struct sockaddr*)&client_addr, (socklen_t*)&addr_len);
#endif
    
    if (client_sock == INVALID_SOCK) return;  // No pending connection
    
    // Set client socket to non-blocking
#ifdef _WIN32
    u_long mode = 1;
    ioctlsocket(client_sock, FIONBIO, &mode);
#else
    int flags = fcntl(client_sock, F_GETFL, 0);
    fcntl(client_sock, F_SETFL, flags | O_NONBLOCK);
#endif
    
    net_mutex_lock(&server->clients_lock);
    
    // Find empty slot
    int slot = -1;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (!server->clients[i].connected) {
            slot = i;
            break;
        }
    }
    
    if (slot >= 0) {
        server->clients[slot].socket = client_sock;
        server->clients[slot].addr = client_addr;
        server->clients[slot].connected = true;
        server->clients[slot].buffer_len = 0;
        server->clients[slot].last_activity = get_timestamp_us();
        server->num_clients++;
        
        printf("Client connected from %s:%d (slot %d)\n",
               inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), slot);
    } else {
        CLOSE_SOCKET(client_sock);
        printf("Max clients reached, rejecting connection\n");
    }
    
    net_mutex_unlock(&server->clients_lock);
}

void network_disconnect_client(NetworkServer* server, int client_idx) {
    if (!server || client_idx < 0 || client_idx >= MAX_CLIENTS) return;
    
    net_mutex_lock(&server->clients_lock);
    
    if (server->clients[client_idx].connected) {
        CLOSE_SOCKET(server->clients[client_idx].socket);
        server->clients[client_idx].connected = false;
        server->clients[client_idx].socket = INVALID_SOCK;
        server->num_clients--;
        
        printf("Client disconnected (slot %d)\n", client_idx);
    }
    
    net_mutex_unlock(&server->clients_lock);
}

void network_handle_client_data(NetworkServer* server, int client_idx) {
    if (!server || client_idx < 0 || client_idx >= MAX_CLIENTS) return;
    
    ClientConnection* client = &server->clients[client_idx];
    if (!client->connected) return;
    
    // Receive data
    char temp_buf[1024];
    int recv_len = recv(client->socket, temp_buf, sizeof(temp_buf) - 1, 0);
    
    if (recv_len <= 0) {
        if (recv_len == 0) {
            // Connection closed
            network_disconnect_client(server, client_idx);
        }
        // recv_len < 0 with EWOULDBLOCK is normal for non-blocking
        return;
    }
    
    temp_buf[recv_len] = '\0';
    client->last_activity = get_timestamp_us();
    
    // Append to buffer
    int space = RECV_BUFFER_SIZE - client->buffer_len - 1;
    if (recv_len > space) recv_len = space;
    
    memcpy(client->recv_buffer + client->buffer_len, temp_buf, recv_len);
    client->buffer_len += recv_len;
    client->recv_buffer[client->buffer_len] = '\0';
    
    // Process complete messages (newline-delimited JSON)
    char* line_start = client->recv_buffer;
    char* newline;
    
    while ((newline = strchr(line_start, '\n')) != NULL) {
        *newline = '\0';
        
        if (strlen(line_start) > 0) {
            // Parse and handle message
            ProtocolMessage msg;
            if (protocol_deserialize_message(line_start, &msg)) {
                switch (msg.type) {
                    case MSG_ORDER_NEW: {
                        MatchResult* result = engine_submit_order(
                            server->engine,
                            msg.payload.order_req.ticker,
                            msg.payload.order_req.side,
                            msg.payload.order_req.type,
                            msg.payload.order_req.price,
                            msg.payload.order_req.quantity
                        );
                        
                        if (result) {
                            // Send execution reports
                            for (int i = 0; i < result->count; i++) {
                                char* json = protocol_serialize_execution(&result->reports[i]);
                                if (json) {
                                    network_send_to_client(server, client_idx, json);
                                    free(json);
                                }
                            }
                            match_result_destroy(result);
                        }
                        break;
                    }
                    case MSG_ORDER_CANCEL: {
                        bool cancelled = engine_cancel_order(
                            server->engine,
                            msg.payload.cancel_req.ticker,
                            msg.payload.cancel_req.order_id
                        );
                        
                        if (!cancelled) {
                            char* json = protocol_serialize_error("Order not found");
                            if (json) {
                                network_send_to_client(server, client_idx, json);
                                free(json);
                            }
                        }
                        break;
                    }
                    default:
                        break;
                }
            }
        }
        
        line_start = newline + 1;
    }
    
    // Move remaining data to start of buffer
    int remaining = client->buffer_len - (line_start - client->recv_buffer);
    if (remaining > 0) {
        memmove(client->recv_buffer, line_start, remaining);
    }
    client->buffer_len = remaining;
}

void network_server_poll(NetworkServer* server) {
    if (!server || !server->running) return;
    
    // Accept new connections
    network_accept_client(server);
    
    // Handle client data
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (server->clients[i].connected) {
            network_handle_client_data(server, i);
        }
    }
}

void network_send_to_client(NetworkServer* server, int client_idx, const char* json) {
    if (!server || !json || client_idx < 0 || client_idx >= MAX_CLIENTS) return;
    
    ClientConnection* client = &server->clients[client_idx];
    if (!client->connected) return;
    
    size_t len = strlen(json);
    char* msg = (char*)malloc(len + 2);
    if (!msg) return;
    
    memcpy(msg, json, len);
    msg[len] = '\n';
    msg[len + 1] = '\0';
    
    send(client->socket, msg, (int)(len + 1), 0);
    free(msg);
}

void network_broadcast_market_data(NetworkServer* server, const MarketDataUpdate* data) {
    if (!server || !data) return;
    
    char* json = protocol_serialize_market_data(data);
    if (!json) return;
    
    net_mutex_lock(&server->clients_lock);
    
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (server->clients[i].connected) {
            network_send_to_client(server, i, json);
        }
    }
    
    net_mutex_unlock(&server->clients_lock);
    
    free(json);
}
