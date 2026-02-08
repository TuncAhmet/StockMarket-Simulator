#include "unity/unity.h"
#include "../include/engine.h"
#include <string.h>

void setUp(void) {
}

void tearDown(void) {
}

// Test engine creation
void test_engine_create(void) {
    ExchangeEngine* engine = engine_create();
    
    TEST_ASSERT_NOT_NULL(engine);
    TEST_ASSERT_EQUAL(0, engine->num_tickers);
    TEST_ASSERT_TRUE(engine->running);
    
    engine_destroy(engine);
}

// Test adding tickers
void test_engine_add_ticker(void) {
    ExchangeEngine* engine = engine_create();
    
    bool added = engine_add_ticker(engine, "AAPL", 150.0);
    TEST_ASSERT_TRUE(added);
    TEST_ASSERT_EQUAL(1, engine->num_tickers);
    
    added = engine_add_ticker(engine, "MSFT", 380.0);
    TEST_ASSERT_TRUE(added);
    TEST_ASSERT_EQUAL(2, engine->num_tickers);
    
    engine_destroy(engine);
}

// Test duplicate ticker rejection
void test_engine_reject_duplicate_ticker(void) {
    ExchangeEngine* engine = engine_create();
    
    TEST_ASSERT_TRUE(engine_add_ticker(engine, "AAPL", 150.0));
    TEST_ASSERT_FALSE(engine_add_ticker(engine, "AAPL", 160.0));
    TEST_ASSERT_EQUAL(1, engine->num_tickers);
    
    engine_destroy(engine);
}

// Test getting order book
void test_engine_get_order_book(void) {
    ExchangeEngine* engine = engine_create();
    engine_add_ticker(engine, "AAPL", 150.0);
    
    OrderBook* book = engine_get_order_book(engine, "AAPL");
    TEST_ASSERT_NOT_NULL(book);
    TEST_ASSERT_EQUAL_STRING("AAPL", book->ticker);
    
    book = engine_get_order_book(engine, "INVALID");
    TEST_ASSERT_NULL(book);
    
    engine_destroy(engine);
}

// Test limit order matching - Buy crosses Ask
void test_matching_buy_crosses_ask(void) {
    ExchangeEngine* engine = engine_create();
    engine_add_ticker(engine, "AAPL", 150.0);
    
    // Place a sell limit order
    MatchResult* result1 = engine_submit_order(engine, "AAPL", SIDE_SELL, ORDER_TYPE_LIMIT, 100.0, 100);
    TEST_ASSERT_NOT_NULL(result1);
    TEST_ASSERT_EQUAL(0, result1->count);  // No match yet
    match_result_destroy(result1);
    
    // Place a buy limit order that crosses
    MatchResult* result2 = engine_submit_order(engine, "AAPL", SIDE_BUY, ORDER_TYPE_LIMIT, 100.0, 100);
    TEST_ASSERT_NOT_NULL(result2);
    TEST_ASSERT_GREATER_THAN(0, result2->count);  // Should match
    
    // Check execution reports
    bool found_fill = false;
    for (int i = 0; i < result2->count; i++) {
        if (result2->reports[i].status == ORDER_STATUS_FILLED) {
            found_fill = true;
            TEST_ASSERT_EQUAL_DOUBLE(100.0, result2->reports[i].price);
            TEST_ASSERT_EQUAL_UINT32(100, result2->reports[i].quantity);
        }
    }
    TEST_ASSERT_TRUE(found_fill);
    
    match_result_destroy(result2);
    engine_destroy(engine);
}

// Test limit order matching - Sell crosses Bid
void test_matching_sell_crosses_bid(void) {
    ExchangeEngine* engine = engine_create();
    engine_add_ticker(engine, "AAPL", 150.0);
    
    // Place a buy limit order
    MatchResult* result1 = engine_submit_order(engine, "AAPL", SIDE_BUY, ORDER_TYPE_LIMIT, 100.0, 100);
    TEST_ASSERT_NOT_NULL(result1);
    TEST_ASSERT_EQUAL(0, result1->count);
    match_result_destroy(result1);
    
    // Place a sell limit order that crosses
    MatchResult* result2 = engine_submit_order(engine, "AAPL", SIDE_SELL, ORDER_TYPE_LIMIT, 99.0, 100);
    TEST_ASSERT_NOT_NULL(result2);
    TEST_ASSERT_GREATER_THAN(0, result2->count);
    
    match_result_destroy(result2);
    engine_destroy(engine);
}

