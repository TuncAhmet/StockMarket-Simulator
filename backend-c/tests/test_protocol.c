#include "unity/unity.h"
#include "../include/protocol.h"
#include <string.h>
#include <stdlib.h>

void setUp(void) {
}

void tearDown(void) {
}

// Test market data serialization
void test_serialize_market_data(void) {
    MarketDataUpdate data = {
        .ticker = "AAPL",
        .bid = 150.25,
        .ask = 150.50,
        .last = 150.30,
        .bid_size = 1000,
        .ask_size = 800,
        .last_size = 100,
        .open = 149.00,
        .high = 151.00,
        .low = 148.50,
        .volume = 1000000.0,
        .timestamp = 1234567890
    };
    
    char* json = protocol_serialize_market_data(&data);
    TEST_ASSERT_NOT_NULL(json);
    
    // Verify it contains expected fields
    TEST_ASSERT_NOT_NULL(strstr(json, "\"type\":\"MARKET_DATA\""));
    TEST_ASSERT_NOT_NULL(strstr(json, "\"ticker\":\"AAPL\""));
    TEST_ASSERT_NOT_NULL(strstr(json, "\"bid\":150.25"));
    TEST_ASSERT_NOT_NULL(strstr(json, "\"ask\":150.5"));
    
    free(json);
}

// Test execution report serialization
void test_serialize_execution(void) {
    ExecutionReport report = {
        .order_id = 12345,
        .match_id = 67890,
        .price = 100.50,
        .quantity = 500,
        .status = ORDER_STATUS_FILLED,
        .timestamp = 9876543210
    };
    
    char* json = protocol_serialize_execution(&report);
    TEST_ASSERT_NOT_NULL(json);
    
    TEST_ASSERT_NOT_NULL(strstr(json, "\"type\":\"EXECUTION_REPORT\""));
    TEST_ASSERT_NOT_NULL(strstr(json, "\"status\":\"FILLED\""));
    TEST_ASSERT_NOT_NULL(strstr(json, "\"quantity\":500"));
    
    free(json);
}

// Test error serialization
void test_serialize_error(void) {
    char* json = protocol_serialize_error("Order not found");
    TEST_ASSERT_NOT_NULL(json);
    
    TEST_ASSERT_NOT_NULL(strstr(json, "\"type\":\"ERROR\""));
    TEST_ASSERT_NOT_NULL(strstr(json, "\"message\":\"Order not found\""));
    
    free(json);
}

// Test order request parsing
void test_parse_order_request(void) {
    const char* json = "{\"type\":\"ORDER_NEW\",\"ticker\":\"MSFT\",\"side\":\"BUY\",\"type\":\"LIMIT\",\"price\":380.0,\"quantity\":100}";
    
    OrderRequest req;
    bool success = protocol_parse_order_request(json, &req);
    
    TEST_ASSERT_TRUE(success);
    TEST_ASSERT_EQUAL_STRING("MSFT", req.ticker);
    TEST_ASSERT_EQUAL(SIDE_BUY, req.side);
    TEST_ASSERT_EQUAL(ORDER_TYPE_LIMIT, req.type);
    TEST_ASSERT_EQUAL_DOUBLE(380.0, req.price);
    TEST_ASSERT_EQUAL_UINT32(100, req.quantity);
}

// Test order request parsing - sell order
void test_parse_order_request_sell(void) {
    const char* json = "{\"ticker\":\"AAPL\",\"side\":\"SELL\",\"type\":\"MARKET\",\"price\":0,\"quantity\":50}";
    
    OrderRequest req;
    bool success = protocol_parse_order_request(json, &req);
    
    TEST_ASSERT_TRUE(success);
    TEST_ASSERT_EQUAL_STRING("AAPL", req.ticker);
    TEST_ASSERT_EQUAL(SIDE_SELL, req.side);
    TEST_ASSERT_EQUAL(ORDER_TYPE_MARKET, req.type);
    TEST_ASSERT_EQUAL_UINT32(50, req.quantity);
}

// Test cancel request parsing
void test_parse_cancel_request(void) {
    const char* json = "{\"type\":\"ORDER_CANCEL\",\"ticker\":\"GOOGL\",\"order_id\":12345}";
    
    CancelRequest req;
    bool success = protocol_parse_cancel_request(json, &req);
    
    TEST_ASSERT_TRUE(success);
    TEST_ASSERT_EQUAL_STRING("GOOGL", req.ticker);
    TEST_ASSERT_EQUAL_UINT64(12345, req.order_id);
}

// Test parsing invalid JSON
void test_parse_invalid_json(void) {
    const char* json = "this is not json";
    
    OrderRequest req;
    bool success = protocol_parse_order_request(json, &req);
    
    TEST_ASSERT_FALSE(success);
}

// Test parsing NULL input
void test_parse_null_input(void) {
    OrderRequest req;
    bool success = protocol_parse_order_request(NULL, &req);
    TEST_ASSERT_FALSE(success);
    
    CancelRequest cancel_req;
    success = protocol_parse_cancel_request(NULL, &cancel_req);
    TEST_ASSERT_FALSE(success);
}

// Test serialize NULL input
void test_serialize_null_input(void) {
    char* json = protocol_serialize_market_data(NULL);
    TEST_ASSERT_NULL(json);
    
    json = protocol_serialize_execution(NULL);
    TEST_ASSERT_NULL(json);
    
    json = protocol_serialize_error(NULL);
    TEST_ASSERT_NULL(json);
}

// Test general message deserialization
void test_deserialize_message(void) {
    const char* json = "{\"type\":\"ORDER_NEW\",\"ticker\":\"TSLA\",\"side\":\"BUY\",\"type\":\"LIMIT\",\"price\":250.0,\"quantity\":10}";
    
    ProtocolMessage msg;
    bool success = protocol_deserialize_message(json, &msg);
    
    TEST_ASSERT_TRUE(success);
    TEST_ASSERT_EQUAL(MSG_ORDER_NEW, msg.type);
}

// Test execution status serialization
void test_execution_status_values(void) {
    ExecutionReport report = {
        .order_id = 1,
        .match_id = 2,
        .price = 100.0,
        .quantity = 100,
        .timestamp = 123
    };
    
    // Test NEW status
    report.status = ORDER_STATUS_NEW;
    char* json = protocol_serialize_execution(&report);
    TEST_ASSERT_NOT_NULL(strstr(json, "\"status\":\"NEW\""));
    free(json);
    
    // Test PARTIAL status
    report.status = ORDER_STATUS_PARTIAL;
    json = protocol_serialize_execution(&report);
    TEST_ASSERT_NOT_NULL(strstr(json, "\"status\":\"PARTIAL\""));
    free(json);
    
    // Test CANCELLED status
    report.status = ORDER_STATUS_CANCELLED;
    json = protocol_serialize_execution(&report);
    TEST_ASSERT_NOT_NULL(strstr(json, "\"status\":\"CANCELLED\""));
    free(json);
}

int main(void) {
    UNITY_BEGIN();
    
    RUN_TEST(test_serialize_market_data);
    RUN_TEST(test_serialize_execution);
    RUN_TEST(test_serialize_error);
    RUN_TEST(test_parse_order_request);
    RUN_TEST(test_parse_order_request_sell);
    RUN_TEST(test_parse_cancel_request);
    RUN_TEST(test_parse_invalid_json);
    RUN_TEST(test_parse_null_input);
    RUN_TEST(test_serialize_null_input);
    RUN_TEST(test_deserialize_message);
    RUN_TEST(test_execution_status_values);
    
    return UNITY_END();
}
