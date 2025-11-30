// md3.cpp — variant using RapidJSON + fewer copies to chase lower latency.
#include <boost/asio.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>

#include <rapidjson/document.h>
#include <rapidjson/error/en.h>

#include <algorithm>
#include <atomic>
#include <charconv>
#include <chrono>
#include <csignal>
#include <iomanip>
#include <iostream>
#include <map>
#include <string>
#include <thread>
#include <vector>

using tcp = boost::asio::ip::tcp;
namespace ssl = boost::asio::ssl;
namespace http = boost::beast::http;

struct OrderBook {
  std::map<double, double, std::greater<double>> bids;
  std::map<double, double, std::less<double>> asks;
};

using Clock = std::chrono::steady_clock;
inline long long to_us(Clock::duration d) {
  return std::chrono::duration_cast<std::chrono::microseconds>(d).count();
}
inline long long now_ms() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

constexpr std::size_t UPDATE_LIMIT = 100;

std::atomic<bool> g_stop{false};
void handle_signal(int) { g_stop.store(true); }

std::string to_lower(std::string s) {
  for (auto &c : s) c = std::tolower(c);
  return s;
}

template <typename Cmp>
void apply_deltas(std::map<double, double, Cmp> &side,
                  const std::vector<std::pair<double, double>> &deltas) {
  for (auto &[p, sz] : deltas) {
    if (sz == 0.0) {
      auto it = side.find(p);
      if (it != side.end()) side.erase(it);
    } else {
      side[p] = sz;
    }
  }
}

inline double parse_double(const rapidjson::Value &v) {
  if (v.IsNumber()) return v.GetDouble();
  if (v.IsString()) {
    auto s = v.GetString();
    auto len = v.GetStringLength();
    double out = 0.0;
    auto res = std::from_chars(s, s + len, out);
    if (res.ec == std::errc()) return out;
    return std::strtod(s, nullptr);
  }
  return 0.0;
}

bool fetch_snapshot(const std::string &symbol, OrderBook &out_book,
                    long long &last_update_id, std::string &err) {
  try {
    boost::asio::io_context ioc;
    ssl::context ctx{ssl::context::tls_client};
    ctx.set_default_verify_paths();

    tcp::resolver resolver{ioc};
    boost::beast::ssl_stream<boost::beast::tcp_stream> stream{ioc, ctx};

    const std::string host = "api.binance.com";
    const std::string port = "443";
    const std::string target =
        "/api/v3/depth?symbol=" + symbol + "&limit=1000";

    auto const results = resolver.resolve(host, port);
    boost::beast::get_lowest_layer(stream).connect(results);
    if (!SSL_set_tlsext_host_name(stream.native_handle(), host.c_str()))
      throw std::runtime_error("SNI set failed");
    stream.handshake(ssl::stream_base::client);

    http::request<http::string_body> req{http::verb::get, target, 11};
    req.set(http::field::host, host);
    req.set(http::field::user_agent, "beast");
    http::write(stream, req);

    boost::beast::flat_buffer buffer;
    http::response<http::string_body> res;
    http::read(stream, buffer, res);
    boost::system::error_code ec;
    stream.shutdown(ec);

    if (res.result() != http::status::ok) {
      err = "HTTP " + std::to_string(static_cast<int>(res.result()));
      return false;
    }

    rapidjson::Document d;
    d.Parse(res.body().c_str(), res.body().size());
    if (d.HasParseError()) {
      err = rapidjson::GetParseError_En(d.GetParseError());
      return false;
    }

    last_update_id = d["lastUpdateId"].GetInt64();
    out_book.bids.clear();
    out_book.asks.clear();

    const auto &bids = d["bids"];
    for (auto &lvl : bids.GetArray()) {
      double p = parse_double(lvl[0]);
      double s = parse_double(lvl[1]);
      if (s > 0) out_book.bids[p] = s;
    }
    const auto &asks = d["asks"];
    for (auto &lvl : asks.GetArray()) {
      double p = parse_double(lvl[0]);
      double s = parse_double(lvl[1]);
      if (s > 0) out_book.asks[p] = s;
    }
    return true;
  } catch (const std::exception &e) {
    err = e.what();
    return false;
  }
}

