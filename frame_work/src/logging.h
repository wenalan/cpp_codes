#pragma once

#include <atomic>
#include <memory>
#include <string>
#include <vector>

#include "types.h"

namespace hft {

class Logger {
 public:
  explicit Logger(std::atomic<bool>& running);

  std::shared_ptr<LogQueue> register_source(const std::string& name);
  void run();

 private:
  struct SourceQueue {
    std::string name;
    std::shared_ptr<LogQueue> queue;
  };

  void drain_once();

  std::atomic<bool>& running_;
  std::chrono::steady_clock::time_point start_;
  std::vector<SourceQueue> sources_;
};

}  // namespace hft
