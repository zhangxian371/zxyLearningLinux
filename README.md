# Linux C++ 学习计划

一份从终端到多客户端服务器的 Linux C++ 学习路线，分两阶段、共 24 天。

- **第 1–10 天**：环境、进程、文件描述符、TCP、Socket、协议、RAII、线程与线程池。
- **第 11–24 天**：epoll、Reactor、MySQL、Redis、聊天室项目交付。

## 在线阅读

通过 GitHub Pages 访问：

```
https://zhangxian371.github.io/zxyLearningLinux/
```

- 首页：[前 10 天计划](https://zhangxian371.github.io/zxyLearningLinux/)
- 进阶：[Day 11–24 计划](https://zhangxian371.github.io/zxyLearningLinux/linux-cpp-study-plan-day11-24.html)

Pages 配置：在仓库 Settings → Pages → Source 选择 `Deploy from a branch`，分支 `main`，目录 `/ (root)`。

## 本仓库内容

| 路径 | 说明 |
| --- | --- |
| `index.html` | 前 10 天执行手册（同时是 GitHub Pages 落地页） |
| `linux-cpp-study-plan-day11-24.html` | Day 11–24 进阶执行手册 |
| `day01-linux-cpp-starter/` | Day 1 配套的最小 CMake 起步项目 |

## 起步项目

```bash
cd day01-linux-cpp-starter
cmake -S . -B build
cmake --build build
./build/linux-cpp-starter
```

使用 C++17，开启 `-Wall -Wextra -Wpedantic`。构建产物位于 `build/`，已在 `.gitignore` 中。

## 阅读建议

1. 每个 checkbox 的勾选状态保存在浏览器 `localStorage` 中，按天推进、按天复盘。
2. 不以"看完了"作为学习完成；以"能解释 + 能演示"作为通过标准。
3. 计划文件为单文件 HTML，无外部依赖，深色主题，适配移动端和打印。
