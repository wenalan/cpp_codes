#include "logging.h"

#include <chrono>
#include <iostream>
#include <thread>

namespace hft {

Logger::Logger(std::atomic<bool>& running) : running_(running), start_(std::chrono::steady_clock::now()) {}

std::shared_ptr<LogQueue> Logger::register_source(const std::string& name) {
  sources_.push_back({name, std::make_shared<LogQueue>()});
  return sources_.back().queue;
}

void Logger::run() {
  while (running_.load(std::memory_order_acquire)) {
    drain_once();
    std::this_thread::sleep_for(std::chrono::microseconds(50));
  }
  drain_once(); // why extra?
}

void Logger::drain_once() {
  for (auto& src : sources_) {
    LogEvent evt;
    while (src.queue->pop(evt)) {
      const auto rel = std::chrono::duration_cast<std::chrono::milliseconds>(evt.ts - start_).count();
      std::cout << "[" << src.name << "] +" << rel << "ms " << evt.message << '\n';
    }
  }
}

}  // namespace hft
