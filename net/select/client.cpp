#include <cerrno>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

#include <arpa/inet.h>
#include <netinet/in.h>
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

        std::string line;
        while (std::getline(std::cin, line)) {
            if (line == "exit") {
                break;
            }
            line.push_back('\n'); // mimic the newline the user typed
            send_all(sock, line);
        }

        ::close(sock);
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
