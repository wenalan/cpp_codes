#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <cstring>
#include <iostream>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "hft_common.h"
#include "protocol.h"

using namespace hft;

class SimExServer {
public:
    SimExServer(int port, int ack_delay_us, int fill_delay_us)
        : port_(port), ack_delay_us_(ack_delay_us), fill_delay_us_(fill_delay_us) {}

    void run() {
        int listen_fd = ::socket(AF_INET, SOCK_STREAM, 0);
        if (listen_fd < 0) {
            perror("socket");
            return;
        }
        int opt = 1;
        setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        addr.sin_port = htons(port_);
        if (bind(listen_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
            perror("bind");
            close(listen_fd);
            return;
        }
        if (listen(listen_fd, 8) < 0) {
            perror("listen");
            close(listen_fd);
            return;
        }
        std::cout << "SimEx listening on " << port_ << " ack_delay_us=" << ack_delay_us_
                  << " fill_delay_us=" << fill_delay_us_ << std::endl;

        for (;;) {
            int client_fd = accept(listen_fd, nullptr, nullptr);
            if (client_fd < 0) {
                if (errno == EINTR) continue;
                perror("accept");
                break;
            }
            handle_client(client_fd);
            close(client_fd);
        }
        close(listen_fd);
    }

private:
    void handle_client(int fd) {
        std::cout << "Client connected\n";
        std::vector<uint8_t> buf;
        uint8_t tmp[2048];
        while (true) {
            ssize_t n = recv(fd, tmp, sizeof(tmp), 0);
            if (n < 0) {
                if (errno == EINTR) continue;
                perror("recv");
                break;
            }
            if (n == 0) break;
            buf.insert(buf.end(), tmp, tmp + n);
            parse_messages(fd, buf);
        }
        std::cout << "Client disconnected\n";
    }

    void parse_messages(int fd, std::vector<uint8_t>& buf) {
        size_t offset = 0;
        std::vector<uint8_t> payload;
        while (unpack_frame(buf, offset, payload)) {
            if (payload.size() < sizeof(WireNewOrder)) continue;
            WireNewOrder w{};
            std::memcpy(&w, payload.data(), sizeof(WireNewOrder));
            if (w.hdr.magic != kProtocolMagic || w.hdr.msg_type != kMsgNewOrder) continue;
            handle_new_order(fd, w);
        }
        buf.erase(buf.begin(), buf.begin() + offset);
    }

    void handle_new_order(int fd, const WireNewOrder& w) {
        int64_t recv_ts = now_ns();
        if (ack_delay_us_ > 0) std::this_thread::sleep_for(std::chrono::microseconds(ack_delay_us_));
        send_report(fd, w, ExecType::Ack, 0, 0, recv_ts);
        if (fill_delay_us_ > 0) std::this_thread::sleep_for(std::chrono::microseconds(fill_delay_us_));
        send_report(fd, w, ExecType::Fill, w.px, w.qty, recv_ts);
    }

    void send_report(int fd, const WireNewOrder& w, ExecType type, int64_t fill_px, int64_t fill_qty, int64_t t_recv) {
        WireExecReport rep{};
        rep.hdr.msg_type = kMsgExecReport;
        rep.hdr.magic = kProtocolMagic;
        rep.cl_ord_id = w.cl_ord_id;
        rep.md_event_id = w.md_event_id;
        rep.exec_type = static_cast<uint8_t>(type);
        rep.fill_px = fill_px;
        rep.fill_qty = fill_qty;
        rep.t_sim_recv_ns = t_recv;
        rep.t_sim_send_ns = now_ns();
        auto frame = pack_with_length(&rep, sizeof(rep));
        size_t sent = 0;
        while (sent < frame.size()) {
            ssize_t n = send(fd, frame.data() + sent, frame.size() - sent, 0);
            if (n < 0) {
                if (errno == EINTR) continue;
                perror("send");
                break;
            }
            sent += static_cast<size_t>(n);
        }
    }

    int port_;
    int ack_delay_us_;
    int fill_delay_us_;
};

int main(int argc, char* argv[]) {
    int ack_delay_us = 200;
    int fill_delay_us = 400;
    if (argc > 1) ack_delay_us = std::stoi(argv[1]);
    if (argc > 2) fill_delay_us = std::stoi(argv[2]);
    SimExServer server(kSimPort, ack_delay_us, fill_delay_us);
    server.run();
    return 0;
}
