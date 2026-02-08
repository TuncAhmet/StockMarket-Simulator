#ifndef MARKET_MAKER_H
#define MARKET_MAKER_H

#include "engine.h"
#include "math_model.h"

// Market maker configuration
typedef struct {
    char ticker[16];
    GBMModel* price_model;
    double spread_bps;          // Spread in basis points (e.g., 10 = 0.1%)
    uint32_t order_size;        // Size of each order
    int num_levels;             // Number of price levels to maintain
    double level_spacing_bps;   // Spacing between levels in bps
    
    // Active orders
    uint64_t* bid_orders;
    uint64_t* ask_orders;
    int max_orders;
} MarketMaker;

// Market maker pool
typedef struct {
    MarketMaker** makers;
    int count;
    int capacity;
    ExchangeEngine* engine;
    bool running;
} MarketMakerPool;

// Market maker functions
MarketMaker* market_maker_create(const char* ticker, double initial_price,
                                  double mu, double sigma, double spread_bps,
                                  uint32_t order_size, int num_levels);
void market_maker_destroy(MarketMaker* mm);

// Pool functions
MarketMakerPool* market_maker_pool_create(ExchangeEngine* engine);
void market_maker_pool_destroy(MarketMakerPool* pool);
bool market_maker_pool_add(MarketMakerPool* pool, MarketMaker* mm);

// Update cycle
void market_maker_update(MarketMaker* mm, ExchangeEngine* engine);
void market_maker_pool_update_all(MarketMakerPool* pool);

// Cancel all orders for a market maker
void market_maker_cancel_all(MarketMaker* mm, ExchangeEngine* engine);

#endif // MARKET_MAKER_H
