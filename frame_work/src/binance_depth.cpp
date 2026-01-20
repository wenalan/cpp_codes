#include "binance_depth.h"

#include <algorithm>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/error.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/json.hpp>
#include <chrono>
#include <string_view>
#include <thread>

namespace hft {

namespace {
namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace ssl = boost::asio::ssl;
using tcp = boost::asio::ip::tcp;
}  // namespace


// snapshot and rest? sync?

BinanceDepthConnector::BinanceDepthConnector(std::atomic<bool>& running,
                                             std::string symbol,
                                             std::shared_ptr<MarketQueue> outbound,
                                             std::shared_ptr<LogQueue> log_queue)
    : running_(running),
      symbol_(std::move(symbol)),
      outbound_(std::move(outbound)),
      log_queue_(std::move(log_queue)) {
  std::transform(symbol_.begin(), symbol_.end(), symbol_.begin(), ::tolower);
}

void BinanceDepthConnector::run() {
#ifdef ENABLE_BINANCE
  try {
    const std::string host = "stream.binance.com";
    const std::string port = "9443";
    const std::string target = "/ws/" + symbol_ + "@depth5@100ms";

    boost::asio::io_context ioc;
    ssl::context ctx{ssl::context::tls_client};
    ctx.set_default_verify_paths();
    ctx.set_verify_mode(ssl::verify_peer);

    tcp::resolver resolver{ioc};
    auto const results = resolver.resolve(host, port);

    websocket::stream<ssl::stream<tcp::socket>> ws{ioc, ctx};
    beast::get_lowest_layer(ws).connect(results);
    ws.next_layer().handshake(ssl::stream_base::client);
    ws.set_option(websocket::stream_base::timeout::suggested(beast::role_type::client));
    ws.handshake(host, target);
    log("connected to " + target);

    beast::flat_buffer buffer;
    while (running_.load(std::memory_order_acquire)) {
      buffer.clear();
      ws.read(buffer);
      const std::string payload = beast::buffers_to_string(buffer.data());
      handle_message(payload);
    }

    beast::error_code ec;
    ws.close(websocket::close_code::normal, ec);
  } catch (const std::exception& ex) {
    log(std::string("binance error: ") + ex.what());
  }
#else
  log("Binance support is disabled at compile time");
  while (running_.load(std::memory_order_acquire)) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
#endif
}

void BinanceDepthConnector::handle_message(const std::string& payload) {
  try {
    auto jv = boost::json::parse(payload);
    const auto& obj = jv.as_object();
    const auto& bids = obj.at("b").as_array();
    const auto& asks = obj.at("a").as_array();
    const auto update_id = obj.at("u").as_int64();

    std::vector<PriceLevel> bid_lvls;
    std::vector<PriceLevel> ask_lvls;
    bid_lvls.reserve(bids.size());
    ask_lvls.reserve(asks.size());

    for (const auto& entry : bids) {
      const auto& arr = entry.as_array();
      const double px = std::stod(std::string(arr.at(0).as_string().c_str()));
      const double qty = std::stod(std::string(arr.at(1).as_string().c_str()));
      bid_lvls.push_back({px, qty});
    }
    for (const auto& entry : asks) {
      const auto& arr = entry.as_array();
      const double px = std::stod(std::string(arr.at(0).as_string().c_str()));
      const double qty = std::stod(std::string(arr.at(1).as_string().c_str()));
      ask_lvls.push_back({px, qty});
    }

    book_.apply_deltas(bid_lvls, ask_lvls, update_id);
    if (const auto best = book_.best()) {
      MarketEvent evt{symbol_, best->bid, best->ask, std::min(best->bid_qty, best->ask_qty), best->update_id};
      if (!outbound_->push(evt)) {
        log("drop market evt seq=" + std::to_string(best->update_id));
      }
    }
  } catch (const std::exception& ex) {
    log(std::string("parse error: ") + ex.what());
  }
}

void BinanceDepthConnector::log(const std::string& msg) {
  log_queue_->push(LogEvent{"binance", msg, std::chrono::steady_clock::now()});
}

}  // namespace hft
