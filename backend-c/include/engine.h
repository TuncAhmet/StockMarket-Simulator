#ifndef ENGINE_H
#define ENGINE_H

#include "order_book.h"

#define MAX_TICKERS 16

// Match result
typedef struct {
    ExecutionReport* reports;
    int count;
    int capacity;
} MatchResult;

// Exchange engine
typedef struct {
    OrderBook* books[MAX_TICKERS];
    int num_tickers;
    bool running;
    mutex_t engine_lock;
} ExchangeEngine;

// Engine lifecycle
ExchangeEngine* engine_create(void);
void engine_destroy(ExchangeEngine* engine);
bool engine_add_ticker(ExchangeEngine* engine, const char* ticker, double initial_price);

// Order processing
MatchResult* engine_submit_order(ExchangeEngine* engine, const char* ticker,
                                 OrderSide side, OrderType type,
                                 double price, uint32_t quantity);
bool engine_cancel_order(ExchangeEngine* engine, const char* ticker, uint64_t order_id);

// Query
OrderBook* engine_get_order_book(ExchangeEngine* engine, const char* ticker);
int engine_get_ticker_index(ExchangeEngine* engine, const char* ticker);

// Matching
MatchResult* match_result_create(void);
void match_result_destroy(MatchResult* result);
void match_result_add(MatchResult* result, ExecutionReport report);

// Internal matching functions
void engine_match_orders(ExchangeEngine* engine, OrderBook* book, Order* incoming, MatchResult* result);

#endif // ENGINE_H
