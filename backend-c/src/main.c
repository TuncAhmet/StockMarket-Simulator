#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include "engine.h"
#include "network.h"
#include "market_maker.h"
#include "math_model.h"

#ifdef _WIN32
    #include <windows.h>
    #define SLEEP_MS(ms) Sleep(ms)
#else
    #include <unistd.h>
    #include <pthread.h>
    #define SLEEP_MS(ms) usleep((ms) * 1000)
#endif

// Default configuration
#define DEFAULT_PORT 8080
#define UPDATE_INTERVAL_MS 100

// Global state for signal handling
static volatile int g_running = 1;

void signal_handler(int sig) {
    (void)sig;
    g_running = 0;
    printf("\nShutting down...\n");
}

// Thread arguments
typedef struct {
    ExchangeEngine* engine;
    NetworkServer* server;
    MarketMakerPool* mm_pool;
} ThreadArgs;

#ifdef _WIN32
DWORD WINAPI simulation_thread(LPVOID arg) {
#else
void* simulation_thread(void* arg) {
#endif
    ThreadArgs* args = (ThreadArgs*)arg;
    
    printf("Simulation thread started\n");
    
    while (g_running) {
        // Update market makers (simulates price movement and provides liquidity)
        market_maker_pool_update_all(args->mm_pool);
        
        // Broadcast market data for each ticker
        for (int i = 0; i < args->engine->num_tickers; i++) {
            OrderBook* book = args->engine->books[i];
            
            order_book_lock(book);
            
            MarketDataUpdate update;
            memset(&update, 0, sizeof(update));
            strncpy(update.ticker, book->ticker, sizeof(update.ticker) - 1);
            update.bid = order_book_get_best_bid(book);
            update.ask = order_book_get_best_ask(book);
            update.last = book->last_trade_price;
            update.last_size = book->last_trade_qty;
            update.timestamp = get_timestamp_us();
            
            order_book_unlock(book);
            
            network_broadcast_market_data(args->server, &update);
        }
        
        SLEEP_MS(UPDATE_INTERVAL_MS);
    }
    
    printf("Simulation thread stopped\n");
    return 0;
}

#ifdef _WIN32
DWORD WINAPI network_thread(LPVOID arg) {
#else
void* network_thread(void* arg) {
#endif
    ThreadArgs* args = (ThreadArgs*)arg;
    
    printf("Network thread started\n");
    
    while (g_running && args->server->running) {
        network_server_poll(args->server);
        SLEEP_MS(10);  // Small sleep to prevent busy-waiting
    }
    
    printf("Network thread stopped\n");
    return 0;
}

void print_usage(const char* program) {
    printf("Usage: %s [options]\n", program);
    printf("Options:\n");
    printf("  -p, --port PORT    Server port (default: %d)\n", DEFAULT_PORT);
    printf("  -h, --help         Show this help message\n");
}

int main(int argc, char* argv[]) {
    int port = DEFAULT_PORT;
    
    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--port") == 0) {
            if (i + 1 < argc) {
                port = atoi(argv[++i]);
            }
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        }
    }
    
    // Setup signal handler
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    printf("=== Stock Exchange Engine ===\n");
    printf("Initializing...\n");
    
    // Initialize network
    if (!network_init()) {
        fprintf(stderr, "Failed to initialize network\n");
        return 1;
    }
    
    // Seed RNG
    rng_seed((unsigned int)get_timestamp_us());
    
    // Create exchange engine
    ExchangeEngine* engine = engine_create();
    if (!engine) {
        fprintf(stderr, "Failed to create exchange engine\n");
        network_cleanup();
        return 1;
    }
    
    // Add tickers with initial prices
    const char* tickers[] = { "AAPL", "MSFT", "GOOGL", "AMZN", "TSLA" };
    const double prices[] = { 150.0, 380.0, 140.0, 180.0, 250.0 };
    const int num_tickers = sizeof(tickers) / sizeof(tickers[0]);
    
    for (int i = 0; i < num_tickers; i++) {
        if (engine_add_ticker(engine, tickers[i], prices[i])) {
            printf("Added ticker: %s @ $%.2f\n", tickers[i], prices[i]);
        }
    }
    
    // Create network server
    NetworkServer* server = network_server_create(port, engine);
    if (!server) {
        fprintf(stderr, "Failed to create network server\n");
        engine_destroy(engine);
        network_cleanup();
        return 1;
    }
    
    // Start server
    if (!network_server_start(server)) {
        fprintf(stderr, "Failed to start network server\n");
        network_server_destroy(server);
        engine_destroy(engine);
        network_cleanup();
        return 1;
    }
    
    // Create market maker pool
    MarketMakerPool* mm_pool = market_maker_pool_create(engine);
    if (!mm_pool) {
        fprintf(stderr, "Failed to create market maker pool\n");
        network_server_destroy(server);
        engine_destroy(engine);
        network_cleanup();
        return 1;
    }
    
    // Create market makers for each ticker
    // Parameters: mu=0.05 (5% annual drift), sigma=0.2 (20% volatility)
    // spread=20 bps, order_size=100, num_levels=5
    for (int i = 0; i < num_tickers; i++) {
        MarketMaker* mm = market_maker_create(tickers[i], prices[i], 
                                               0.05, 0.20, 20.0, 100, 5);
        if (mm) {
            market_maker_pool_add(mm_pool, mm);
            printf("Created market maker for %s\n", tickers[i]);
        }
    }
    
    // Thread arguments
    ThreadArgs args = { engine, server, mm_pool };
    
    // Start threads
#ifdef _WIN32
    HANDLE sim_thread = CreateThread(NULL, 0, simulation_thread, &args, 0, NULL);
    HANDLE net_thread = CreateThread(NULL, 0, network_thread, &args, 0, NULL);
    
    if (!sim_thread || !net_thread) {
        fprintf(stderr, "Failed to create threads\n");
        g_running = 0;
    }
    
    // Wait for threads
    if (sim_thread) WaitForSingleObject(sim_thread, INFINITE);
    if (net_thread) WaitForSingleObject(net_thread, INFINITE);
    
    if (sim_thread) CloseHandle(sim_thread);
    if (net_thread) CloseHandle(net_thread);
#else
    pthread_t sim_thread, net_thread;
    
    if (pthread_create(&sim_thread, NULL, simulation_thread, &args) != 0) {
        fprintf(stderr, "Failed to create simulation thread\n");
        g_running = 0;
    }
    
    if (pthread_create(&net_thread, NULL, network_thread, &args) != 0) {
        fprintf(stderr, "Failed to create network thread\n");
        g_running = 0;
    }
    
    // Wait for threads
    pthread_join(sim_thread, NULL);
    pthread_join(net_thread, NULL);
#endif
    
    // Cleanup
    printf("Cleaning up...\n");
    market_maker_pool_destroy(mm_pool);
    network_server_destroy(server);
    engine_destroy(engine);
    network_cleanup();
    
    printf("Shutdown complete\n");
    return 0;
}
