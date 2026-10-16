#include "forgedb/event_loop.hpp"

#include <cerrno>
#include <cstring>
#include <iostream>

#include <sys/event.h>
#include <sys/time.h>
#include <unistd.h>

namespace forgedb {

EventLoop::EventLoop() {
    queue_fd_ = kqueue();

    if (queue_fd_ < 0) {
        std::cerr
            << "Failed to create kqueue: "
            << std::strerror(errno)
            << "\n";
    }
}

EventLoop::~EventLoop() {
    if (queue_fd_ >= 0) {
        close(queue_fd_);
    }
}

bool EventLoop::register_event(
    int fd,
    EventType type,
    bool enable
) {
    if (queue_fd_ < 0) {
        return false;
    }

    struct kevent change{};

    int16_t filter =
        type == EventType::READ
            ? EVFILT_READ
            : EVFILT_WRITE;

    uint16_t flags =
        enable
            ? EV_ADD | EV_ENABLE
            : EV_DELETE;

    EV_SET(
        &change,
        fd,
        filter,
        flags,
        0,
        0,
        nullptr
    );

    int result = kevent(
        queue_fd_,
        &change,
        1,
        nullptr,
        0,
        nullptr
    );

    return result == 0;
}

bool EventLoop::add_read(
    int fd,
    Callback callback
) {
    read_callbacks_[fd] =
        std::move(callback);

    if (!register_event(
            fd,
            EventType::READ,
            true
        )) {
        read_callbacks_.erase(fd);
        return false;
    }

    return true;
}

bool EventLoop::add_write(
    int fd,
    Callback callback
) {
    write_callbacks_[fd] =
        std::move(callback);

    if (!register_event(
            fd,
            EventType::WRITE,
            true
        )) {
        write_callbacks_.erase(fd);
        return false;
    }

    return true;
}

bool EventLoop::remove_read(
    int fd
) {
    read_callbacks_.erase(fd);

    /*
     * It is possible that the event was already
     * removed, so we do not treat a delete failure
     * as a fatal error here.
     */
    register_event(
        fd,
        EventType::READ,
        false
    );

    return true;
}

bool EventLoop::remove_write(
    int fd
) {
    write_callbacks_.erase(fd);

    register_event(
        fd,
        EventType::WRITE,
        false
    );

    return true;
}

bool EventLoop::remove(
    int fd
) {
    remove_read(fd);
    remove_write(fd);

    return true;
}

void EventLoop::run() {
    if (queue_fd_ < 0) {
        return;
    }

    running_ = true;

    constexpr int MAX_EVENTS = 64;

    struct kevent events[MAX_EVENTS];

    while (running_) {
        int event_count = kevent(
            queue_fd_,
            nullptr,
            0,
            events,
            MAX_EVENTS,
            nullptr
        );

        if (event_count < 0) {
            if (errno == EINTR) {
                continue;
            }

            std::cerr
                << "kevent failed: "
                << std::strerror(errno)
                << "\n";

            break;
        }

        for (int i = 0;
             i < event_count;
             ++i) {

            int fd =
                static_cast<int>(
                    events[i].ident
                );

            if (events[i].flags & EV_ERROR) {
                remove(fd);
                continue;
            }

            if (events[i].filter ==
                EVFILT_READ) {

                auto it =
                    read_callbacks_.find(fd);

                if (it !=
                    read_callbacks_.end()) {

                    it->second(fd);
                }
            }

            if (events[i].filter ==
                EVFILT_WRITE) {

                auto it =
                    write_callbacks_.find(fd);

                if (it !=
                    write_callbacks_.end()) {

                    it->second(fd);
                }
            }
        }
    }
}

void EventLoop::stop() {
    running_ = false;
}

} // namespace forgedb
