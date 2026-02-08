# Stock Exchange Engine & Algorithmic Trading Dashboard

A high-performance Order Matching Engine written in C with a Python PyQt6 trading dashboard for real-time visualization.

[![CI](https://github.com/TuncAhmet/StockMarket-Simulator/actions/workflows/ci.yml/badge.svg)](https://github.com/TuncAhmet/StockMarket-Simulator/actions/workflows/ci.yml)
![C](https://img.shields.io/badge/C-99-blue.svg)
![Python](https://img.shields.io/badge/Python-3.10+-green.svg)
![License](https://img.shields.io/badge/license-MIT-blue.svg)

## Features

### C Backend (Exchange Engine)
- **Limit Order Book (LOB)** with AVL tree-based price levels supporting price-time priority
- **Order Types**: Market orders (immediate execution) and Limit orders (price-specific)
- **Matching Engine**: Automatic matching when buy/sell prices cross
- **Geometric Brownian Motion (GBM)**: Realistic price simulation using stochastic processes
- **Market Maker Bots**: Automated liquidity providers based on GBM model
- **Multi-threaded Architecture**: Separate simulation and networking threads
- **Thread-Safe**: Mutex-protected order book operations

### Python Frontend (Trading Dashboard)
- **Real-time Candlestick Charts** using PyQtGraph
- **Order Book Depth Visualization**: Bid/Ask cumulative volume display
- **Trading Controls**: Submit Market and Limit orders
- **Portfolio Tracker**: Live P&L and position management
- **Event Log**: Scrolling terminal for server messages

### Communication
- **TCP Socket Server**: Non-blocking, multi-client support
- **JSON Protocol**: Human-readable message format using cJSON library

## Project Structure

```
StockMarket-Simulator/
├── backend-c/
│   ├── src/                 # C source files
│   │   ├── main.c           # Entry point, threading
│   │   ├── order_book.c     # AVL-based limit order book
│   │   ├── engine.c         # Order matching logic
│   │   ├── math_model.c     # GBM price simulation
│   │   ├── market_maker.c   # Automated liquidity
│   │   ├── network.c        # TCP socket server
│   │   └── protocol.c       # JSON serialization
│   ├── include/             # Header files
│   ├── tests/               # Unity framework tests
│   │   ├── unity/           # Unity test framework
│   │   ├── test_order_book.c
│   │   ├── test_matching.c
│   │   ├── test_gbm.c
│   │   └── test_protocol.c
│   ├── lib/cjson/           # cJSON library
│   └── CMakeLists.txt       # Build configuration
├── frontend-python/
│   ├── ui/                  # PyQt6 widgets
│   │   └── main_window.py   # Main dashboard
│   ├── core/                # Networking
│   │   └── socket_client.py # Async socket client
│   ├── charts/              # Visualization
│   │   ├── candlestick.py   # OHLC chart
│   │   └── orderbook_depth.py
│   ├── main.py              # Entry point
│   └── requirements.txt     # Python dependencies
└── docs/                    # Documentation
```

## Build Instructions

### Prerequisites

- **C Compiler**: GCC, Clang, or MSVC
- **CMake**: 3.16 or higher
- **Python**: 3.10 or higher
- **pip**: For Python package management

### Build C Backend

```bash
cd backend-c
mkdir build && cd build
cmake ..
cmake --build .
```

### Run Tests

```bash
cd backend-c/build
ctest --output-on-failure
```

### Install Python Dependencies

```bash
cd frontend-python
pip install -r requirements.txt
```

## Running the Application

### 1. Start the Exchange Server

```bash
./backend-c/build/exchange_server --port 8080
```

Expected output:
```
=== Stock Exchange Engine ===
Initializing...
Added ticker: AAPL @ $150.00
Added ticker: MSFT @ $380.00
Added ticker: GOOGL @ $140.00
Added ticker: AMZN @ $180.00
Added ticker: TSLA @ $250.00
Server listening on port 8080
Simulation thread started
Network thread started
```

### 2. Launch the Trading Dashboard

```bash
cd frontend-python
python main.py
```

## API Protocol

### Message Types

| Type | Direction | Description |
|------|-----------|-------------|
| `ORDER_NEW` | Client → Server | Submit new order |
| `ORDER_CANCEL` | Client → Server | Cancel existing order |
| `MARKET_DATA` | Server → Client | Price/volume updates |
| `EXECUTION_REPORT` | Server → Client | Order fill notification |
| `ERROR` | Server → Client | Error message |

### Example Messages

**Submit Order:**
```json
{"type":"ORDER_NEW","ticker":"AAPL","side":"BUY","type":"LIMIT","price":150.00,"quantity":100}
```

**Execution Report:**
```json
{"type":"EXECUTION_REPORT","order_id":1,"match_id":2,"price":150.00,"quantity":100,"status":"FILLED","timestamp":1234567890}
```

## Testing

### Unit Tests (C)
- `test_order_book.c`: Order insertion, deletion, price-time priority
- `test_matching.c`: Order execution, partial fills, crossing logic
- `test_gbm.c`: GBM statistical properties verification
- `test_protocol.c`: JSON serialization/deserialization

### Integration Tests
- Full loop: Python client → C server → execution report → client

## License

MIT License - See LICENSE file for details.
