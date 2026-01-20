#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <cstring>
#include <functional>
#include <iostream>
#include <map>
#include <optional>
#include <random>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "hft_common.h"
#include "protocol.h"
#include "spsc_ring.h"

namespace hft {

constexpr int kRingDepth = 1024;
constexpr const char* kSymbol = "BTCUSDT";

class OrderBook {
public:
    void apply(const BookDelta& delta) {
        for (const auto& lvl : delta.levels) {
            auto& book = lvl.side == Side::Buy ? bids_ : asks_;
            if (lvl.qty == 0) {
                book.erase(lvl.px);
            } else {
                book[lvl.px] = lvl.qty;
            }
        }
    }

    std::optional<double> mid() const {
        auto best_bid = best_bid_px();
        auto best_ask = best_ask_px();
        if (!best_bid || !best_ask) return std::nullopt;
        return (static_cast<double>(*best_bid) + static_cast<double>(*best_ask)) / 2.0;
    }

    std::optional<int64_t> spread() const {
        auto bid = best_bid_px();
        auto ask = best_ask_px();
        if (!bid || !ask) return std::nullopt;
        return *ask - *bid;
    }

private:
    std::optional<int64_t> best_bid_px() const {
        if (bids_.empty()) return std::nullopt;
        return bids_.rbegin()->first;
    }
    std::optional<int64_t> best_ask_px() const {
        if (asks_.empty()) return std::nullopt;
        return asks_.begin()->first;
    }

    std::map<int64_t, int64_t> bids_;  // descending via reverse iteration
    std::map<int64_t, int64_t> asks_;  // ascending
};

struct StrategyConfig {
    double z_enter = 1.5;
    double z_exit = 0.2;
    double tp_ticks = 150;
    double sl_ticks = -150;
    int64_t pos_limit = 1;
    int64_t tick_size = 10;
};

class Strategy {
public:
    Strategy(const StrategyConfig& cfg, std::function<bool(const OrderRequest&)> send_fn)
        : cfg_(cfg), send_order_(std::move(send_fn)) {}

    void on_book(const BookDelta& delta, OrderBook& ob, Telemetry& tele) {
        ScopedTimer t("strategy_total", [&](const std::string& name, int64_t ns) { tele.record(name, ns); });
        ob.apply(delta);
        auto mid_opt = ob.mid();
        auto spread_opt = ob.spread();
        if (!mid_opt || !spread_opt) return;

        double mid = *mid_opt;
        stats_.add(mid);
        double z = 0.0;
        double stdev = stats_.stddev();
        if (stdev > 0.0) {
            z = (mid - stats_.mean) / stdev;
        }

        strategy_decision(delta.md_event_id, mid, z, *spread_opt, tele);
        drain_execs(tele);
    }

    void on_exec(const ExecUpdate& exec) {
        pending_execs_.push_back(exec);
    }

private:
    void strategy_decision(uint64_t md_event_id, double mid, double z, int64_t spread, Telemetry& tele) {
        ScopedTimer decision_timer("strategy_decision", [&](const std::string& n, int64_t ns) { tele.record(n, ns); });
        const int64_t px = static_cast<int64_t>(mid);

        // Exit conditions
        if (position_ != 0) {
            double unrealized = (position_ > 0 ? (mid - avg_px_) : (avg_px_ - mid)) * std::abs(position_);
            bool hit_tp = unrealized >= cfg_.tp_ticks;
            bool hit_sl = unrealized <= cfg_.sl_ticks;
            bool revert = std::abs(z) < cfg_.z_exit;
            if (hit_tp || hit_sl || revert) {
                send_close(md_event_id, px, z);
                return;
            }
        }

        // Entry conditions
        if (active_order_) return;
        if (std::abs(z) < cfg_.z_enter) return;
        if (spread > cfg_.tick_size * 5) return;
        if (std::abs(position_) >= cfg_.pos_limit) return;

        OrderRequest req;
        req.req_id = md_event_id;
        req.md_event_id = md_event_id;
        req.side = z > 0 ? Side::Sell : Side::Buy;
        req.px = px;
        req.qty = 1;
        req.signal_z = z;
        if (send_order_(req)) {
            active_order_ = true;
            last_req_id_ = req.req_id;
        }
    }

