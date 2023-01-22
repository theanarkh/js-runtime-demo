
#include "loop.h"

void Deer::Loop::init_event_system(struct event_loop* loop) {
    loop->event_fd = kqueue();
}

int Deer::Loop::poll(struct event_loop* loop) {
    struct kevent events[MAX_EVENT_SIZE];
    struct kevent ready_events[MAX_EVENT_SIZE];
    int nevent = 0;
    // 遍历 IO 观察者，把它们感兴趣的事件注册到事件驱动模块
    for (auto watcher: loop->io_watchers) {
        // 当前对可读事件感兴趣并且还没有注册到操作系统则注册
        if ((watcher->event & POLL_IN) == 0 && (watcher->pevent & POLL_IN) != 0) {
            EV_SET(&events[nevent++], watcher->fd, EVFILT_READ, EV_ADD, 0, 0, (void *)watcher);
            // 个数太多，先注册
            if (nevent == MAX_EVENT_SIZE) {
                kevent(loop->event_fd, events, nevent, 0, 0, nullptr);
                nevent = 0;
            }
        }
        if ((watcher->event & POLL_OUT) == 0 && (watcher->pevent & POLL_OUT) != 0) {
            EV_SET(&events[nevent++], watcher->fd, EVFILT_WRITE, EV_ADD, 0, 0, (void *)watcher);
            if (nevent == MAX_EVENT_SIZE) {
                kevent(loop->event_fd, events, nevent, 0, 0, nullptr);
                nevent = 0;
            }
        }
        watcher->event = watcher->pevent;
    }
    struct timespec *timeout = nullptr;
    loop->event_fd_count += nevent;
    if (loop->event_fd_count == 0) {
        return 0;
    }
    // 注册并等待事件触发
	int n = kevent(loop->event_fd, events, nevent, ready_events, MAX_EVENT_SIZE, timeout);
    if (n > 0) {
        // 遍历触发的事件
        for (int i = 0; i < n; i++) {
            io_watcher* watcher = static_cast<io_watcher *>(ready_events[i].udata);
            int event = 0;
            // 事件触发了但是用户对该事件已经不感兴趣了，删除它
            if (watcher->pevent == 0) {
                struct kevent event[1];
                loop->event_fd_count--;
                EV_SET(&event[0], watcher->fd, ready_events[i].filter, EV_DELETE, 0, 0, nullptr);
                kevent(loop->event_fd, event, 1, nullptr, 0, nullptr);
                watcher->handler(watcher, 0);
                continue;
            } else if (ready_events[i].filter == EVFILT_READ) { // 可读事件触发
                // 判断当前用户对事件是否还感兴趣
                if (watcher->pevent & POLL_IN) {
                    watcher->handler(watcher, POLL_IN);
                } else {
                    struct kevent event[1];
                    loop->event_fd_count--;
                    EV_SET(&event[0], watcher->fd, EVFILT_READ, EV_DELETE, 0, 0, nullptr);
                    kevent(loop->event_fd, event, 1, nullptr, 0, nullptr);
                }
            } else if (ready_events[i].filter == EVFILT_WRITE) { // 同上
                if (watcher->pevent & POLL_OUT) {
                    watcher->handler(watcher, POLL_OUT);
                } else {
                    struct kevent event[1];
                    loop->event_fd_count--;
                    EV_SET(&event[0], watcher->fd, EVFILT_WRITE, EV_DELETE, 0, 0, nullptr);
                    kevent(loop->event_fd, event, 1, nullptr, 0, nullptr);
                }
            }
        }
    }
    return loop->event_fd_count;
};

void Deer::Loop::run_event_system(struct event_loop* loop) {
	while(1) {
        int event = poll(loop);
        if (event == 0) {
            break;
        }
    }
};