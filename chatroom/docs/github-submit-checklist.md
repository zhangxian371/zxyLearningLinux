# GitHub Submit Checklist

## Should Commit

- `CMakeLists.txt`
- `README.md`
- `.gitignore`
- `src/`
- `tests/`
- `docs/`
- `scripts/`
- `sql/schema.sql`

## Should Not Commit

- `build/`
- compiled binaries such as `chat_server`, `chat_client`, `chat_bench`
- `*.log`
- temporary files

`chatroom/.gitignore` already ignores the build directory and common temporary files.

## Pre-submit Commands

```bash
cd chatroom
cmake -S . -B build
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Optional network smoke test:

```bash
./build/chat_server 19080 4
./build/chat_bench 100 10 19080
```

Expected benchmark shape:

```text
clients=100 messages_per_client=10 sent=1000 fail=0 ...
```

## Suggested Commit Message

```text
day24: add epoll reactor chatroom project
```
