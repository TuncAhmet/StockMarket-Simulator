#ifndef ORDER_BOOK_H
#define ORDER_BOOK_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <winsock2.h>
    #include <windows.h>
    typedef CRITICAL_SECTION mutex_t;
#else
    #include <pthread.h>
    typedef pthread_mutex_t mutex_t;
#endif

// Order side
typedef enum {
    SIDE_BUY,
    SIDE_SELL
} OrderSide;

// Order type
typedef enum {
    ORDER_TYPE_MARKET,
    ORDER_TYPE_LIMIT
} OrderType;

// Order status
typedef enum {
    ORDER_STATUS_NEW,
    ORDER_STATUS_PARTIAL,
    ORDER_STATUS_FILLED,
    ORDER_STATUS_CANCELLED
} OrderStatus;

// Single order
typedef struct Order {
    uint64_t id;
    char ticker[16];
    OrderSide side;
    OrderType type;
    double price;           // 0 for market orders
    uint32_t quantity;
    uint32_t filled_qty;
    OrderStatus status;
    uint64_t timestamp;     // Microseconds since epoch
    struct Order* next;
    struct Order* prev;
} Order;

// Price level (one price point in the order book)
typedef struct PriceLevel {
    double price;
    uint32_t total_quantity;
    Order* head;
    Order* tail;
    struct PriceLevel* left;
    struct PriceLevel* right;
    struct PriceLevel* parent;
    int height;             // For AVL balancing
} PriceLevel;

// Order Book for a single ticker
typedef struct {
    char ticker[16];
    PriceLevel* bids;       // Buy orders (max-heap by price)
    PriceLevel* asks;       // Sell orders (min-heap by price)
    uint64_t next_order_id;
    mutex_t lock;
    
    // Best prices cache
    double best_bid;
    double best_ask;
    double last_trade_price;
    uint32_t last_trade_qty;
} OrderBook;

// Execution report
typedef struct {
    uint64_t order_id;
    uint64_t match_id;
    double price;
    uint32_t quantity;
    OrderStatus status;
    uint64_t timestamp;
} ExecutionReport;

// Order Book functions
OrderBook* order_book_create(const char* ticker);
void order_book_destroy(OrderBook* book);

// Thread-safe operations
void order_book_lock(OrderBook* book);
void order_book_unlock(OrderBook* book);

// Order operations
Order* order_book_add_order(OrderBook* book, OrderSide side, OrderType type, 
                            double price, uint32_t quantity);
bool order_book_cancel_order(OrderBook* book, uint64_t order_id);

// Query operations
double order_book_get_best_bid(OrderBook* book);
double order_book_get_best_ask(OrderBook* book);
double order_book_get_mid_price(OrderBook* book);
double order_book_get_spread(OrderBook* book);

// Price level operations
PriceLevel* order_book_get_bids(OrderBook* book, int max_levels, int* count);
PriceLevel* order_book_get_asks(OrderBook* book, int max_levels, int* count);

// Utility
uint64_t get_timestamp_us(void);
Order* order_create(uint64_t id, const char* ticker, OrderSide side, 
                    OrderType type, double price, uint32_t quantity);
void order_destroy(Order* order);

#endif // ORDER_BOOK_H
