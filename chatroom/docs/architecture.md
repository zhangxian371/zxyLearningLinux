# Architecture

## Event Path

```text
epoll_wait
  -> listen fd readable
  -> accept4 loop until EAGAIN
  -> conn fd readable
  -> recv loop until EAGAIN
  -> Codec extracts complete frames
  -> ThreadPool handles ChatService request
  -> EventLoop wakes via eventfd
  -> append response to output buffer
  -> EPOLLOUT send loop until EAGAIN or empty
```

## Why Reactor

The epoll loop only knows events. Business logic lives behind `ChatService`.
Adding a new command should not require changing the epoll dispatch loop.

## Thread Ownership

- I/O thread owns sockets and connection buffers.
- Worker threads own synchronous business execution.
- Worker threads never call `send` directly.
- Results are posted back to the I/O thread.

## Current Storage Boundary

The current project uses in-memory storage to keep the project runnable without
external services. The intended production boundary is:

- MySQL: users, rooms, room members, messages.
- Redis: token, online state, rate limit counters, room member cache.

