#include "engine.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Platform-specific mutex operations
#ifdef _WIN32
    #include <windows.h>
    static void engine_mutex_init(mutex_t* mutex) { InitializeCriticalSection(mutex); }
    static void engine_mutex_destroy(mutex_t* mutex) { DeleteCriticalSection(mutex); }
    static void engine_mutex_lock(mutex_t* mutex) { EnterCriticalSection(mutex); }
    static void engine_mutex_unlock(mutex_t* mutex) { LeaveCriticalSection(mutex); }
#else
    static void engine_mutex_init(mutex_t* mutex) { pthread_mutex_init(mutex, NULL); }
    static void engine_mutex_destroy(mutex_t* mutex) { pthread_mutex_destroy(mutex); }
    static void engine_mutex_lock(mutex_t* mutex) { pthread_mutex_lock(mutex); }
    static void engine_mutex_unlock(mutex_t* mutex) { pthread_mutex_unlock(mutex); }
#endif

// Create match result
MatchResult* match_result_create(void) {
    MatchResult* result = (MatchResult*)calloc(1, sizeof(MatchResult));
    if (!result) return NULL;
    
    result->capacity = 16;
    result->reports = (ExecutionReport*)calloc(result->capacity, sizeof(ExecutionReport));
    if (!result->reports) {
        free(result);
        return NULL;
    }
    result->count = 0;
    
    return result;
}

void match_result_destroy(MatchResult* result) {
    if (result) {
        free(result->reports);
        free(result);
    }
}

void match_result_add(MatchResult* result, ExecutionReport report) {
    if (result->count >= result->capacity) {
        result->capacity *= 2;
        result->reports = (ExecutionReport*)realloc(result->reports, 
                                                     result->capacity * sizeof(ExecutionReport));
    }
    result->reports[result->count++] = report;
}

// Create engine
ExchangeEngine* engine_create(void) {
    ExchangeEngine* engine = (ExchangeEngine*)calloc(1, sizeof(ExchangeEngine));
    if (!engine) return NULL;
    
    engine->num_tickers = 0;
    engine->running = true;
    engine_mutex_init(&engine->engine_lock);
    
    return engine;
}

void engine_destroy(ExchangeEngine* engine) {
    if (!engine) return;
    
    engine->running = false;
    
    for (int i = 0; i < engine->num_tickers; i++) {
        order_book_destroy(engine->books[i]);
    }
    
    engine_mutex_destroy(&engine->engine_lock);
    free(engine);
}

bool engine_add_ticker(ExchangeEngine* engine, const char* ticker, double initial_price) {
    if (!engine || engine->num_tickers >= MAX_TICKERS) return false;
    
    engine_mutex_lock(&engine->engine_lock);
    
    // Check if ticker already exists
    for (int i = 0; i < engine->num_tickers; i++) {
        if (strcmp(engine->books[i]->ticker, ticker) == 0) {
            engine_mutex_unlock(&engine->engine_lock);
            return false;
        }
    }
    
    OrderBook* book = order_book_create(ticker);
    if (!book) {
        engine_mutex_unlock(&engine->engine_lock);
        return false;
    }
    
    book->last_trade_price = initial_price;
    engine->books[engine->num_tickers++] = book;
    
    engine_mutex_unlock(&engine->engine_lock);
    return true;
}

int engine_get_ticker_index(ExchangeEngine* engine, const char* ticker) {
    if (!engine || !ticker) return -1;
    
    for (int i = 0; i < engine->num_tickers; i++) {
        if (strcmp(engine->books[i]->ticker, ticker) == 0) {
            return i;
        }
    }
    return -1;
}

OrderBook* engine_get_order_book(ExchangeEngine* engine, const char* ticker) {
    int idx = engine_get_ticker_index(engine, ticker);
    return (idx >= 0) ? engine->books[idx] : NULL;
}