void apply_update_json(std::string_view payload, long long &last_update_id,
                       OrderBook &book) {
  rapidjson::Document d;
  d.Parse(payload.data(), payload.size());
  if (d.HasParseError()) throw std::runtime_error("parse error");

  long long U = d["U"].GetInt64();
  long long u = d["u"].GetInt64();
  long long pu = d.HasMember("pu") ? d["pu"].GetInt64() : 0LL;

  if (u <= last_update_id) return;
  bool bridged_window = (U <= last_update_id + 1 && last_update_id + 1 <= u);
  bool bridged_pu = (pu == last_update_id && pu != 0);
  if (!(bridged_window || bridged_pu))
    throw std::runtime_error("sequence gap; need resnapshot");

  std::vector<std::pair<double, double>> bid_d, ask_d;
  const auto &b = d["b"];
  const auto &a = d["a"];
  bid_d.reserve(b.Size());
  ask_d.reserve(a.Size());
  for (auto &lvl : b.GetArray()) {
    bid_d.emplace_back(parse_double(lvl[0]), parse_double(lvl[1]));
  }
  for (auto &lvl : a.GetArray()) {
    ask_d.emplace_back(parse_double(lvl[0]), parse_double(lvl[1]));
  }

  apply_deltas(book.bids, bid_d);
  apply_deltas(book.asks, ask_d);
  last_update_id = u;
}

void print_book(const OrderBook &book, std::size_t depth = 10,
                long long latency_us = -1, long long recv_ts_ms = -1) {
  auto _flags = std::cout.flags();
  auto _prec = std::cout.precision();
  std::cout.setf(std::ios::fixed);
  std::cout << std::setprecision(6);

  std::cout << "[BINANCE]";
  if (recv_ts_ms >= 0) std::cout << " ts_ms=" << recv_ts_ms;
  if (latency_us >= 0) std::cout << " proc_us=" << latency_us;
  std::cout << " top " << depth << " levels\n  Bids: ";
  std::size_t i = 0;
  for (auto &[p, s] : book.bids) {
    if (i++ >= depth) break;
    std::cout << p << "@" << s << "  ";
  }
  if (i == 0) std::cout << "(empty)";
  std::cout << "\n  Asks: ";
  i = 0;
  for (auto &[p, s] : book.asks) {
    if (i++ >= depth) break;
    std::cout << p << "@" << s << "  ";
  }
  if (i == 0) std::cout << "(empty)";
  std::cout << "\n" << std::endl;

  std::cout.flags(_flags);
  std::cout.precision(_prec);
}

long long percentile_us(std::vector<long long> &sorted, double pct) {
  if (sorted.empty()) return 0;
  auto idx = static_cast<std::size_t>(std::llround(
      (pct / 100.0) * (static_cast<double>(sorted.size() - 1))));
  return sorted[std::min(idx, sorted.size() - 1)];
}

void print_latency_stats(const std::vector<long long> &latencies) {
  if (latencies.empty()) {
    std::cout << "[STATS] no samples collected" << std::endl;
    return;
  }
  std::vector<long long> v = latencies;
  std::sort(v.begin(), v.end());
  long long min = v.front();
  long long max = v.back();
  long long p10 = percentile_us(v, 10.0);
  long long p50 = percentile_us(v, 50.0);
  long long p90 = percentile_us(v, 90.0);
  long long p99 = percentile_us(v, 99.0);
  std::cout << "[STATS] samples=" << v.size() << " min=" << min << "us"
            << " max=" << max << "us"
            << " p10=" << p10 << "us"
            << " p50=" << p50 << "us"
            << " p90=" << p90 << "us"
            << " p99=" << p99 << "us" << std::endl;
}

