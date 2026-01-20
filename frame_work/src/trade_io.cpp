#include "trade_io.h"

#include <chrono>
#include <iomanip>
#include <sstream>
#include <thread>

namespace hft {

TradeIo::TradeIo(std::atomic<bool>& running, std::shared_ptr<OrderQueue> inbound, std::shared_ptr<LogQueue> log_queue)
    : running_(running), inbound_(std::move(inbound)), log_queue_(std::move(log_queue)) {}

void TradeIo::run() {
  while (running_.load(std::memory_order_acquire)) {
    OrderCommand cmd;
    if (!inbound_->pop(cmd)) {
      std::this_thread::sleep_for(std::chrono::microseconds(50));  // should be busy loop in production?
      continue;
    }
    log("send " + cmd.symbol + " " + (cmd.buy ? "BUY" : "SELL") + " qty=" + fmt(cmd.qty) +
        " px=" + fmt(cmd.price) + " seq=" + std::to_string(cmd.seq));
  }
}

std::string TradeIo::fmt(double v) {
  std::ostringstream oss;
  oss << std::fixed << std::setprecision(2) << v;
  return oss.str();
}

void TradeIo::log(const std::string& msg) {
  log_queue_->push(LogEvent{"trade", msg, std::chrono::steady_clock::now()});
}

}  // namespace hft
