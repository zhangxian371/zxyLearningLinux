# day03-fd-copy

Day 3 of the [前 10 天计划](../index.html)：用系统调用复制文件，理解 fd 与短读写。

## Build

```bash
cmake -S . -B build
cmake --build build
```

C++17，开 `-Wall -Wextra -Wpedantic`。

## `copy` — 只用 `open/read/write/close` 的文件复制

```bash
./build/copy <src> <dst>
```

实现要点：

- `open(src, O_RDONLY)` 打开源，`open(dst, O_WRONLY | O_CREAT | O_TRUNC, 0644)` 打开目标。
- 循环 `read()` 直到 EOF（返回 0）。每次循环处理一次短读：返回 < 0 且 `errno == EINTR` 时重试，其他 errno 报错退出。
- 每次拿到一段缓冲后调用 `write_all()`，循环 `write()` 直到全部字节写入，处理短写和 `EINTR`。
- 任何失败路径都会 `close()` 已打开的 fd（包括第二次 `open()` 失败时关第一次的）。
- 空文件、大文件（任意大小）、不存在的源、磁盘写满都能给到具体 `strerror` 输出。

## `dup_demo` — `dup2` 重定向 stdout

```bash
./build/dup_demo /tmp/out.txt
cat /tmp/out.txt
```

`fd` 是进程内的小整数索引；`dup2(fd, STDOUT_FILENO)` 让 fd 表里第 1 项指向与 `fd` 同一个打开文件描述。之后 `printf`（写 fd 1）就落到文件而不是终端。运行脚本里 `tests/run_demo.sh` 会同时校验终端只看到 dup 之前的行、文件只看到 dup 之后的行。

## 观察实验

```bash
# 系统调用摘要
strace -c ./build/copy /etc/hosts /tmp/hosts_copy

# 关键调用逐条追踪
strace -e trace=openat,read,write,close ./build/copy /etc/hosts /tmp/hosts_copy

# 跑在后台时查看进程打开的文件
./build/copy /etc/hosts /tmp/hosts_copy &
PID=$!
lsof -p "$PID"
wait "$PID"

# 单进程 fd 上限
ulimit -n
```

短读/短写在追踪里非常明显：单次 `read()` 返回的字节数常常小于缓冲区大小，`write()` 也可能分多次。理解这一点是 Day 7 协议解析（半包 / 粘包）的基础。

## 验收

- [ ] `read`/`write` 都用循环处理，不假设一次完成
- [ ] 能用 `strace -e trace=openat,read,write,close` 逐条说出每次调用发生了什么
- [ ] 能解释 fd 是进程内 fd 表的索引，不是文件本身；同一个文件可以被多个 fd 引用，`dup`/`dup2` 利用的就是这一点

## 跑演示脚本

```bash
./tests/run_demo.sh
```

会依次验证：复制小文件、空文件、10 MiB 随机大文件（sha256 比对）、不存在的源（期望 ENOENT 错误信息）、`dup2` 重定向 demo。

## 与后续 Day 的连接

- **Day 8（RAII）**：本目录里所有手动 `close()` 与"每条错误路径都要清理"的样板，会被 `UniqueFd` 取代。
- **Day 7（协议）**：循环读写同一份字节流，再叠加"长度头 + 帧体"的解析，就是完整消息协议。
