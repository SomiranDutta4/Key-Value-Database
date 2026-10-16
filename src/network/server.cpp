#include "forgedb/server.hpp"

#include <cerrno>
#include <cstring>
#include <iostream>
#include <string>
#include <utility>

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include "forgedb/command.hpp"
#include "forgedb/database.hpp"
#include "forgedb/parser.hpp"

namespace forgedb {

Server::Server(
    Database& database,
    int port
)
    : database_(database),
      port_(port) {
}

Server::~Server() {
    stop();
}

bool Server::initialize(
    std::string& error
) {
    if (!create_listening_socket(error)) {
        return false;
    }

    if (!set_non_blocking(
            server_fd_,
            error
        )) {

        ::close(server_fd_);
        server_fd_ = -1;

        return false;
    }

    bool added =
        event_loop_.add_read(
            server_fd_,
            [this](int) {
                handle_accept();
            }
        );

    if (!added) {
        error =
            "Failed to register listening socket "
            "with event loop";

        ::close(server_fd_);
        server_fd_ = -1;

        return false;
    }

    return true;
}

bool Server::create_listening_socket(
    std::string& error
) {
    server_fd_ = ::socket(
        AF_INET,
        SOCK_STREAM,
        0
    );

    if (server_fd_ < 0) {
        error =
            std::string("socket() failed: ") +
            std::strerror(errno);

        return false;
    }

    int opt = 1;

    if (::setsockopt(
            server_fd_,
            SOL_SOCKET,
            SO_REUSEADDR,
            &opt,
            sizeof(opt)
        ) < 0) {

        error =
            std::string(
                "setsockopt() failed: "
            ) +
            std::strerror(errno);

        ::close(server_fd_);
        server_fd_ = -1;

        return false;
    }

    sockaddr_in address{};

    address.sin_family = AF_INET;
    address.sin_addr.s_addr =
        htonl(INADDR_ANY);

    address.sin_port =
        htons(
            static_cast<uint16_t>(
                port_
            )
        );

    if (::bind(
            server_fd_,
            reinterpret_cast<sockaddr*>(
                &address
            ),
            sizeof(address)
        ) < 0) {

        error =
            std::string("bind() failed: ") +
            std::strerror(errno);

        ::close(server_fd_);
        server_fd_ = -1;

        return false;
    }

    constexpr int BACKLOG = 128;

    if (::listen(
            server_fd_,
            BACKLOG
        ) < 0) {

        error =
            std::string("listen() failed: ") +
            std::strerror(errno);

        ::close(server_fd_);
        server_fd_ = -1;

        return false;
    }

    return true;
}

bool Server::set_non_blocking(
    int fd,
    std::string& error
) {
    int flags =
        ::fcntl(
            fd,
            F_GETFL,
            0
        );

    if (flags < 0) {
        error =
            std::string(
                "fcntl(F_GETFL) failed: "
            ) +
            std::strerror(errno);

        return false;
    }

    if (::fcntl(
            fd,
            F_SETFL,
            flags | O_NONBLOCK
        ) < 0) {

        error =
            std::string(
                "fcntl(F_SETFL) failed: "
            ) +
            std::strerror(errno);

        return false;
    }

    return true;
}

void Server::handle_accept() {
    while (true) {
        sockaddr_in client_address{};
        socklen_t client_length =
            sizeof(client_address);

        int client_fd =
            ::accept(
                server_fd_,
                reinterpret_cast<sockaddr*>(
                    &client_address
                ),
                &client_length
            );

        if (client_fd >= 0) {
            std::string error;

            if (!set_non_blocking(
                    client_fd,
                    error
                )) {

                std::cerr
                    << "Failed to make client "
                    << "socket non-blocking: "
                    << error
                    << "\n";

                ::close(client_fd);
                continue;
            }

            auto connection =
                std::make_unique<Connection>(
                    client_fd,
                    event_loop_,

                    /*
                     * Complete request
                     *      ↓
                     * Parse command
                     *      ↓
                     * Execute database operation
                     *      ↓
                     * Return response
                     */
                    [this](
                        const std::string& message
                    ) -> std::string {

                        Command command =
                            parse_command(message);

                        return database_.execute(
                            command
                        );
                    },

                    /*
                     * Connection is closed.
                     * Remove its ownership from Server.
                     */
                    [this](int fd) {
                        remove_connection(fd);
                    }
                );

            connections_.emplace(
                client_fd,
                std::move(connection)
            );

            auto it =
                connections_.find(
                    client_fd
                );

            if (it != connections_.end()) {
                it->second->start();
            }

            std::cout
                << "Client connected. fd="
                << client_fd
                << "\n";

            continue;
        }

        if (errno == EAGAIN ||
            errno == EWOULDBLOCK) {

            /*
             * All pending connections have been
             * accepted.
             */
            return;
        }

        if (errno == EINTR) {
            continue;
        }

        std::cerr
            << "accept() failed: "
            << std::strerror(errno)
            << "\n";

        return;
    }
}

void Server::remove_connection(
    int fd
) {
    auto it =
        connections_.find(fd);

    if (it == connections_.end()) {
        return;
    }

    std::cout
        << "Client disconnected. fd="
        << fd
        << "\n";

    connections_.erase(it);
}

void Server::run() {
    std::cout
        << "ForgeDB listening on port "
        << port_
        << "\n";

    event_loop_.run();
}

void Server::stop() {
    event_loop_.stop();

    connections_.clear();

    if (server_fd_ >= 0) {
        event_loop_.remove(server_fd_);

        ::close(server_fd_);
        server_fd_ = -1;
    }
}

} // namespace forgedb
