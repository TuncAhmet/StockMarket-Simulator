#include "market_maker.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

MarketMaker* market_maker_create(const char* ticker, double initial_price,
                                  double mu, double sigma, double spread_bps,
                                  uint32_t order_size, int num_levels) {
    MarketMaker* mm = (MarketMaker*)calloc(1, sizeof(MarketMaker));
    if (!mm) return NULL;
    
    strncpy(mm->ticker, ticker, sizeof(mm->ticker) - 1);
    
    // Time step: assume updates every 100ms = 0.1 second
    // Convert to annual: 0.1 / (252 * 6.5 * 3600) ≈ 1.7e-8
    double dt = 0.1 / (252.0 * 6.5 * 3600.0);
    
    mm->price_model = gbm_create(initial_price, mu, sigma, dt);
    if (!mm->price_model) {
        free(mm);
        return NULL;
    }
    
    mm->spread_bps = spread_bps;
    mm->order_size = order_size;
    mm->num_levels = num_levels;
    mm->level_spacing_bps = 5.0;  // 5 bps between levels
    
    mm->max_orders = num_levels * 2;
    mm->bid_orders = (uint64_t*)calloc(num_levels, sizeof(uint64_t));
    mm->ask_orders = (uint64_t*)calloc(num_levels, sizeof(uint64_t));
    
    if (!mm->bid_orders || !mm->ask_orders) {
        free(mm->bid_orders);
        free(mm->ask_orders);
        gbm_destroy(mm->price_model);
        free(mm);
        return NULL;
    }
    
    return mm;
}

void market_maker_destroy(MarketMaker* mm) {
    if (!mm) return;
    
    gbm_destroy(mm->price_model);
    free(mm->bid_orders);
    free(mm->ask_orders);
    free(mm);
}

void market_maker_cancel_all(MarketMaker* mm, ExchangeEngine* engine) {
    if (!mm || !engine) return;
    
    for (int i = 0; i < mm->num_levels; i++) {
        if (mm->bid_orders[i] > 0) {
            engine_cancel_order(engine, mm->ticker, mm->bid_orders[i]);
            mm->bid_orders[i] = 0;
        }
        if (mm->ask_orders[i] > 0) {
            engine_cancel_order(engine, mm->ticker, mm->ask_orders[i]);
            mm->ask_orders[i] = 0;
        }
    }
}

void market_maker_update(MarketMaker* mm, ExchangeEngine* engine) {
    if (!mm || !engine) return;
    
    // Get next GBM price
    double fair_price = gbm_next_price(mm->price_model);
    
    // Cancel existing orders
    market_maker_cancel_all(mm, engine);
    
    // Calculate spread
    double half_spread = fair_price * (mm->spread_bps / 10000.0) / 2.0;
    double level_spacing = fair_price * (mm->level_spacing_bps / 10000.0);
    
    // Place new orders at multiple levels
    for (int i = 0; i < mm->num_levels; i++) {
        double offset = i * level_spacing;
        
        // Bid orders (below fair price)
        double bid_price = fair_price - half_spread - offset;
        MatchResult* bid_result = engine_submit_order(engine, mm->ticker, 
                                                       SIDE_BUY, ORDER_TYPE_LIMIT,
                                                       bid_price, mm->order_size);
        if (bid_result) {
            // Store order ID if added to book (not immediately matched)
            // For simplicity, we assume market makers add liquidity
            match_result_destroy(bid_result);
        }
        
        // Ask orders (above fair price)
        double ask_price = fair_price + half_spread + offset;
        MatchResult* ask_result = engine_submit_order(engine, mm->ticker,
                                                       SIDE_SELL, ORDER_TYPE_LIMIT,
                                                       ask_price, mm->order_size);
        if (ask_result) {
            match_result_destroy(ask_result);
        }
    }
}

// Pool functions
MarketMakerPool* market_maker_pool_create(ExchangeEngine* engine) {
    MarketMakerPool* pool = (MarketMakerPool*)calloc(1, sizeof(MarketMakerPool));
    if (!pool) return NULL;
    
    pool->capacity = 16;
    pool->makers = (MarketMaker**)calloc(pool->capacity, sizeof(MarketMaker*));
    if (!pool->makers) {
        free(pool);
        return NULL;
    }
    
    pool->count = 0;
    pool->engine = engine;
    pool->running = true;
    
    return pool;
}

void market_maker_pool_destroy(MarketMakerPool* pool) {
    if (!pool) return;
    
    pool->running = false;
    
    for (int i = 0; i < pool->count; i++) {
        market_maker_destroy(pool->makers[i]);
    }
    
    free(pool->makers);
    free(pool);
}

bool market_maker_pool_add(MarketMakerPool* pool, MarketMaker* mm) {
    if (!pool || !mm) return false;
    
    if (pool->count >= pool->capacity) {
        pool->capacity *= 2;
        pool->makers = (MarketMaker**)realloc(pool->makers, 
                                               pool->capacity * sizeof(MarketMaker*));
        if (!pool->makers) return false;
    }
    
    pool->makers[pool->count++] = mm;
    return true;
}

void market_maker_pool_update_all(MarketMakerPool* pool) {
    if (!pool || !pool->running) return;
    
    for (int i = 0; i < pool->count; i++) {
        market_maker_update(pool->makers[i], pool->engine);
    }
}
