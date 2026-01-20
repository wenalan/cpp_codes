#include <algorithm>
#include <array>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

namespace {

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

void add_fd(int epoll_fd, int fd) {
    epoll_event ev{};
    ev.events = EPOLLIN | EPOLLRDHUP;
    ev.data.fd = fd;
    if (::epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev) < 0) {
        throw std::runtime_error(std::string("epoll_ctl ADD failed: ") + std::strerror(errno));
    }
}

void remove_fd(int epoll_fd, int fd) {
    ::epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, nullptr);
    ::close(fd);
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
        int epoll_fd = ::epoll_create1(0);
        if (epoll_fd < 0) {
            throw std::runtime_error(std::string("epoll_create1 failed: ") + std::strerror(errno));
        }

        add_fd(epoll_fd, listen_fd);

        std::vector<epoll_event> events(64);
        std::cout << "Epoll server listening on port " << port << std::endl;

        while (true) {
            int ready = ::epoll_wait(epoll_fd, events.data(), static_cast<int>(events.size()), -1);
            if (ready < 0) {
                if (errno == EINTR) {
                    continue;
                }
                throw std::runtime_error(std::string("epoll_wait failed: ") + std::strerror(errno));
            }

            if (ready == static_cast<int>(events.size())) {
                events.resize(events.size() * 2);
            }

            for (int i = 0; i < ready; ++i) {
                const epoll_event& ev = events[i];
                int fd = ev.data.fd;

                if (fd == listen_fd) {
                    while (true) {
                        sockaddr_in client_addr{};
                        socklen_t len = sizeof(client_addr);
                        int client_fd = ::accept(listen_fd, reinterpret_cast<sockaddr*>(&client_addr), &len);
                        if (client_fd < 0) {
                            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                                break;
                            }
                            if (errno == EINTR) {
                                continue;
                            }
                            std::cerr << "accept failed: " << std::strerror(errno) << std::endl;
                            break;
                        }

                        try {
                            set_nonblocking(client_fd);
                            add_fd(epoll_fd, client_fd);
                        } catch (const std::exception& ex) {
                            std::cerr << ex.what() << std::endl;
                            ::close(client_fd);
                            continue;
                        }

                        char ip[INET_ADDRSTRLEN] = {};
                        ::inet_ntop(AF_INET, &client_addr.sin_addr, ip, sizeof(ip));
                        std::cout << "Client connected: " << ip << ":" << ntohs(client_addr.sin_port) << std::endl;
                    }
                } else {
                    bool close_client = false;
                    if (ev.events & (EPOLLERR | EPOLLRDHUP | EPOLLHUP)) {
                        close_client = true;
                    } else {
                        std::array<char, 4096> buffer{};
                        while (true) {
                            ssize_t received = ::recv(fd, buffer.data(), buffer.size(), 0);
                            if (received > 0) {
                                std::cout.write(buffer.data(), received);
                                std::cout.flush();
                            } else if (received == 0) {
                                close_client = true;
                                break;
                            } else if (errno == EINTR) {
                                continue;
                            } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
                                break;
                            } else {
                                std::cerr << "recv failed: " << std::strerror(errno) << std::endl;
                                close_client = true;
                                break;
                            }
                        }
                    }

                    if (close_client) {
                        remove_fd(epoll_fd, fd);
                    }
                }
            }
        }

        ::close(epoll_fd);
        ::close(listen_fd);
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
