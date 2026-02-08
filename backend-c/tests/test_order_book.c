#include "unity/unity.h"
#include "../include/order_book.h"
#include <string.h>

void setUp(void) {
    // Set up before each test
}

void tearDown(void) {
    // Clean up after each test
}

// Test order creation
void test_order_create(void) {
    Order* order = order_create(1, "AAPL", SIDE_BUY, ORDER_TYPE_LIMIT, 150.0, 100);
    
    TEST_ASSERT_NOT_NULL(order);
    TEST_ASSERT_EQUAL_UINT64(1, order->id);
    TEST_ASSERT_EQUAL_STRING("AAPL", order->ticker);
    TEST_ASSERT_EQUAL(SIDE_BUY, order->side);
    TEST_ASSERT_EQUAL(ORDER_TYPE_LIMIT, order->type);
    TEST_ASSERT_EQUAL_DOUBLE(150.0, order->price);
    TEST_ASSERT_EQUAL_UINT32(100, order->quantity);
    TEST_ASSERT_EQUAL_UINT32(0, order->filled_qty);
    TEST_ASSERT_EQUAL(ORDER_STATUS_NEW, order->status);
    TEST_ASSERT_TRUE(order->timestamp > 0);
    
    order_destroy(order);
}

// Test order book creation
void test_order_book_create(void) {
    OrderBook* book = order_book_create("AAPL");
    
    TEST_ASSERT_NOT_NULL(book);
    TEST_ASSERT_EQUAL_STRING("AAPL", book->ticker);
    TEST_ASSERT_NULL(book->bids);
    TEST_ASSERT_NULL(book->asks);
    TEST_ASSERT_EQUAL_UINT64(1, book->next_order_id);
    TEST_ASSERT_EQUAL_DOUBLE(0.0, book->best_bid);
    TEST_ASSERT_EQUAL_DOUBLE(0.0, book->best_ask);
    
    order_book_destroy(book);
}

// Test adding a single buy order
void test_add_single_buy_order(void) {
    OrderBook* book = order_book_create("AAPL");
    
    Order* order = order_book_add_order(book, SIDE_BUY, ORDER_TYPE_LIMIT, 150.0, 100);
    
    TEST_ASSERT_NOT_NULL(order);
    TEST_ASSERT_EQUAL_UINT64(1, order->id);
    TEST_ASSERT_EQUAL_DOUBLE(150.0, order_book_get_best_bid(book));
    TEST_ASSERT_EQUAL_DOUBLE(0.0, order_book_get_best_ask(book));
    
    order_book_destroy(book);
}

// Test adding a single sell order
void test_add_single_sell_order(void) {
    OrderBook* book = order_book_create("AAPL");
    
    Order* order = order_book_add_order(book, SIDE_SELL, ORDER_TYPE_LIMIT, 155.0, 100);
    
    TEST_ASSERT_NOT_NULL(order);
    TEST_ASSERT_EQUAL_UINT64(1, order->id);
    TEST_ASSERT_EQUAL_DOUBLE(0.0, order_book_get_best_bid(book));
    TEST_ASSERT_EQUAL_DOUBLE(155.0, order_book_get_best_ask(book));
    
    order_book_destroy(book);
}

// Test price-time priority for bids (higher price = better)
void test_bid_price_priority(void) {
    OrderBook* book = order_book_create("AAPL");
    
    // Add orders at different prices
    order_book_add_order(book, SIDE_BUY, ORDER_TYPE_LIMIT, 150.0, 100);
    order_book_add_order(book, SIDE_BUY, ORDER_TYPE_LIMIT, 152.0, 100);
    order_book_add_order(book, SIDE_BUY, ORDER_TYPE_LIMIT, 148.0, 100);
    
    // Best bid should be the highest price
    TEST_ASSERT_EQUAL_DOUBLE(152.0, order_book_get_best_bid(book));
    
    order_book_destroy(book);
}

// Test price-time priority for asks (lower price = better)
void test_ask_price_priority(void) {
    OrderBook* book = order_book_create("AAPL");
    
    // Add orders at different prices
    order_book_add_order(book, SIDE_SELL, ORDER_TYPE_LIMIT, 155.0, 100);
    order_book_add_order(book, SIDE_SELL, ORDER_TYPE_LIMIT, 153.0, 100);
    order_book_add_order(book, SIDE_SELL, ORDER_TYPE_LIMIT, 157.0, 100);
    
    // Best ask should be the lowest price
    TEST_ASSERT_EQUAL_DOUBLE(153.0, order_book_get_best_ask(book));
    
    order_book_destroy(book);
}