    void send_close(uint64_t md_event_id, int64_t px, double z) {
        if (active_order_) return;
        OrderRequest req;
        req.req_id = md_event_id;
        req.md_event_id = md_event_id;
        req.side = position_ > 0 ? Side::Sell : Side::Buy;
        req.px = px;
        req.qty = std::abs(position_);
        req.signal_z = z;
        if (send_order_(req)) {
            active_order_ = true;
            last_req_id_ = req.req_id;
        }
    }

    void drain_execs(Telemetry& tele) {
        ScopedTimer timer("strategy_exec_drain", [&](const std::string& n, int64_t ns) { tele.record(n, ns); });
        if (pending_execs_.empty()) return;
        for (auto& ex : pending_execs_) {
            if (ex.exec_type == ExecType::Ack) {
                active_order_ = false;
            } else if (ex.exec_type == ExecType::Fill || ex.exec_type == ExecType::PartialFill) {
                apply_fill(ex);
                active_order_ = false;
            } else if (ex.exec_type == ExecType::Reject || ex.exec_type == ExecType::Cancel) {
                active_order_ = false;
            }
        }
        pending_execs_.clear();
    }

    void apply_fill(const ExecUpdate& ex) {
        const int64_t signed_qty = (side_for_cloid(ex.cl_ord_id) == Side::Buy) ? ex.fill_qty : -ex.fill_qty;
        int64_t new_pos = position_ + signed_qty;
        if ((position_ > 0 && signed_qty > 0) || (position_ < 0 && signed_qty < 0)) {
            // adding to position: update avg
            avg_px_ = ((avg_px_ * std::abs(position_)) + (ex.fill_px * std::abs(signed_qty))) /
                      static_cast<double>(std::abs(new_pos));
        } else if (new_pos == 0) {
            realized_pnl_ += (position_ > 0 ? (ex.fill_px - avg_px_) : (avg_px_ - ex.fill_px)) * std::abs(position_);
            avg_px_ = 0.0;
        }
        position_ = new_pos;
    }

    Side side_for_cloid(uint64_t clordid) const {
        // Parity to determine side (even -> buy, odd -> sell) purely for demo.
        return (clordid % 2 == 0) ? Side::Buy : Side::Sell;
    }

    StrategyConfig cfg_;
    RollingStats stats_;
    std::function<bool(const OrderRequest&)> send_order_;
    int64_t position_ = 0;
    double avg_px_ = 0.0;
    double realized_pnl_ = 0.0;
    bool active_order_ = false;
    uint64_t last_req_id_ = 0;
    std::vector<ExecUpdate> pending_execs_;
};

class MarketDataGenerator {
public:
    MarketDataGenerator(int64_t px, int64_t tick) : px_(px), tick_(tick), rng_(42), dist_(-tick, tick) {}

    BookDelta next(uint64_t md_event_id) {
        px_ += dist_(rng_);
        BookDelta delta;
        delta.md_event_id = md_event_id;
        delta.symbol = kSymbol;
        delta.exch_update_id_begin = md_event_id * 2;
        delta.exch_update_id_end = delta.exch_update_id_begin + 1;
        delta.levels.push_back(LevelDelta{Side::Buy, px_ - tick_, 1});
        delta.levels.push_back(LevelDelta{Side::Sell, px_ + tick_, 1});
        return delta;
    }

private:
    int64_t px_;
    int64_t tick_;
    std::mt19937 rng_;
    std::uniform_int_distribution<int64_t> dist_;
};

class OmsEngine {
public:
    OmsEngine(SPSCRing<OrderRequest, kRingDepth>& in_ring,
              SPSCRing<ExecUpdate, kRingDepth>& out_ring,
              int eventfd_in,
              int eventfd_out)
        : inbound_(in_ring), outbound_(out_ring), eventfd_in_(eventfd_in), eventfd_out_(eventfd_out) {}

    void start() { thread_ = std::thread([this]() { run(); }); }

