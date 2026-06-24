# chatroom

Linux C++ 聊天室一期项目，面向 Day 24 最终验收：`epoll` + Reactor + 线程池 + 长度前缀协议 + 聊天室业务 + 压测脚本。

当前实现使用线程安全内存存储模拟 MySQL/Redis 边界，已经保留 `sql/schema.sql` 和文档中的 MySQL/Redis 设计。这样可以先验证网络、协议、业务和压测闭环，再替换真实持久化实现。

## 架构

```text
Client
  -> TCP length-prefixed KV frames
  -> ReactorServer
  -> epoll ET EventLoop
  -> TcpConnection input/output buffers
  -> ThreadPool
  -> ChatService
  -> in-memory repository/cache boundary
```

核心点：

- 所有 socket 均为非阻塞。
- `epoll` 使用 ET，`accept/recv/send` 都循环到 `EAGAIN`。
- I/O 线程不执行慢业务，业务在线程池中处理。
- 业务结果通过 `eventfd` 唤醒并投递回 EventLoop 发送。
- 协议使用 4 字节网络字节序长度头，防止粘包/半包。
- 单帧最大 1 MiB，单连接输入/输出缓冲有上限。

## 构建

```bash
cmake -S . -B build
cmake --build build -j
ctest --test-dir build --output-on-failure
```

## 运行服务端

```bash
./build/chat_server 8080 4
```

参数：

- `8080`：监听端口
- `4`：业务线程数

## 运行客户端

```bash
./build/chat_client 127.0.0.1 8080
```

客户端命令：

```text
/register <name> <password>
/login <name> <password>
/auth <token>
/join <room_id>
/send <room_id> <text>
/dm <user> <text>
/history <room_id> [limit]
/quit
```

演示建议：

1. 启动服务端。
2. 打开三个客户端。
3. 分别注册、登录、复制登录响应里的 token。
4. 执行 `/auth <token>`。
5. 三个客户端 `/join 1`。
6. 任意客户端 `/send 1 hello`，其他在线成员会收到 `ROOM_MESSAGE`。
7. 执行 `/history 1 10` 查看历史。

## 压测

```bash
./build/chat_bench 100 10 8080
```

含义：

- 100 个客户端
- 每个客户端发送 10 条消息
- 连接本机 8080

一次本机验证结果：

```text
clients=100 messages_per_client=10 sent=1000 fail=0 seconds=0.255946 msg_per_sec=3907.07
```

## 文件说明

- `src/net/`：Reactor、epoll、连接、缓冲区、长度前缀 codec。
- `src/app/`：聊天室业务。
- `src/protocol/`：KV 协议解析和序列化。
- `src/base/`：线程池。
- `src/common/`：日志、非阻塞 fd、RAII fd。
- `src/server/`：服务端入口。
- `src/client/`：客户端入口和通信工具。
- `src/bench/`：压测程序。
- `tests/`：协议和 codec 基础测试。
- `docs/`：协议、架构、压测和面试问答。
- `sql/`：真实 MySQL 版本的表结构草案。

## 当前边界

已完成：

- `epoll` ET 服务端。
- Reactor 风格事件循环。
- 线程池业务分发。
- 长度前缀协议。
- 注册、登录、认证、加入房间、群聊、私聊、历史消息。
- 100 客户端压测。

待替换为真实外部服务：

- MySQL 持久化实现。
- Redis token/在线状态/限流实现。

