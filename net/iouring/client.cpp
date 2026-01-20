#include <array>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

#include <arpa/inet.h>
#include <liburing.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

namespace {

struct IoRequest {
    enum class Type { ReadInput, SendSocket, PollSocket } type;
    int fd;
    std::array<char, 4096> buffer;
    size_t length = 0;
};

int connect_to_server(const std::string& host, int port) {
    int sock = ::socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        throw std::runtime_error(std::string("socket failed: ") + std::strerror(errno));
    }

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(static_cast<uint16_t>(port));
    if (::inet_pton(AF_INET, host.c_str(), &server_addr.sin_addr) <= 0) {
        ::close(sock);
        throw std::runtime_error(std::string("inet_pton failed: ") + std::strerror(errno));
    }

    if (::connect(sock, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr)) < 0) {
        ::close(sock);
        throw std::runtime_error(std::string("connect failed: ") + std::strerror(errno));
    }

    return sock;
}

io_uring_sqe* get_sqe(io_uring& ring) {
    io_uring_sqe* sqe = io_uring_get_sqe(&ring);
    while (!sqe) {
        int ret = io_uring_submit(&ring);
        if (ret < 0) {
            throw std::runtime_error(std::string("io_uring_submit failed: ") + std::strerror(-ret));
        }
        sqe = io_uring_get_sqe(&ring);
    }
    return sqe;
}

void submit_stdin_read(io_uring& ring, IoRequest* req) {
    req->type = IoRequest::Type::ReadInput;
    req->fd = STDIN_FILENO;
    io_uring_sqe* sqe = get_sqe(ring);
    io_uring_prep_read(sqe, STDIN_FILENO, req->buffer.data(), req->buffer.size(), -1);
    io_uring_sqe_set_data(sqe, req);
}

void submit_send(io_uring& ring, IoRequest* req, int sock) {
    req->type = IoRequest::Type::SendSocket;
    req->fd = sock;
    io_uring_sqe* sqe = get_sqe(ring);
    io_uring_prep_send(sqe, sock, req->buffer.data(), req->length, 0);
    io_uring_sqe_set_data(sqe, req);
}

void submit_poll(io_uring& ring, IoRequest* req, int sock) {
    req->type = IoRequest::Type::PollSocket;
    req->fd = sock;
    io_uring_sqe* sqe = get_sqe(ring);
    io_uring_prep_poll_add(sqe, sock, POLLERR | POLLHUP | POLLRDHUP);
    io_uring_sqe_set_data(sqe, req);
}

} // namespace

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <server_ip> <port>\n";
        return EXIT_FAILURE;
    }

    const std::string server_ip = argv[1];
    int port;
    try {
        port = std::stoi(argv[2]);
    } catch (...) {
        std::cerr << "Invalid port: " << argv[2] << "\n";
        return EXIT_FAILURE;
    }

    if (port <= 0 || port > 65535) {
        std::cerr << "Port must be between 1 and 65535\n";
        return EXIT_FAILURE;
    }

    try {
        int sock = connect_to_server(server_ip, port);
        io_uring ring{};
        if (int ret = io_uring_queue_init(128, &ring, 0); ret < 0) {
            ::close(sock);
            throw std::runtime_error(std::string("io_uring_queue_init failed: ") + std::strerror(-ret));
        }

        auto* read_req = new IoRequest{};
        submit_stdin_read(ring, read_req);

        auto* poll_req = new IoRequest{};
        submit_poll(ring, poll_req, sock);

        bool running = true;
        while (running) {
            int ret = io_uring_submit_and_wait(&ring, 1);
            if (ret < 0 && ret != -EINTR) {
                throw std::runtime_error(std::string("io_uring_submit_and_wait failed: ") + std::strerror(-ret));
            }

            io_uring_cqe* cqe = nullptr;
            while (io_uring_peek_cqe(&ring, &cqe) == 0) {
                auto* req = static_cast<IoRequest*>(io_uring_cqe_get_data(cqe));
                int res = cqe->res;

                if (!req) {
                    io_uring_cqe_seen(&ring, cqe);
                    continue;
                }

                switch (req->type) {
                case IoRequest::Type::ReadInput: {
                    if (res <= 0) {
                        running = false;
                        delete req;
                        break;
                    }

                    req->length = static_cast<size_t>(res);
                    std::string_view view{req->buffer.data(), req->length};
                    while (!view.empty() && (view.back() == '\n' || view.back() == '\r')) {
                        view.remove_suffix(1);
                    }
                    if (view == "exit") {
                        running = false;
                        delete req;
                        break;
                    }

                    submit_send(ring, req, sock);
                    break;
                }
                case IoRequest::Type::SendSocket: {
                    if (res < 0) {
                        std::cerr << "send failed: " << std::strerror(-res) << std::endl;
                        running = false;
                        delete req;
                        break;
                    }
                    submit_stdin_read(ring, req);
                    break;
                }
                case IoRequest::Type::PollSocket: {
                    if (res >= 0) {
                        std::cerr << "Server closed the connection" << std::endl;
                    } else {
                        std::cerr << "Poll error: " << std::strerror(-res) << std::endl;
                    }
                    running = false;
                    delete req;
                    break;
                }
                }

                io_uring_cqe_seen(&ring, cqe);
            }
        }

        io_uring_queue_exit(&ring);
        ::close(sock);
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
