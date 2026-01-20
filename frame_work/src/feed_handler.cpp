#include "feed_handler.h"

#include <chrono>
#include <random>
#include <thread>

namespace hft {

FeedHandler::FeedHandler(std::atomic<bool>& running,
                         std::vector<std::string> symbols,
                         std::vector<std::shared_ptr<MarketQueue>> shard_queues,
                         std::shared_ptr<LogQueue> log_queue)
    : running_(running),
      symbols_(std::move(symbols)),
      shard_queues_(std::move(shard_queues)),
      log_queue_(std::move(log_queue)) {}

void FeedHandler::run() {
  std::unordered_map<std::string, std::int64_t> seqs;
  std::mt19937_64 rng{std::random_device{}()};
  std::normal_distribution<double> price_noise{0.0, 0.5};
  while (running_.load(std::memory_order_acquire)) {
    for (const auto& sym : symbols_) {
      const auto shard = shard_for_symbol(sym);
      auto& q = shard_queues_[shard];
      auto& seq = seqs[sym];
      ++seq;
      MarketEvent evt{sym, 100.0 + price_noise(rng), 100.4 + price_noise(rng), 0.5, seq};
      if (!q->push(evt)) {
        log("drop market evt for " + sym);
      }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
}

std::size_t FeedHandler::shard_for_symbol(const std::string& sym) const {
  return std::hash<std::string>{}(sym) % shard_queues_.size();
}

void FeedHandler::log(const std::string& msg) {
  log_queue_->push(LogEvent{"feed", msg, std::chrono::steady_clock::now()});
}

}  // namespace hft