    void join() {
        running_ = false;
        if (thread_.joinable()) thread_.join();
    }

private:
    void run() {
        setup_socket();
        setup_epoll();
        loop();
        cleanup();
    }

    void setup_socket() {
        sock_fd_ = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
        if (sock_fd_ < 0) {
            perror("socket");
            return;
        }
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(kSimPort);
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        if (::connect(sock_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
            if (errno != EINPROGRESS) {
                perror("connect");
            }
        }
    }

    void setup_epoll() {
        epoll_fd_ = epoll_create1(0);
        if (epoll_fd_ < 0) {
            perror("epoll_create1");
            return;
        }
        epoll_event ev{};
        ev.events = EPOLLIN;
        ev.data.fd = eventfd_in_;
        epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, eventfd_in_, &ev);

        if (sock_fd_ >= 0) {
            epoll_event sev{};
            sev.events = EPOLLIN | EPOLLOUT | EPOLLET;
            sev.data.fd = sock_fd_;
            epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, sock_fd_, &sev);
        }
    }

    void loop() {
        constexpr int kMaxEvents = 8;
        epoll_event events[kMaxEvents];
        while (running_) {
            int nfds = epoll_wait(epoll_fd_, events, kMaxEvents, 50);
            if (nfds < 0) {
                if (errno == EINTR) continue;
                perror("epoll_wait");
                break;
            }
            for (int i = 0; i < nfds; ++i) {
                if (events[i].data.fd == eventfd_in_) {
                    handle_ring();
                } else if (events[i].data.fd == sock_fd_) {
                    if (events[i].events & EPOLLIN) handle_socket_read();
                }
            }
            // periodic drain of ring even without doorbell
            handle_ring();
        }
    }

    void handle_ring() {
        eventfd_t v;
        while (eventfd_read(eventfd_in_, &v) == 0) {
        }
        while (auto req = inbound_.pop()) {
            send_new_order(*req);
        }
    }

    void send_new_order(const OrderRequest& req) {
        WireNewOrder w;
        w.cl_ord_id = next_clordid_++;
        w.md_event_id = req.md_event_id;
        w.side = req.side == Side::Buy ? 0 : 1;
        w.ord_type = static_cast<uint8_t>(req.type);
        w.tif = static_cast<uint8_t>(req.tif);
        w.px = req.px;
        w.qty = req.qty;
        w.t_oms_send_ns = now_ns();
        orders_[w.cl_ord_id] = OrderState{req.md_event_id, req.side, req.px, req.qty, ExecType::Ack, 0};

        auto frame = pack_with_length(&w, sizeof(w));
        send_all(frame);
    }

    void handle_socket_read() {
        uint8_t buf[2048];
        for (;;) {
            ssize_t n = ::recv(sock_fd_, buf, sizeof(buf), 0);
            if (n < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) break;
                perror("recv");
                break;
            }
            if (n == 0) {
                std::cerr << "SimEx disconnected\n";
                running_ = false;
                break;
            }
            rx_buffer_.insert(rx_buffer_.end(), buf, buf + n);
            parse_exec_reports();
        }
    }

    void parse_exec_reports() {
        size_t offset = 0;
        std::vector<uint8_t> payload;
        while (unpack_frame(rx_buffer_, offset, payload)) {
            if (payload.size() < sizeof(WireExecReport)) continue;
            WireExecReport w{};
            std::memcpy(&w, payload.data(), sizeof(WireExecReport));
            if (w.hdr.magic != kProtocolMagic || w.hdr.msg_type != kMsgExecReport) continue;
            ExecUpdate ex{};
            ex.cl_ord_id = w.cl_ord_id;
            ex.md_event_id = w.md_event_id;
            ex.exec_type = static_cast<ExecType>(w.exec_type);
            ex.fill_px = w.fill_px;
            ex.fill_qty = w.fill_qty;
            ex.ts_oms_recv_ns = now_ns();
            outbound_.push(ex);
            eventfd_write(eventfd_out_, 1);
            if (auto it = orders_.find(w.cl_ord_id); it != orders_.end()) {
                it->second.state = ex.exec_type;
                it->second.filled += ex.fill_qty;
            }
        }
        rx_buffer_.erase(rx_buffer_.begin(), rx_buffer_.begin() + offset);
    }

    void send_all(const std::vector<uint8_t>& frame) {
        size_t sent = 0;
        while (sent < frame.size()) {
            ssize_t n = ::send(sock_fd_, frame.data() + sent, frame.size() - sent, 0);
            if (n < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    std::this_thread::sleep_for(std::chrono::microseconds(50));
                    continue;
                }
                perror("send");
                break;
            }
            sent += static_cast<size_t>(n);
        }
    }

    void cleanup() {
        if (sock_fd_ >= 0) ::close(sock_fd_);
        if (epoll_fd_ >= 0) ::close(epoll_fd_);
    }

    SPSCRing<OrderRequest, kRingDepth>& inbound_;
    SPSCRing<ExecUpdate, kRingDepth>& outbound_;
    int eventfd_in_;
    int eventfd_out_;
    int sock_fd_ = -1;
    int epoll_fd_ = -1;
    std::thread thread_;
    std::atomic<bool> running_{true};
    uint64_t next_clordid_ = 1;
    std::vector<uint8_t> rx_buffer_;
    std::unordered_map<uint64_t, OrderState> orders_;
};

