cmake_minimum_required(VERSION 3.20)
project(hft_demo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ENABLE_BINANCE "Enable Binance websocket connector" ON)

add_executable(hft_demo
  src/main.cpp
  src/feed_handler.cpp
  src/logging.cpp
  src/oms_risk.cpp
  src/strategy_shard.cpp
  src/trade_io.cpp
  src/order_book.cpp
)

if(ENABLE_BINANCE)
  find_package(Boost QUIET COMPONENTS system thread)
  find_package(OpenSSL QUIET)
  if(Boost_FOUND AND OpenSSL_FOUND)
    target_sources(hft_demo PRIVATE src/binance_depth.cpp)
    target_link_libraries(hft_demo PRIVATE Boost::system Boost::thread OpenSSL::SSL OpenSSL::Crypto)
    target_compile_definitions(hft_demo PRIVATE ENABLE_BINANCE)
  else()
    message(WARNING "Binance connector disabled (Boost/OpenSSL not found)")
  endif()
endif()

target_compile_options(hft_demo PRIVATE -Wall -Wextra -Wpedantic -O2)
target_link_libraries(hft_demo PRIVATE pthread)
