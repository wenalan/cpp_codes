#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace hft {

struct PriceLevel {
  double price{};
  double qty{};
};

struct BestBidAsk {
  double bid{};   // why not use price level?
  double bid_qty{};
  double ask{};
  double ask_qty{};
  std::int64_t update_id{};
};

class OrderBook {
 public:
  void apply_deltas(const std::vector<PriceLevel>& bids, const std::vector<PriceLevel>& asks, std::int64_t update_id);
  std::optional<BestBidAsk> best() const;

 private:
  std::map<double, double, std::greater<>> bids_;  // price -> qty
  std::map<double, double, std::less<>> asks_;     // price -> qty
  std::int64_t last_update_id_{0};
};

}  // namespace hft