void run_thread_a(SPSCRing<OrderRequest, kRingDepth>& a_to_b,
                  SPSCRing<ExecUpdate, kRingDepth>& b_to_a,
                  int eventfd_a_to_b,
                  int eventfd_b_to_a) {
    Telemetry telemetry;
    OrderBook ob;
    MarketDataGenerator md_gen(28'000'000, 50);
    StrategyConfig cfg;

    Strategy strat(cfg, [&](const OrderRequest& req) {
        if (a_to_b.push(req)) {
            eventfd_write(eventfd_a_to_b, 1);
            telemetry.record("strategy_send_order", 0);
            return true;
        }
        telemetry.record("strategy_send_order_ring_full", 0);
        return false;
    });

    constexpr int kEvents = 2000;
    for (int i = 0; i < kEvents; ++i) {
        uint64_t md_event_id = i + 1;
        {
            ScopedTimer t("md_total", [&](const std::string& n, int64_t ns) { telemetry.record(n, ns); });
            ScopedTimer read_t("md_read", [&](const std::string& n, int64_t ns) { telemetry.record(n, ns); });
        }
        {
            ScopedTimer parse_t("md_parse", [&](const std::string& n, int64_t ns) { telemetry.record(n, ns); });
        }
        BookDelta delta = md_gen.next(md_event_id);
        {
            ScopedTimer align_t("md_align", [&](const std::string& n, int64_t ns) { telemetry.record(n, ns); });
        }

        // Drain any exec updates before applying strategy decisions.
        eventfd_t v;
        while (eventfd_read(eventfd_b_to_a, &v) == 0) {
        }
        while (auto ex = b_to_a.pop()) {
            strat.on_exec(*ex);
        }

        strat.on_book(delta, ob, telemetry);
    }

    std::cout << "=== Telemetry ===\n" << telemetry.summary() << std::endl;
}

}  // namespace hft

int main() {
    using namespace hft;
    SPSCRing<OrderRequest, kRingDepth> a_to_b_ring;
    SPSCRing<ExecUpdate, kRingDepth> b_to_a_ring;

    int eventfd_a_to_b = eventfd(0, EFD_NONBLOCK | EFD_SEMAPHORE);
    int eventfd_b_to_a = eventfd(0, EFD_NONBLOCK | EFD_SEMAPHORE);
    if (eventfd_a_to_b < 0 || eventfd_b_to_a < 0) {
        perror("eventfd");
        return 1;
    }

    OmsEngine oms(a_to_b_ring, b_to_a_ring, eventfd_a_to_b, eventfd_b_to_a);
    oms.start();

    run_thread_a(a_to_b_ring, b_to_a_ring, eventfd_a_to_b, eventfd_b_to_a);

    oms.join();
    close(eventfd_a_to_b);
    close(eventfd_b_to_a);
    return 0;
}
