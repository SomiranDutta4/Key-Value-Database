#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <string>

#include <sys/socket.h>
#include <unistd.h>

#include "forgedb/protocol.hpp"

namespace {

constexpr int PORT = 8080;
constexpr const char* HOST = "127.0.0.1";

bool send_all(
    int fd,
    const std::string& data
) {
    std::size_t offset = 0;

    while (offset < data.size()) {
        ssize_t bytes_written =
            ::send(
                fd,
                data.data() + offset,
                data.size() - offset,
                0
            );

        if (bytes_written > 0) {
            offset +=
                static_cast<std::size_t>(
                    bytes_written
                );

            continue;
        }

        if (bytes_written < 0 &&
            errno == EINTR) {
            continue;
        }

        return false;
    }

    return true;
}

bool receive_response(
    int fd,
    std::string& response
) {
    std::string input_buffer;

    while (true) {
        if (forgedb::try_extract_frame(
                input_buffer,
                response
            )) {
            return true;
        }

        char buffer[4096];

        ssize_t bytes_read =
            ::recv(
                fd,
                buffer,
                sizeof(buffer),
                0
            );

        if (bytes_read > 0) {
            input_buffer.append(
                buffer,
                static_cast<std::size_t>(
                    bytes_read
                )
            );

            continue;
        }

        if (bytes_read == 0) {
            return false;
        }

        if (errno == EINTR) {
            continue;
        }

        return false;
    }
}

int connect_to_server() {
    int fd =
        ::socket(
            AF_INET,
            SOCK_STREAM,
            0
        );

    if (fd < 0) {
        std::cerr
            << "Failed to create socket: "
            << std::strerror(errno)
            << "\n";

        return -1;
    }

    sockaddr_in server_address{};

    server_address.sin_family = AF_INET;
    server_address.sin_port =
        htons(PORT);

    if (::inet_pton(
            AF_INET,
            HOST,
            &server_address.sin_addr
        ) != 1) {

        std::cerr
            << "Invalid server address\n";

        ::close(fd);
        return -1;
    }

    if (::connect(
            fd,
            reinterpret_cast<sockaddr*>(
                &server_address
            ),
            sizeof(server_address)
        ) < 0) {

        std::cerr
            << "Failed to connect to ForgeDB: "
            << std::strerror(errno)
            << "\n";

        ::close(fd);
        return -1;
    }

    return fd;
}

} // namespace

namespace forgedb {

int run_cli() {
    int fd = connect_to_server();

    if (fd < 0) {
        return 1;
    }

    std::cout
        << "Connected to ForgeDB\n";

    std::cout
        << "Commands:\n"
        << "  PUT <key> <value>\n"
        << "  GET <key>\n"
        << "  DEL <key>\n"
        << "  EXIT\n\n";

    std::string command;

    while (true) {
        std::cout << "forgedb> ";

        if (!std::getline(
                std::cin,
                command
            )) {
            break;
        }

        if (command.empty()) {
            continue;
        }

        if (command == "EXIT" ||
            command == "exit") {

            break;
        }

        std::string frame =
            create_frame(command);

        if (frame.empty() &&
            !command.empty()) {

            std::cerr
                << "Command is too large\n";

            continue;
        }

        if (!send_all(
                fd,
                frame
            )) {

            std::cerr
                << "Failed to send command\n";

            break;
        }

        std::string response;

        if (!receive_response(
                fd,
                response
            )) {

            std::cerr
                << "Failed to receive response\n";

            break;
        }

        std::cout
            << response
            << "\n";
    }

    ::close(fd);

    std::cout
        << "Disconnected from ForgeDB\n";

    return 0;
}

} // namespace forgedb
