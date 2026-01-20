#include <atomic>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "binance_depth.h"
#include "feed_handler.h"
#include "logging.h"
#include "oms_risk.h"
#include "strategy_shard.h"
#include "trade_io.h"
#include "types.h"

int main(int argc, char* argv[]) {
  using namespace std::chrono_literals;
  std::atomic<bool> running{true};

  hft::Logger logger(running);
  auto log_feed = logger.register_source("feed");
  auto log_binance = logger.register_source("binance");
  auto log_strat0 = logger.register_source("strat0");
  auto log_strat1 = logger.register_source("strat1");
  auto log_oms = logger.register_source("oms");
  auto log_trade = logger.register_source("trade");

  std::thread log_thread([&]() { logger.run(); });

  auto feed_to_strat0 = std::make_shared<hft::MarketQueue>();
  auto feed_to_strat1 = std::make_shared<hft::MarketQueue>();
  auto strat0_to_oms = std::make_shared<hft::StrategyQueue>();
  auto strat1_to_oms = std::make_shared<hft::StrategyQueue>();
  auto oms_to_trade = std::make_shared<hft::OrderQueue>();

  std::vector<std::string> symbols = {"BTCUSDT", "ETHUSDT", "XRPUSDT", "SOLUSDT"};
  std::vector<std::shared_ptr<hft::MarketQueue>> shard_market_queues{feed_to_strat0, feed_to_strat1};
  hft::FeedHandler feed(running, symbols, shard_market_queues, log_feed);

  hft::StrategyShard strat0(running, "strat0", feed_to_strat0, strat0_to_oms, log_strat0);
  hft::StrategyShard strat1(running, "strat1", feed_to_strat1, strat1_to_oms, log_strat1);

  std::vector<std::shared_ptr<hft::StrategyQueue>> strat_outputs{strat0_to_oms, strat1_to_oms};
  hft::OmsRisk oms(running, strat_outputs, oms_to_trade, log_oms);
  hft::TradeIo trade(running, oms_to_trade, log_trade);

  bool use_binance = false;
  std::string binance_symbol = "btcusdt";
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--binance" && i + 1 < argc) {
      use_binance = true;
      binance_symbol = argv[++i];
    }
  }

  std::thread feed_thread;
  std::unique_ptr<hft::BinanceDepthConnector> binance;
  if (use_binance) {
#ifdef ENABLE_BINANCE
    binance = std::make_unique<hft::BinanceDepthConnector>(running, binance_symbol, feed_to_strat0, log_binance);
    feed_thread = std::thread([&]() { binance->run(); });
#else
    std::cerr << "Binance support not built; rebuild with ENABLE_BINANCE\n";
    feed_thread = std::thread([&]() { feed.run(); });
#endif
  } else {
    feed_thread = std::thread([&]() { feed.run(); });
  }
  std::thread strat_thread0([&]() { strat0.run(); });
  std::thread strat_thread1([&]() { strat1.run(); });
  std::thread oms_thread([&]() { oms.run(); });
  std::thread trade_thread([&]() { trade.run(); });

  std::this_thread::sleep_for(2s);
  running.store(false, std::memory_order_release);

  feed_thread.join();
  strat_thread0.join();
  strat_thread1.join();
  oms_thread.join();
  trade_thread.join();
  log_thread.join();

  return 0;
}
