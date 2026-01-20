#include <algorithm>
#include <array>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

namespace {

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

    return fd;
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
        fd_set master_set;
        fd_set read_set;
        FD_ZERO(&master_set);
        FD_SET(listen_fd, &master_set);
        int max_fd = listen_fd;

        std::cout << "Echo server listening on port " << port << std::endl;

        while (true) {
            read_set = master_set;
            int ready = ::select(max_fd + 1, &read_set, nullptr, nullptr, nullptr);
            if (ready < 0) {
                if (errno == EINTR) {
                    continue;
                }
                throw std::runtime_error(std::string("select failed: ") + std::strerror(errno));
            }

            for (int fd = 0; fd <= max_fd && ready > 0; ++fd) {
                if (!FD_ISSET(fd, &read_set)) {
                    continue;
                }
                --ready;

                if (fd == listen_fd) {
                    sockaddr_in client_addr{};
                    socklen_t client_len = sizeof(client_addr);
                    int client_fd = ::accept(listen_fd, reinterpret_cast<sockaddr*>(&client_addr), &client_len);
                    if (client_fd < 0) {
                        if (errno == EINTR) {
                            continue;
                        }
                        std::cerr << "accept failed: " << std::strerror(errno) << std::endl;
                        continue;
                    }

                    FD_SET(client_fd, &master_set);
                    max_fd = std::max(max_fd, client_fd);

                    char ip[INET_ADDRSTRLEN] = {};
                    ::inet_ntop(AF_INET, &client_addr.sin_addr, ip, sizeof(ip));
                    std::cout << "Client connected: " << ip << ":" << ntohs(client_addr.sin_port) << std::endl;
                } else {
                    std::array<char, 4096> buffer{};
                    ssize_t received = ::recv(fd, buffer.data(), buffer.size(), 0);
                    if (received <= 0) {
                        if (received < 0 && errno != EINTR) {
                            std::cerr << "recv failed: " << std::strerror(errno) << std::endl;
                        }
                        ::close(fd);
                        FD_CLR(fd, &master_set);
                    } else {
                        std::cout.write(buffer.data(), received);
                        std::cout.flush();
                    }
                }
            }
        }

        ::close(listen_fd);
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
