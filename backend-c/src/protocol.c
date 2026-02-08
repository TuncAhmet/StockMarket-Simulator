#include "protocol.h"
#include "cJSON.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Helper: Get string for order side
static const char* side_to_string(OrderSide side) {
    return (side == SIDE_BUY) ? "BUY" : "SELL";
}

static OrderSide string_to_side(const char* str) {
    if (str && strcmp(str, "BUY") == 0) return SIDE_BUY;
    return SIDE_SELL;
}

// Helper: Get string for order type
static const char* type_to_string(OrderType type) {
    return (type == ORDER_TYPE_MARKET) ? "MARKET" : "LIMIT";
}

static OrderType string_to_type(const char* str) {
    if (str && strcmp(str, "MARKET") == 0) return ORDER_TYPE_MARKET;
    return ORDER_TYPE_LIMIT;
}

// Helper: Get string for message type
static const char* msg_type_to_string(MessageType type) {
    switch (type) {
        case MSG_ORDER_NEW: return "ORDER_NEW";
        case MSG_ORDER_CANCEL: return "ORDER_CANCEL";
        case MSG_MARKET_DATA: return "MARKET_DATA";
        case MSG_EXECUTION_REPORT: return "EXECUTION_REPORT";
        case MSG_PORTFOLIO_SYNC: return "PORTFOLIO_SYNC";
        case MSG_ERROR: return "ERROR";
        case MSG_HEARTBEAT: return "HEARTBEAT";
        default: return "UNKNOWN";
    }
}

static MessageType string_to_msg_type(const char* str) {
    if (!str) return MSG_ERROR;
    if (strcmp(str, "ORDER_NEW") == 0) return MSG_ORDER_NEW;
    if (strcmp(str, "ORDER_CANCEL") == 0) return MSG_ORDER_CANCEL;
    if (strcmp(str, "MARKET_DATA") == 0) return MSG_MARKET_DATA;
    if (strcmp(str, "EXECUTION_REPORT") == 0) return MSG_EXECUTION_REPORT;
    if (strcmp(str, "PORTFOLIO_SYNC") == 0) return MSG_PORTFOLIO_SYNC;
    if (strcmp(str, "HEARTBEAT") == 0) return MSG_HEARTBEAT;
    return MSG_ERROR;
}

// Serialize market data update
char* protocol_serialize_market_data(const MarketDataUpdate* data) {
    if (!data) return NULL;
    
    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "type", "MARKET_DATA");
    cJSON_AddStringToObject(root, "ticker", data->ticker);
    cJSON_AddNumberToObject(root, "bid", data->bid);
    cJSON_AddNumberToObject(root, "ask", data->ask);
    cJSON_AddNumberToObject(root, "last", data->last);
    cJSON_AddNumberToObject(root, "bid_size", data->bid_size);
    cJSON_AddNumberToObject(root, "ask_size", data->ask_size);
    cJSON_AddNumberToObject(root, "last_size", data->last_size);
    cJSON_AddNumberToObject(root, "open", data->open);
    cJSON_AddNumberToObject(root, "high", data->high);
    cJSON_AddNumberToObject(root, "low", data->low);
    cJSON_AddNumberToObject(root, "volume", data->volume);
    cJSON_AddNumberToObject(root, "timestamp", (double)data->timestamp);
    
    char* json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    
    return json;
}

// Serialize execution report
char* protocol_serialize_execution(const ExecutionReport* report) {
    if (!report) return NULL;
    
    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "type", "EXECUTION_REPORT");
    cJSON_AddNumberToObject(root, "order_id", (double)report->order_id);
    cJSON_AddNumberToObject(root, "match_id", (double)report->match_id);
    cJSON_AddNumberToObject(root, "price", report->price);
    cJSON_AddNumberToObject(root, "quantity", report->quantity);
    
    const char* status_str = "UNKNOWN";
    switch (report->status) {
        case ORDER_STATUS_NEW: status_str = "NEW"; break;
        case ORDER_STATUS_PARTIAL: status_str = "PARTIAL"; break;
        case ORDER_STATUS_FILLED: status_str = "FILLED"; break;
        case ORDER_STATUS_CANCELLED: status_str = "CANCELLED"; break;
    }
    cJSON_AddStringToObject(root, "status", status_str);
    cJSON_AddNumberToObject(root, "timestamp", (double)report->timestamp);
    
    char* json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    
    return json;
}

