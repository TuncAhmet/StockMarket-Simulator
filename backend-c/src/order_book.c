#include "order_book.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef _WIN32
    #include <windows.h>
#else
    #include <sys/time.h>
#endif

// Platform-specific mutex operations
static void mutex_init(mutex_t* mutex) {
#ifdef _WIN32
    InitializeCriticalSection(mutex);
#else
    pthread_mutex_init(mutex, NULL);
#endif
}

static void mutex_destroy(mutex_t* mutex) {
#ifdef _WIN32
    DeleteCriticalSection(mutex);
#else
    pthread_mutex_destroy(mutex);
#endif
}

void order_book_lock(OrderBook* book) {
#ifdef _WIN32
    EnterCriticalSection(&book->lock);
#else
    pthread_mutex_lock(&book->lock);
#endif
}

void order_book_unlock(OrderBook* book) {
#ifdef _WIN32
    LeaveCriticalSection(&book->lock);
#else
    pthread_mutex_unlock(&book->lock);
#endif
}

// Get current timestamp in microseconds
uint64_t get_timestamp_us(void) {
#ifdef _WIN32
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    uint64_t t = ((uint64_t)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
    return (t - 116444736000000000ULL) / 10;  // Convert to Unix epoch in microseconds
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000000 + tv.tv_usec;
#endif
}

// Create a new order
Order* order_create(uint64_t id, const char* ticker, OrderSide side, 
                    OrderType type, double price, uint32_t quantity) {
    Order* order = (Order*)calloc(1, sizeof(Order));
    if (!order) return NULL;
    
    order->id = id;
    strncpy(order->ticker, ticker, sizeof(order->ticker) - 1);
    order->side = side;
    order->type = type;
    order->price = price;
    order->quantity = quantity;
    order->filled_qty = 0;
    order->status = ORDER_STATUS_NEW;
    order->timestamp = get_timestamp_us();
    order->next = NULL;
    order->prev = NULL;
    
    return order;
}

void order_destroy(Order* order) {
    free(order);
}

// Create a new price level
static PriceLevel* price_level_create(double price) {
    PriceLevel* level = (PriceLevel*)calloc(1, sizeof(PriceLevel));
    if (!level) return NULL;
    
    level->price = price;
    level->total_quantity = 0;
    level->head = NULL;
    level->tail = NULL;
    level->left = NULL;
    level->right = NULL;
    level->parent = NULL;
    level->height = 1;
    
    return level;
}

static void price_level_destroy(PriceLevel* level) {
    if (!level) return;
    
    // Free all orders at this level
    Order* order = level->head;
    while (order) {
        Order* next = order->next;
        order_destroy(order);
        order = next;
    }
    
    free(level);
}

// AVL tree height helper
static int get_height(PriceLevel* node) {
    return node ? node->height : 0;
}

static int get_balance(PriceLevel* node) {
    return node ? get_height(node->left) - get_height(node->right) : 0;
}

static void update_height(PriceLevel* node) {
    if (node) {
        int left_h = get_height(node->left);
        int right_h = get_height(node->right);
        node->height = (left_h > right_h ? left_h : right_h) + 1;
    }
}

// AVL rotations
static PriceLevel* rotate_right(PriceLevel* y) {
    PriceLevel* x = y->left;
    PriceLevel* T2 = x->right;
    
    x->right = y;
    y->left = T2;
    
    x->parent = y->parent;
    y->parent = x;
    if (T2) T2->parent = y;
    
    update_height(y);
    update_height(x);
    
    return x;
}

static PriceLevel* rotate_left(PriceLevel* x) {
    PriceLevel* y = x->right;
    PriceLevel* T2 = y->left;
    
    y->left = x;
    x->right = T2;
    
    y->parent = x->parent;
    x->parent = y;
    if (T2) T2->parent = x;
    
    update_height(x);
    update_height(y);
    
    return y;
}

// Insert a price level into the AVL tree
static PriceLevel* avl_insert(PriceLevel* node, PriceLevel* new_level, bool is_bid) {
    if (!node) {
        return new_level;
    }
    
    // For bids: higher prices go left (descending order)
    // For asks: lower prices go left (ascending order)
    bool go_left = is_bid ? (new_level->price > node->price) : (new_level->price < node->price);
    
    if (go_left) {
        node->left = avl_insert(node->left, new_level, is_bid);
        if (node->left) node->left->parent = node;
    } else {
        node->right = avl_insert(node->right, new_level, is_bid);
        if (node->right) node->right->parent = node;
    }
    
    update_height(node);
    
    int balance = get_balance(node);
    
    // Left Left
    if (balance > 1 && get_balance(node->left) >= 0)
        return rotate_right(node);
    
    // Left Right
    if (balance > 1 && get_balance(node->left) < 0) {
        node->left = rotate_left(node->left);
        return rotate_right(node);
    }
    
    // Right Right
    if (balance < -1 && get_balance(node->right) <= 0)
        return rotate_left(node);
    
    // Right Left
    if (balance < -1 && get_balance(node->right) > 0) {
        node->right = rotate_right(node->right);
        return rotate_left(node);
    }
    
    return node;
}

// Find a price level in the tree
static PriceLevel* avl_find(PriceLevel* node, double price, bool is_bid) {
    while (node) {
        if (price == node->price) return node;
        bool go_left = is_bid ? (price > node->price) : (price < node->price);
        node = go_left ? node->left : node->right;
    }
    return NULL;
}

// Get the leftmost (best) price level
static PriceLevel* avl_get_best(PriceLevel* node) {
    if (!node) return NULL;
    while (node->left) node = node->left;
    return node;
}

// Get minimum node in subtree
static PriceLevel* avl_min(PriceLevel* node) {
    while (node && node->left) node = node->left;
    return node;
}

// Delete a price level from AVL tree
static PriceLevel* avl_delete(PriceLevel* root, double price, bool is_bid) {
    if (!root) return NULL;
    
    bool go_left = is_bid ? (price > root->price) : (price < root->price);
    
    if (price == root->price) {
        // Node to delete found
        if (!root->left || !root->right) {
            PriceLevel* temp = root->left ? root->left : root->right;
            if (!temp) {
                price_level_destroy(root);
                return NULL;
            } else {
                temp->parent = root->parent;
                price_level_destroy(root);
                return temp;
            }
        } else {
            // Find in-order successor
            PriceLevel* successor = avl_min(root->right);
            root->price = successor->price;
            root->total_quantity = successor->total_quantity;
            root->head = successor->head;
            root->tail = successor->tail;
            root->right = avl_delete(root->right, successor->price, is_bid);
        }
    } else if (go_left) {
        root->left = avl_delete(root->left, price, is_bid);
        if (root->left) root->left->parent = root;
    } else {
        root->right = avl_delete(root->right, price, is_bid);
        if (root->right) root->right->parent = root;
    }
    
    update_height(root);
    
    int balance = get_balance(root);
    
    if (balance > 1 && get_balance(root->left) >= 0)
        return rotate_right(root);
    if (balance > 1 && get_balance(root->left) < 0) {
        root->left = rotate_left(root->left);
        return rotate_right(root);
    }
    if (balance < -1 && get_balance(root->right) <= 0)
        return rotate_left(root);
    if (balance < -1 && get_balance(root->right) > 0) {
        root->right = rotate_right(root->right);
        return rotate_left(root);
    }
    
    return root;
}

// Recursively destroy all nodes in an AVL tree
static void avl_destroy(PriceLevel* node) {
    if (!node) return;
    avl_destroy(node->left);
    avl_destroy(node->right);
    price_level_destroy(node);
}

// Create order book
OrderBook* order_book_create(const char* ticker) {
    OrderBook* book = (OrderBook*)calloc(1, sizeof(OrderBook));
    if (!book) return NULL;
    
    strncpy(book->ticker, ticker, sizeof(book->ticker) - 1);
    book->bids = NULL;
    book->asks = NULL;
    book->next_order_id = 1;
    book->best_bid = 0.0;
    book->best_ask = 0.0;
    book->last_trade_price = 0.0;
    book->last_trade_qty = 0;
    
    mutex_init(&book->lock);
    
    return book;
}

void order_book_destroy(OrderBook* book) {
    if (!book) return;
    
    avl_destroy(book->bids);
    avl_destroy(book->asks);
    mutex_destroy(&book->lock);
    
    free(book);
}

// Add an order to the book
Order* order_book_add_order(OrderBook* book, OrderSide side, OrderType type, 
                            double price, uint32_t quantity) {
    if (!book) return NULL;
    
    uint64_t order_id = book->next_order_id++;
    Order* order = order_create(order_id, book->ticker, side, type, price, quantity);
    if (!order) return NULL;
    
    // Market orders shouldn't be added to the book, return immediately
    if (type == ORDER_TYPE_MARKET) {
        return order;
    }
    
    PriceLevel** root = (side == SIDE_BUY) ? &book->bids : &book->asks;
    bool is_bid = (side == SIDE_BUY);
    
    // Find or create price level
    PriceLevel* level = avl_find(*root, price, is_bid);
    if (!level) {
        level = price_level_create(price);
        if (!level) {
            order_destroy(order);
            return NULL;
        }
        *root = avl_insert(*root, level, is_bid);
    }
    
    // Add order to the end of the level (FIFO)
    if (level->tail) {
        level->tail->next = order;
        order->prev = level->tail;
    } else {
        level->head = order;
    }
    level->tail = order;
    level->total_quantity += quantity;
    
    // Update best prices
    PriceLevel* best = avl_get_best(*root);
    if (is_bid) {
        book->best_bid = best ? best->price : 0.0;
    } else {
        book->best_ask = best ? best->price : 0.0;
    }
    
    return order;
}

// Cancel an order by ID
bool order_book_cancel_order(OrderBook* book, uint64_t order_id) {
    if (!book) return false;
    
    // Search in both sides
    PriceLevel* sides[] = { book->bids, book->asks };
    bool is_bids[] = { true, false };
    
    for (int s = 0; s < 2; s++) {
        PriceLevel* level = sides[s];
        // In-order traversal to find the order
        PriceLevel* stack[64];
        int stack_top = 0;
        
        while (level || stack_top > 0) {
            while (level) {
                stack[stack_top++] = level;
                level = level->left;
            }
            
            level = stack[--stack_top];
            
            // Search orders at this level
            Order* order = level->head;
            while (order) {
                if (order->id == order_id) {
                    // Found the order, remove it
                    if (order->prev) order->prev->next = order->next;
                    else level->head = order->next;
                    
                    if (order->next) order->next->prev = order->prev;
                    else level->tail = order->prev;
                    
                    level->total_quantity -= (order->quantity - order->filled_qty);
                    
                    // Remove empty price level
                    if (!level->head) {
                        if (is_bids[s]) {
                            book->bids = avl_delete(book->bids, level->price, true);
                        } else {
                            book->asks = avl_delete(book->asks, level->price, false);
                        }
                        
                        // Update best prices
                        PriceLevel* best = avl_get_best(is_bids[s] ? book->bids : book->asks);
                        if (is_bids[s]) {
                            book->best_bid = best ? best->price : 0.0;
                        } else {
                            book->best_ask = best ? best->price : 0.0;
                        }
                    }
                    
                    order_destroy(order);
                    return true;
                }
                order = order->next;
            }
            
            level = level->right;
        }
    }
    
    return false;
}

double order_book_get_best_bid(OrderBook* book) {
    if (!book) return 0.0;
    PriceLevel* best = avl_get_best(book->bids);
    return best ? best->price : 0.0;
}

double order_book_get_best_ask(OrderBook* book) {
    if (!book) return 0.0;
    PriceLevel* best = avl_get_best(book->asks);
    return best ? best->price : 0.0;
}

double order_book_get_mid_price(OrderBook* book) {
    double bid = order_book_get_best_bid(book);
    double ask = order_book_get_best_ask(book);
    if (bid > 0 && ask > 0) return (bid + ask) / 2.0;
    if (bid > 0) return bid;
    if (ask > 0) return ask;
    return book->last_trade_price;
}

double order_book_get_spread(OrderBook* book) {
    double bid = order_book_get_best_bid(book);
    double ask = order_book_get_best_ask(book);
    if (bid > 0 && ask > 0) return ask - bid;
    return 0.0;
}

// Get price levels for visualization (returns array, caller must free)
static void collect_levels(PriceLevel* node, PriceLevel** arr, int* count, int max_levels) {
    if (!node || *count >= max_levels) return;
    collect_levels(node->left, arr, count, max_levels);
    if (*count < max_levels) {
        arr[*count] = node;
        (*count)++;
    }
    collect_levels(node->right, arr, count, max_levels);
}

PriceLevel* order_book_get_bids(OrderBook* book, int max_levels, int* count) {
    *count = 0;
    if (!book || !book->bids) return NULL;
    
    PriceLevel* arr = (PriceLevel*)malloc(sizeof(PriceLevel) * max_levels);
    if (!arr) return NULL;
    
    collect_levels(book->bids, &arr, count, max_levels);
    return arr;
}

PriceLevel* order_book_get_asks(OrderBook* book, int max_levels, int* count) {
    *count = 0;
    if (!book || !book->asks) return NULL;
    
    PriceLevel* arr = (PriceLevel*)malloc(sizeof(PriceLevel) * max_levels);
    if (!arr) return NULL;
    
    collect_levels(book->asks, &arr, count, max_levels);
    return arr;
}
