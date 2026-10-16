#include "forgedb/connection.hpp"

#include <cerrno>
#include <cstring>
#include <iostream>
#include <utility>

#include <sys/socket.h>
#include <unistd.h>

#include "forgedb/event_loop.hpp"
#include "forgedb/protocol.hpp"

namespace forgedb {

Connection::Connection(
    int fd,
    EventLoop& event_loop,
    MessageHandler message_handler,
    CloseHandler close_handler
)
    : fd_(fd),
      event_loop_(event_loop),
      message_handler_(
          std::move(message_handler)
      ),
      close_handler_(
          std::move(close_handler)
      ) {
}

Connection::~Connection() {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}

int Connection::fd() const {
    return fd_;
}

void Connection::start() {
    if (state_ == State::CLOSED) {
        return;
    }

    event_loop_.add_read(
        fd_,
        [this](int) {
            handle_readable();
        }
    );
}

void Connection::close() {
    if (state_ == State::CLOSED) {
        return;
    }

    state_ = State::CLOSED;

    int old_fd = fd_;

    event_loop_.remove(old_fd);

    if (old_fd >= 0) {
        ::close(old_fd);
        fd_ = -1;
    }

    if (close_handler_) {
        close_handler_(old_fd);
    }
}

void Connection::handle_readable() {
    if (state_ == State::CLOSED) {
        return;
    }

    char buffer[READ_BUFFER_SIZE];

    while (true) {
        ssize_t bytes_read =
            ::recv(
                fd_,
                buffer,
                sizeof(buffer),
                0
            );

        if (bytes_read > 0) {
            input_buffer_.append(
                buffer,
                static_cast<std::size_t>(
                    bytes_read
                )
            );

            continue;
        }

        if (bytes_read == 0) {
            close();
            return;
        }

        if (errno == EAGAIN ||
            errno == EWOULDBLOCK) {
            break;
        }

        if (errno == EINTR) {
            continue;
        }

        std::cerr
            << "Read failed: "
            << std::strerror(errno)
            << "\n";

        close();
        return;
    }

    process_messages();
}

void Connection::process_messages() {
    while (state_ != State::CLOSED) {
        std::string payload;

        /*
         * try_extract_frame() returns true only when
         * one COMPLETE frame exists in input_buffer_.
         *
         * If it returns false, we may simply need
         * more bytes from the network.
         */
        if (!try_extract_frame(
                input_buffer_,
                payload
            )) {
            return;
        }

        std::string response;

        try {
            response =
                message_handler_(payload);
        } catch (const std::exception& e) {
            response =
                std::string("ERROR ") +
                e.what();
        } catch (...) {
            response =
                "ERROR internal server error";
        }

        queue_response(
            std::move(response)
        );
    }
}

void Connection::queue_response(
    std::string response
) {
    if (state_ == State::CLOSED) {
        return;
    }

    std::string framed_response =
        create_frame(response);

    if (framed_response.empty() &&
        !response.empty()) {

        std::cerr
            << "Response is too large to frame\n";

        close();
        return;
    }

    output_buffer_.append(
        framed_response
    );

    enable_writing();

    state_ = State::WRITING;
}

void Connection::enable_writing() {
    event_loop_.add_write(
        fd_,
        [this](int) {
            handle_writable();
        }
    );
}

void Connection::disable_writing() {
    event_loop_.remove_write(
        fd_
    );
}

void Connection::handle_writable() {
    if (state_ == State::CLOSED) {
        return;
    }

    while (output_offset_ <
           output_buffer_.size()) {

        const char* data =
            output_buffer_.data() +
            output_offset_;

        std::size_t remaining =
            output_buffer_.size() -
            output_offset_;

        ssize_t bytes_written =
            ::send(
                fd_,
                data,
                remaining,
                0
            );

        if (bytes_written > 0) {
            output_offset_ +=
                static_cast<std::size_t>(
                    bytes_written
                );

            continue;
        }

        if (bytes_written < 0 &&
            (errno == EAGAIN ||
             errno == EWOULDBLOCK)) {

            return;
        }

        if (bytes_written < 0 &&
            errno == EINTR) {
            continue;
        }

        std::cerr
            << "Write failed: "
            << std::strerror(errno)
            << "\n";

        close();
        return;
    }

    /*
     * Everything has been written.
     */
    output_buffer_.clear();
    output_offset_ = 0;

    disable_writing();

    state_ = State::READING;

    /*
     * There may be more complete requests already
     * waiting in input_buffer_.
     *
     * Process them without waiting for another
     * network read event.
     */
    process_messages();
}

} // namespace forgedb
