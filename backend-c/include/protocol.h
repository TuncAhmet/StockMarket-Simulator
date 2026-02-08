#ifndef PROTOCOL_H
#define PROTOCOL_H

#include "order_book.h"
#include "engine.h"
#include <stdbool.h>

// Message types
typedef enum {
    MSG_ORDER_NEW,
    MSG_ORDER_CANCEL,
    MSG_MARKET_DATA,
    MSG_EXECUTION_REPORT,
    MSG_PORTFOLIO_SYNC,
    MSG_ERROR,
    MSG_HEARTBEAT
} MessageType;

// Order request from client
typedef struct {
    char ticker[16];
    OrderSide side;
    OrderType type;
    double price;
    uint32_t quantity;
} OrderRequest;

// Cancel request from client
typedef struct {
    char ticker[16];
    uint64_t order_id;
} CancelRequest;

// Market data update
typedef struct {
    char ticker[16];
    double bid;
    double ask;
    double last;
    uint32_t bid_size;
    uint32_t ask_size;
    uint32_t last_size;
    double open;
    double high;
    double low;
    double volume;
    uint64_t timestamp;
} MarketDataUpdate;

// Portfolio position
typedef struct {
    char ticker[16];
    int32_t quantity;       // Can be negative for shorts
    double avg_cost;
    double unrealized_pnl;
    double realized_pnl;
} PortfolioPosition;

// General message wrapper
typedef struct {
    MessageType type;
    union {
        OrderRequest order_req;
        CancelRequest cancel_req;
        MarketDataUpdate market_data;
        ExecutionReport exec_report;
        PortfolioPosition portfolio;
        char error_msg[256];
    } payload;
} ProtocolMessage;

// JSON serialization
char* protocol_serialize_message(const ProtocolMessage* msg);
bool protocol_deserialize_message(const char* json, ProtocolMessage* msg);

// Helper serialization functions
char* protocol_serialize_market_data(const MarketDataUpdate* data);
char* protocol_serialize_execution(const ExecutionReport* report);
char* protocol_serialize_error(const char* error);

// Helper deserialization functions
bool protocol_parse_order_request(const char* json, OrderRequest* req);
bool protocol_parse_cancel_request(const char* json, CancelRequest* req);

#endif // PROTOCOL_H