// Test spread calculation
void test_spread_calculation(void) {
    OrderBook* book = order_book_create("AAPL");
    
    order_book_add_order(book, SIDE_BUY, ORDER_TYPE_LIMIT, 150.0, 100);
    order_book_add_order(book, SIDE_SELL, ORDER_TYPE_LIMIT, 152.0, 100);
    
    TEST_ASSERT_EQUAL_DOUBLE(150.0, order_book_get_best_bid(book));
    TEST_ASSERT_EQUAL_DOUBLE(152.0, order_book_get_best_ask(book));
    TEST_ASSERT_EQUAL_DOUBLE(2.0, order_book_get_spread(book));
    TEST_ASSERT_EQUAL_DOUBLE(151.0, order_book_get_mid_price(book));
    
    order_book_destroy(book);
}

// Test order cancellation
void test_order_cancellation(void) {
    OrderBook* book = order_book_create("AAPL");
    
    Order* order1 = order_book_add_order(book, SIDE_BUY, ORDER_TYPE_LIMIT, 150.0, 100);
    Order* order2 = order_book_add_order(book, SIDE_BUY, ORDER_TYPE_LIMIT, 152.0, 100);
    
    uint64_t order1_id = order1->id;
    uint64_t order2_id = order2->id;
    
    TEST_ASSERT_EQUAL_DOUBLE(152.0, order_book_get_best_bid(book));
    
    // Cancel the best bid
    bool cancelled = order_book_cancel_order(book, order2_id);
    TEST_ASSERT_TRUE(cancelled);
    
    // Best bid should now be the second order
    TEST_ASSERT_EQUAL_DOUBLE(150.0, order_book_get_best_bid(book));
    
    // Cancel remaining order
    cancelled = order_book_cancel_order(book, order1_id);
    TEST_ASSERT_TRUE(cancelled);
    
    TEST_ASSERT_EQUAL_DOUBLE(0.0, order_book_get_best_bid(book));
    
    order_book_destroy(book);
}

// Test cancelling non-existent order
void test_cancel_nonexistent_order(void) {
    OrderBook* book = order_book_create("AAPL");
    
    bool cancelled = order_book_cancel_order(book, 999);
    TEST_ASSERT_FALSE(cancelled);
    
    order_book_destroy(book);
}

// Test market order not added to book
void test_market_order_not_in_book(void) {
    OrderBook* book = order_book_create("AAPL");
    
    Order* order = order_book_add_order(book, SIDE_BUY, ORDER_TYPE_MARKET, 0.0, 100);
    
    TEST_ASSERT_NOT_NULL(order);
    TEST_ASSERT_EQUAL(ORDER_TYPE_MARKET, order->type);
    
    // Market orders don't go into the book so best bid should still be 0
    TEST_ASSERT_EQUAL_DOUBLE(0.0, order_book_get_best_bid(book));
    
    order_destroy(order);  // Caller owns market orders
    order_book_destroy(book);
}

// Test multiple orders at same price (FIFO)
void test_multiple_orders_same_price(void) {
    OrderBook* book = order_book_create("AAPL");
    
    order_book_add_order(book, SIDE_BUY, ORDER_TYPE_LIMIT, 150.0, 100);
    order_book_add_order(book, SIDE_BUY, ORDER_TYPE_LIMIT, 150.0, 200);
    order_book_add_order(book, SIDE_BUY, ORDER_TYPE_LIMIT, 150.0, 300);
    
    // Best bid should still be 150.0
    TEST_ASSERT_EQUAL_DOUBLE(150.0, order_book_get_best_bid(book));
    
    order_book_destroy(book);
}

// Test locking/unlocking
void test_thread_safety_lock(void) {
    OrderBook* book = order_book_create("AAPL");
    
    // This should not deadlock
    order_book_lock(book);
    order_book_unlock(book);
    
    order_book_destroy(book);
    TEST_PASS();
}

int main(void) {
    UNITY_BEGIN();
    
    RUN_TEST(test_order_create);
    RUN_TEST(test_order_book_create);
    RUN_TEST(test_add_single_buy_order);
    RUN_TEST(test_add_single_sell_order);
    RUN_TEST(test_bid_price_priority);
    RUN_TEST(test_ask_price_priority);
    RUN_TEST(test_spread_calculation);
    RUN_TEST(test_order_cancellation);
    RUN_TEST(test_cancel_nonexistent_order);
    RUN_TEST(test_market_order_not_in_book);
    RUN_TEST(test_multiple_orders_same_price);
    RUN_TEST(test_thread_safety_lock);
    
    return UNITY_END();
}
