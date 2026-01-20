#include "order_book.h"

namespace hft {

void OrderBook::apply_deltas(const std::vector<PriceLevel>& bids, const std::vector<PriceLevel>& asks, std::int64_t update_id) {
  for (const auto& lvl : bids) {
    if (lvl.qty == 0.0) {
      bids_.erase(lvl.price);
    } else {
      bids_[lvl.price] = lvl.qty;
    }
  }
  for (const auto& lvl : asks) {
    if (lvl.qty == 0.0) {
      asks_.erase(lvl.price);
    } else {
      asks_[lvl.price] = lvl.qty;
    }
  }
  last_update_id_ = update_id;
}

std::optional<BestBidAsk> OrderBook::best() const {
  if (bids_.empty() || asks_.empty()) {
    return std::nullopt;
  }
  const auto best_bid = *bids_.begin();
  const auto best_ask = *asks_.begin();
  return BestBidAsk{best_bid.first, best_bid.second, best_ask.first, best_ask.second, last_update_id_};
}

}  // namespace hft
