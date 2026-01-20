#include <array>
#include <cerrno>
#include <csignal>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>

#include <arpa/inet.h>
#include <fcntl.h>
#include <liburing.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

namespace {

struct IoRequest {
    enum class Type { Accept, Read } type;
    int fd;
    sockaddr_in addr;
    socklen_t addr_len;
    std::array<char, 4096> buffer;
};

void set_nonblocking(int fd) {
    int flags = ::fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        throw std::runtime_error(std::string("fcntl(F_GETFL) failed: ") + std::strerror(errno));
    }
    if (::fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        throw std::runtime_error(std::string("fcntl(F_SETFL) failed: ") + std::strerror(errno));
    }
}

int create_listening_socket(int port) {
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        throw std::runtime_error(std::string("socket failed: ") + std::strerror(errno));
    }

    int opt = 1;
    if (::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        ::close(fd);
        throw std::runtime_error(std::string("setsockopt failed: ") + std::strerror(errno));
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(static_cast<uint16_t>(port));

    if (::bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        ::close(fd);
        throw std::runtime_error(std::string("bind failed: ") + std::strerror(errno));
    }

    if (::listen(fd, SOMAXCONN) < 0) {
        ::close(fd);
        throw std::runtime_error(std::string("listen failed: ") + std::strerror(errno));
    }

    set_nonblocking(fd);
    return fd;
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

void queue_accept(io_uring& ring, int listen_fd) {
    auto* req = new IoRequest{};
    req->type = IoRequest::Type::Accept;
    req->fd = listen_fd;
    req->addr_len = sizeof(req->addr);

    io_uring_sqe* sqe = get_sqe(ring);
    io_uring_prep_accept(sqe, listen_fd, reinterpret_cast<sockaddr*>(&req->addr), &req->addr_len, 0);
    io_uring_sqe_set_data(sqe, req);
}

void queue_read(io_uring& ring, int client_fd, IoRequest* req = nullptr) {
    IoRequest* target = req;
    if (!target) {
        target = new IoRequest{};
    }
    target->type = IoRequest::Type::Read;
    target->fd = client_fd;

    io_uring_sqe* sqe = get_sqe(ring);
    io_uring_prep_recv(sqe, client_fd, target->buffer.data(), target->buffer.size(), 0);
    io_uring_sqe_set_data(sqe, target);
}

void close_client(int fd) {
    if (fd >= 0) {
        ::close(fd);
    }
}

} // namespace

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <port>\n";
        return EXIT_FAILURE;
    }

    int port;
    try {
        port = std::stoi(argv[1]);
    } catch (...) {
        std::cerr << "Invalid port: " << argv[1] << "\n";
        return EXIT_FAILURE;
    }

    if (port <= 0 || port > 65535) {
        std::cerr << "Port must be between 1 and 65535\n";
        return EXIT_FAILURE;
    }

    try {
        int listen_fd = create_listening_socket(port);
        io_uring ring{};
        if (int ret = io_uring_queue_init(256, &ring, 0); ret < 0) {
            throw std::runtime_error(std::string("io_uring_queue_init failed: ") + std::strerror(-ret));
        }

        queue_accept(ring, listen_fd);
        std::cout << "io_uring server listening on port " << port << std::endl;

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

                if (req->type == IoRequest::Type::Accept) {
                    if (res >= 0) {
                        int client_fd = res;
                        set_nonblocking(client_fd);
                        char ip[INET_ADDRSTRLEN] = {};
                        ::inet_ntop(AF_INET, &req->addr.sin_addr, ip, sizeof(ip));
                        std::cout << "Client connected: " << ip << ":" << ntohs(req->addr.sin_port) << std::endl;
                        queue_read(ring, client_fd);
                    } else if (res != -EINTR) {
                        std::cerr << "accept failed: " << std::strerror(-res) << std::endl;
                    }

                    delete req;
                    queue_accept(ring, listen_fd);
                } else if (req->type == IoRequest::Type::Read) {
                    if (res > 0) {
                        std::cout.write(req->buffer.data(), res);
                        std::cout.flush();
                        queue_read(ring, req->fd, req);
                    } else {
                        if (res < 0 && res != -ECONNRESET) {
                            std::cerr << "recv failed: " << std::strerror(-res) << std::endl;
                        }
                        close_client(req->fd);
                        delete req;
                    }
                }

                io_uring_cqe_seen(&ring, cqe);
            }
        }

        io_uring_queue_exit(&ring);
        ::close(listen_fd);
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
