#include <cerrno>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

namespace {

void send_all(int fd, std::string_view data) {
    size_t total_sent = 0;
    while (total_sent < data.size()) {
        ssize_t sent = ::send(fd, data.data() + total_sent, data.size() - total_sent, 0);
        if (sent < 0) {
            if (errno == EINTR) {
                continue;
            }
            throw std::runtime_error(std::string("send failed: ") + std::strerror(errno));
        }
        if (sent == 0) {
            throw std::runtime_error("connection closed while sending");
        }
        total_sent += static_cast<size_t>(sent);
    }
}

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

void add_fd(int epoll_fd, int fd, uint32_t events) {
    epoll_event ev{};
    ev.events = events;
    ev.data.fd = fd;
    if (::epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev) < 0) {
        throw std::runtime_error(std::string("epoll_ctl ADD failed: ") + std::strerror(errno));
    }
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
        int epoll_fd = ::epoll_create1(0);
        if (epoll_fd < 0) {
            ::close(sock);
            throw std::runtime_error(std::string("epoll_create1 failed: ") + std::strerror(errno));
        }

        add_fd(epoll_fd, STDIN_FILENO, EPOLLIN);
        add_fd(epoll_fd, sock, EPOLLRDHUP | EPOLLHUP | EPOLLERR);

        std::vector<epoll_event> events(4);
        bool running = true;

        while (running) {
            int ready = ::epoll_wait(epoll_fd, events.data(), static_cast<int>(events.size()), -1);
            if (ready < 0) {
                if (errno == EINTR) {
                    continue;
                }
                throw std::runtime_error(std::string("epoll_wait failed: ") + std::strerror(errno));
            }

            for (int i = 0; i < ready; ++i) {
                const epoll_event& ev = events[i];
                if (ev.data.fd == STDIN_FILENO) {
                    if (!(ev.events & EPOLLIN)) {
                        continue;
                    }

                    std::string line;
                    if (!std::getline(std::cin, line)) {
                        running = false;
                        break;
                    }
                    if (line == "exit") {
                        running = false;
                        break;
                    }
                    line.push_back('\n');
                    send_all(sock, line);
                } else if (ev.data.fd == sock) {
                    std::cerr << "Server closed the connection" << std::endl;
                    running = false;
                    break;
                }
            }
        }

        ::close(sock);
        ::close(epoll_fd);
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
