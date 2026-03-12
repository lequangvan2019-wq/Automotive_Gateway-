#pragma once
#include <functional>
#include <unordered_map>
#include <stdexcept>
#include <sys/epoll.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>

class EventLoop {
public:
    EventLoop() {
        epoll_fd_ = epoll_create1(0);
        if (epoll_fd_ < 0)
            throw std::runtime_error(
                std::string("epoll_create1: ") + strerror(errno));
    }

    ~EventLoop() { if (epoll_fd_ >= 0) close(epoll_fd_); }

    void add_reader(int fd, std::function<void()> cb) {
        readers_[fd] = std::move(cb);
        epoll_event ev{};
        ev.events  = EPOLLIN;
        ev.data.fd = fd;
        if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &ev) < 0)
            throw std::runtime_error(
                std::string("epoll_ctl ADD: ") + strerror(errno));
    }

    void remove(int fd) {
        readers_.erase(fd);
        epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr);
    }

    void run() {
        running_ = true;
        epoll_event events[32];
        while (running_) {
            int n = epoll_wait(epoll_fd_, events, 32, -1);
            if (n < 0) {
                if (errno == EINTR) continue;
                throw std::runtime_error(
                    std::string("epoll_wait: ") + strerror(errno));
            }
            for (int i = 0; i < n; i++) {
                int fd = events[i].data.fd;
                if ((events[i].events & EPOLLIN) && readers_.count(fd))
                    readers_[fd]();
            }
        }
    }

    void stop() { running_ = false; }

private:
    int  epoll_fd_ = -1;
    bool running_  = false;
    std::unordered_map<int, std::function<void()>> readers_;
};