// Serialize error
char* protocol_serialize_error(const char* error) {
    if (!error) return NULL;
    
    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "type", "ERROR");
    cJSON_AddStringToObject(root, "message", error);
    
    char* json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    
    return json;
}

// Parse order request
bool protocol_parse_order_request(const char* json, OrderRequest* req) {
    if (!json || !req) return false;
    
    cJSON* root = cJSON_Parse(json);
    if (!root) return false;
    
    memset(req, 0, sizeof(OrderRequest));
    
    cJSON* ticker = cJSON_GetObjectItem(root, "ticker");
    cJSON* side = cJSON_GetObjectItem(root, "side");
    cJSON* type = cJSON_GetObjectItem(root, "type");
    cJSON* price = cJSON_GetObjectItem(root, "price");
    cJSON* quantity = cJSON_GetObjectItem(root, "quantity");
    
    if (ticker && cJSON_IsString(ticker)) {
        strncpy(req->ticker, ticker->valuestring, sizeof(req->ticker) - 1);
    }
    if (side && cJSON_IsString(side)) {
        req->side = string_to_side(side->valuestring);
    }
    if (type && cJSON_IsString(type)) {
        req->type = string_to_type(type->valuestring);
    }
    if (price && cJSON_IsNumber(price)) {
        req->price = price->valuedouble;
    }
    if (quantity && cJSON_IsNumber(quantity)) {
        req->quantity = (uint32_t)quantity->valuedouble;
    }
    
    cJSON_Delete(root);
    return true;
}

// Parse cancel request
bool protocol_parse_cancel_request(const char* json, CancelRequest* req) {
    if (!json || !req) return false;
    
    cJSON* root = cJSON_Parse(json);
    if (!root) return false;
    
    memset(req, 0, sizeof(CancelRequest));
    
    cJSON* ticker = cJSON_GetObjectItem(root, "ticker");
    cJSON* order_id = cJSON_GetObjectItem(root, "order_id");
    
    if (ticker && cJSON_IsString(ticker)) {
        strncpy(req->ticker, ticker->valuestring, sizeof(req->ticker) - 1);
    }
    if (order_id && cJSON_IsNumber(order_id)) {
        req->order_id = (uint64_t)order_id->valuedouble;
    }
    
    cJSON_Delete(root);
    return true;
}

// General message serialization
char* protocol_serialize_message(const ProtocolMessage* msg) {
    if (!msg) return NULL;
    
    switch (msg->type) {
        case MSG_MARKET_DATA:
            return protocol_serialize_market_data(&msg->payload.market_data);
        case MSG_EXECUTION_REPORT:
            return protocol_serialize_execution(&msg->payload.exec_report);
        case MSG_ERROR:
            return protocol_serialize_error(msg->payload.error_msg);
        default:
            return NULL;
    }
}

// General message deserialization
bool protocol_deserialize_message(const char* json, ProtocolMessage* msg) {
    if (!json || !msg) return false;
    
    cJSON* root = cJSON_Parse(json);
    if (!root) return false;
    
    memset(msg, 0, sizeof(ProtocolMessage));
    
    cJSON* type = cJSON_GetObjectItem(root, "type");
    if (!type || !cJSON_IsString(type)) {
        cJSON_Delete(root);
        return false;
    }
    
    msg->type = string_to_msg_type(type->valuestring);
    
    bool result = false;
    switch (msg->type) {
        case MSG_ORDER_NEW:
            result = protocol_parse_order_request(json, &msg->payload.order_req);
            break;
        case MSG_ORDER_CANCEL:
            result = protocol_parse_cancel_request(json, &msg->payload.cancel_req);
            break;
        default:
            result = true;  // Other message types handled separately
            break;
    }
    
    cJSON_Delete(root);
    return result;
}
