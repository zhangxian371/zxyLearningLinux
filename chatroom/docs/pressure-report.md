# Pressure Report

Command:

```bash
./build/chat_server 19080 4
./build/chat_bench 100 10 19080
```

Observed result on local machine:

```text
clients=100 messages_per_client=10 sent=1000 fail=0 seconds=0.255946 msg_per_sec=3907.07
```

What this verifies:

- 100 TCP clients can connect concurrently.
- Each client can register, login, authenticate, join room, and send messages.
- The server uses nonblocking socket + epoll ET without obvious event-loop stalls.
- The business thread pool can process this basic load.

Known caveat:

- The current storage implementation is memory based. Real MySQL/Redis latency is not included in this number.
- The benchmark does not wait for every broadcast event; it focuses on request path success.