int main(int argc, char **argv) {
  std::ios::sync_with_stdio(false);
  std::cin.tie(nullptr);

  std::string symbol = "BTCUSDT";
  if (argc > 1 && argv[1]) symbol = argv[1];

  std::signal(SIGINT, handle_signal);
  std::signal(SIGTERM, handle_signal);

  std::cout << "Starting Binance depth stream for " << symbol
            << " (Ctrl+C to quit)" << std::endl;

  std::vector<long long> latencies;
  latencies.reserve(UPDATE_LIMIT);

  using namespace std::chrono_literals;
  while (!g_stop.load()) {
    try {
      OrderBook book;
      long long last_id = 0;

      // 1) Connect WS first
      boost::asio::io_context ioc;
      ssl::context ctx{ssl::context::tls_client};
      ctx.set_default_verify_paths();
      tcp::resolver resolver{ioc};
      boost::beast::websocket::stream<
          boost::beast::ssl_stream<boost::beast::tcp_stream>>
          ws{ioc, ctx};

      const std::string host = "stream.binance.com";
      const std::string port = "9443";
      const std::string path =
          "/ws/" + to_lower(symbol) + "@depth@100ms";

      auto const results = resolver.resolve(host, port);
      boost::beast::get_lowest_layer(ws).connect(results);
      if (!SSL_set_tlsext_host_name(ws.next_layer().native_handle(),
                                    host.c_str()))
        throw std::runtime_error("SNI set failed");
      ws.next_layer().handshake(ssl::stream_base::client);
      ws.handshake(host, path);

      // 2) REST snapshot
      std::string err;
      if (!fetch_snapshot(symbol, book, last_id, err)) {
        std::cerr << "[BINANCE] snapshot error: " << err << std::endl;
        boost::system::error_code ec;
        ws.close(boost::beast::websocket::close_code::normal, ec);
        std::this_thread::sleep_for(1s);
        continue;
      }

      // 3) Bridge buffered updates
      boost::beast::flat_buffer buffer;
      for (;;) {
        buffer.clear();
        ws.read(buffer);
        auto t_recv = Clock::now();
        auto recv_ms = now_ms();
        std::string_view payload_view(
            static_cast<const char *>(buffer.cdata().data()), buffer.size());
        try {
          auto before = last_id;
          apply_update_json(payload_view, last_id, book);
          if (last_id != before) {
            auto proc = to_us(Clock::now() - t_recv);
            latencies.push_back(proc);
            print_book(book, 10, proc, recv_ms);
            if (latencies.size() >= UPDATE_LIMIT) {
              print_latency_stats(latencies);
              g_stop.store(true);
              break;
            }
            break;
          }
        } catch (...) {
          continue;
        }
      }

      if (g_stop.load()) break;

      // 4) Steady-state
      for (;;) {
        if (g_stop.load()) break;
        buffer.clear();
        ws.read(buffer);
        auto t_recv = Clock::now();
        auto recv_ms = now_ms();
        std::string_view payload_view(
            static_cast<const char *>(buffer.cdata().data()), buffer.size());
        try {
          apply_update_json(payload_view, last_id, book);
          auto proc = to_us(Clock::now() - t_recv);
          latencies.push_back(proc);
          print_book(book, 10, proc, recv_ms);
          if (latencies.size() >= UPDATE_LIMIT) {
            print_latency_stats(latencies);
            g_stop.store(true);
            break;
          }
        } catch (const std::exception &e) {
          std::cerr << "[BINANCE] " << e.what() << " — resyncing..."
                    << std::endl;
          break;
        }
      }

      boost::system::error_code ec;
      ws.close(boost::beast::websocket::close_code::normal, ec);
    } catch (const std::exception &e) {
      if (g_stop.load()) break;
      std::cerr << "[BINANCE] exception: " << e.what() << std::endl;
    }
    std::this_thread::sleep_for(1s);
  }

  std::cout << "Stopped" << std::endl;
  return 0;
}