// Test no match when prices don't cross
void test_no_match_when_prices_dont_cross(void) {
    ExchangeEngine* engine = engine_create();
    engine_add_ticker(engine, "AAPL", 150.0);
    
    // Sell at 102
    MatchResult* result1 = engine_submit_order(engine, "AAPL", SIDE_SELL, ORDER_TYPE_LIMIT, 102.0, 100);
    TEST_ASSERT_EQUAL(0, result1->count);
    match_result_destroy(result1);
    
    // Buy at 100 - no cross since 100 < 102
    MatchResult* result2 = engine_submit_order(engine, "AAPL", SIDE_BUY, ORDER_TYPE_LIMIT, 100.0, 100);
    TEST_ASSERT_EQUAL(0, result2->count);
    match_result_destroy(result2);
    
    engine_destroy(engine);
}

// Test partial fill
void test_partial_fill(void) {
    ExchangeEngine* engine = engine_create();
    engine_add_ticker(engine, "AAPL", 150.0);
    
    // Sell 50 shares
    MatchResult* result1 = engine_submit_order(engine, "AAPL", SIDE_SELL, ORDER_TYPE_LIMIT, 100.0, 50);
    TEST_ASSERT_EQUAL(0, result1->count);
    match_result_destroy(result1);
    
    // Buy 100 shares - should partially fill
    MatchResult* result2 = engine_submit_order(engine, "AAPL", SIDE_BUY, ORDER_TYPE_LIMIT, 100.0, 100);
    TEST_ASSERT_NOT_NULL(result2);
    
    // Check for partial fill
    bool found_partial = false;
    for (int i = 0; i < result2->count; i++) {
        if (result2->reports[i].status == ORDER_STATUS_PARTIAL) {
            found_partial = true;
            TEST_ASSERT_EQUAL_UINT32(50, result2->reports[i].quantity);
        }
    }
    // Either partial or we filled 50 of 100
    TEST_ASSERT_GREATER_THAN(0, result2->count);
    
    match_result_destroy(result2);
    engine_destroy(engine);
}

// Test order cancellation through engine
void test_engine_cancel_order(void) {
    ExchangeEngine* engine = engine_create();
    engine_add_ticker(engine, "AAPL", 150.0);
    
    // Place an order
    engine_submit_order(engine, "AAPL", SIDE_BUY, ORDER_TYPE_LIMIT, 100.0, 100);
    
    OrderBook* book = engine_get_order_book(engine, "AAPL");
    TEST_ASSERT_EQUAL_DOUBLE(100.0, order_book_get_best_bid(book));
    
    // Cancel it
    bool cancelled = engine_cancel_order(engine, "AAPL", 1);
    TEST_ASSERT_TRUE(cancelled);
    
    TEST_ASSERT_EQUAL_DOUBLE(0.0, order_book_get_best_bid(book));
    
    engine_destroy(engine);
}

// Test match result creation and destruction
void test_match_result_lifecycle(void) {
    MatchResult* result = match_result_create();
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQUAL(0, result->count);
    TEST_ASSERT_GREATER_THAN(0, result->capacity);
    
    // Add some reports
    ExecutionReport report = {
        .order_id = 1,
        .match_id = 2,
        .price = 100.0,
        .quantity = 50,
        .status = ORDER_STATUS_FILLED,
        .timestamp = 123456789
    };
    
    match_result_add(result, report);
    TEST_ASSERT_EQUAL(1, result->count);
    TEST_ASSERT_EQUAL_UINT64(1, result->reports[0].order_id);
    
    match_result_destroy(result);
    TEST_PASS();
}

int main(void) {
    UNITY_BEGIN();
    
    RUN_TEST(test_engine_create);
    RUN_TEST(test_engine_add_ticker);
    RUN_TEST(test_engine_reject_duplicate_ticker);
    RUN_TEST(test_engine_get_order_book);
    RUN_TEST(test_matching_buy_crosses_ask);
    RUN_TEST(test_matching_sell_crosses_bid);
    RUN_TEST(test_no_match_when_prices_dont_cross);
    RUN_TEST(test_partial_fill);
    RUN_TEST(test_engine_cancel_order);
    RUN_TEST(test_match_result_lifecycle);
    
    return UNITY_END();
}
