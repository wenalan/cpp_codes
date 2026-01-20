# hft_test

Prototype of a single-symbol trading loop with split threads (MarketData/Strategy vs. OMS) and a SimEx TCP process.

## Layout
- `hft_main`: main process with Thread A (market data + order book + strategy) and Thread B (OMS state machine + TCP client).
- `simex_server`: standalone TCP server that acks then fills orders with configurable delays to mimic exchange latency.
- `include/`: shared data types, SPSC ring, and wire protocol.

## Build
```
cmake -S hft_test -B hft_test/build
cmake --build hft_test/build
```

## Run
Start the exchange simulator first (optional args: `ack_delay_us` `fill_delay_us`):
```
./hft_test/build/simex_server 200 400
```

Run the main process in another shell:
```
./hft_test/build/hft_main
```

`hft_main`:
- Thread A runs a synthetic Binance-like book feed (diff + snapshot shape), applies to an order book, and triggers a simple z-score strategy with single active order constraint.
- Thread B owns OMS state, consumes A→B SPSC ring (doorbelled by eventfd), and talks to SimEx via TCP using a framed binary protocol.
- B→A exec updates flow over the return SPSC ring and eventfd for low-CPU wakeups.

Telemetry prints p50/p90/p99 (us) per stage placeholders for read/parse/align/strategy/queue/oms/tcp/sim.
