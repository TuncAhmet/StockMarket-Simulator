cmake_minimum_required(VERSION 3.16)
project(ExchangeEngine C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Options
option(BUILD_TESTS "Build unit tests" ON)

# Platform-specific settings
if(WIN32)
    add_definitions(-D_WIN32_WINNT=0x0601)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    add_definitions(-DCJSON_HIDE_SYMBOLS)
    set(PLATFORM_LIBS ws2_32)
else()
    set(PLATFORM_LIBS pthread m)
endif()

# Enable Unity double precision for tests
add_definitions(-DUNITY_INCLUDE_DOUBLE)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/lib/cjson)

# Source files
set(ENGINE_SOURCES
    src/order_book.c
    src/engine.c
    src/math_model.c
    src/market_maker.c
    src/network.c
    src/protocol.c
    lib/cjson/cJSON.c
)

# Main executable
add_executable(exchange_server
    src/main.c
    ${ENGINE_SOURCES}
)
target_link_libraries(exchange_server ${PLATFORM_LIBS})

# Tests
if(BUILD_TESTS)
    enable_testing()
    
    # Unity test framework
    set(UNITY_SOURCES tests/unity/unity.c)
    
    # Order Book tests
    add_executable(test_order_book
        tests/test_order_book.c
        src/order_book.c
        ${UNITY_SOURCES}
    )
    target_link_libraries(test_order_book ${PLATFORM_LIBS})
    add_test(NAME OrderBookTests COMMAND test_order_book)
    
    # Matching Engine tests
    add_executable(test_matching
        tests/test_matching.c
        src/order_book.c
        src/engine.c
        ${UNITY_SOURCES}
    )
    target_link_libraries(test_matching ${PLATFORM_LIBS})
    add_test(NAME MatchingTests COMMAND test_matching)
    
    # GBM tests
    add_executable(test_gbm
        tests/test_gbm.c
        src/math_model.c
        ${UNITY_SOURCES}
    )
    target_link_libraries(test_gbm ${PLATFORM_LIBS})
    add_test(NAME GBMTests COMMAND test_gbm)
    
    # Protocol tests
    add_executable(test_protocol
        tests/test_protocol.c
        src/protocol.c
        lib/cjson/cJSON.c
        ${UNITY_SOURCES}
    )
    target_link_libraries(test_protocol ${PLATFORM_LIBS})
    add_test(NAME ProtocolTests COMMAND test_protocol)
endif()