// Core matching logic
void engine_match_orders(ExchangeEngine* engine, OrderBook* book, Order* incoming, MatchResult* result) {
    if (!engine || !book || !incoming || !result) return;
    
    bool is_buy = (incoming->side == SIDE_BUY);
    
    while (incoming->quantity > incoming->filled_qty) {
        // Get best opposing price level
        PriceLevel* best_opposing = NULL;
        double best_price = 0.0;
        
        if (is_buy) {
            best_price = order_book_get_best_ask(book);
            if (best_price <= 0) break;  // No asks available
            
            // For limit orders, check if price is acceptable
            if (incoming->type == ORDER_TYPE_LIMIT && best_price > incoming->price) {
                break;  // Ask price too high
            }
        } else {
            best_price = order_book_get_best_bid(book);
            if (best_price <= 0) break;  // No bids available
            
            // For limit orders, check if price is acceptable
            if (incoming->type == ORDER_TYPE_LIMIT && best_price < incoming->price) {
                break;  // Bid price too low
            }
        }
        
        // Find the actual price level
        PriceLevel** root = is_buy ? &book->asks : &book->bids;
        PriceLevel* level = *root;
        
        // Navigate to leftmost (best) price
        while (level && level->left) level = level->left;
        if (!level) break;
        
        // Match against orders at this level (FIFO)
        Order* resting = level->head;
        while (resting && incoming->quantity > incoming->filled_qty) {
            uint32_t remaining_incoming = incoming->quantity - incoming->filled_qty;
            uint32_t remaining_resting = resting->quantity - resting->filled_qty;
            uint32_t fill_qty = (remaining_incoming < remaining_resting) ? remaining_incoming : remaining_resting;
            
            // Execute the trade
            incoming->filled_qty += fill_qty;
            resting->filled_qty += fill_qty;
            level->total_quantity -= fill_qty;
            
            // Update last trade info
            book->last_trade_price = level->price;
            book->last_trade_qty = fill_qty;
            
            // Create execution reports
            ExecutionReport report_incoming = {
                .order_id = incoming->id,
                .match_id = resting->id,
                .price = level->price,
                .quantity = fill_qty,
                .status = (incoming->filled_qty >= incoming->quantity) ? ORDER_STATUS_FILLED : ORDER_STATUS_PARTIAL,
                .timestamp = get_timestamp_us()
            };
            match_result_add(result, report_incoming);
            
            ExecutionReport report_resting = {
                .order_id = resting->id,
                .match_id = incoming->id,
                .price = level->price,
                .quantity = fill_qty,
                .status = (resting->filled_qty >= resting->quantity) ? ORDER_STATUS_FILLED : ORDER_STATUS_PARTIAL,
                .timestamp = get_timestamp_us()
            };
            match_result_add(result, report_resting);
            
            // Update order statuses
            if (incoming->filled_qty >= incoming->quantity) {
                incoming->status = ORDER_STATUS_FILLED;
            } else {
                incoming->status = ORDER_STATUS_PARTIAL;
            }
            
            Order* next_resting = resting->next;
            
            if (resting->filled_qty >= resting->quantity) {
                resting->status = ORDER_STATUS_FILLED;
                
                // Remove filled order from level
                if (resting->prev) resting->prev->next = resting->next;
                else level->head = resting->next;
                
                if (resting->next) resting->next->prev = resting->prev;
                else level->tail = resting->prev;
                
                order_destroy(resting);
            }
            
            resting = next_resting;
        }
        
        // Remove empty price level
        if (!level->head) {
            if (is_buy) {
                book->asks = NULL;  // Simplified - proper AVL delete needed
                // Update best ask
                book->best_ask = order_book_get_best_ask(book);
            } else {
                book->bids = NULL;  // Simplified - proper AVL delete needed
                // Update best bid
                book->best_bid = order_book_get_best_bid(book);
            }
        }
        
        // If no more fills possible, break
        if (!resting) break;
    }
}

// Submit an order
MatchResult* engine_submit_order(ExchangeEngine* engine, const char* ticker,
                                 OrderSide side, OrderType type,
                                 double price, uint32_t quantity) {
    if (!engine || !ticker) return NULL;
    
    OrderBook* book = engine_get_order_book(engine, ticker);
    if (!book) return NULL;
    
    MatchResult* result = match_result_create();
    if (!result) return NULL;
    
    order_book_lock(book);
    
    // Create the order
    Order* order = order_book_add_order(book, side, type, price, quantity);
    if (!order) {
        order_book_unlock(book);
        match_result_destroy(result);
        return NULL;
    }
    
    // Try to match
    engine_match_orders(engine, book, order, result);
    
    // If market order wasn't fully filled, mark as cancelled (no book placement)
    if (type == ORDER_TYPE_MARKET && order->filled_qty < order->quantity) {
        order->status = ORDER_STATUS_CANCELLED;
        order_destroy(order);
    }
    // Limit orders that aren't fully filled remain in the book
    
    order_book_unlock(book);
    
    return result;
}

bool engine_cancel_order(ExchangeEngine* engine, const char* ticker, uint64_t order_id) {
    if (!engine || !ticker) return false;
    
    OrderBook* book = engine_get_order_book(engine, ticker);
    if (!book) return false;
    
    order_book_lock(book);
    bool cancelled = order_book_cancel_order(book, order_id);
    order_book_unlock(book);
    
    return cancelled;
}
