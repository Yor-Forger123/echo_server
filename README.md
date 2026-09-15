# Echo Server (基于 epoll 的高并发回声服务器) 🚀

## 🌟 项目简介

一个使用 C++17 和 Linux `epoll` 实现的高并发 TCP Echo 服务器。  
单线程即可同时处理数千个客户端连接，是理解 **I/O 多路复用** 与 **事件驱动模型** 的绝佳实践。

## ✨ 核心特性

- **单线程高并发**：基于 `epoll` 边缘触发 (EPOLLET) 模式，一个线程同时监听多个客户端
- **非阻塞 IO**：所有 socket 均设置为 `O_NONBLOCK`，避免线程被单个连接阻塞
- **事件驱动**：采用 Reactor 模式，`epoll_wait` 阻塞等待事件，事件就绪后立即处理
- **轻量级**：无任何第三方依赖，仅使用 Linux 系统调用
- **完整 Echo 逻辑**：客户端发送什么，服务器就原样返回什么

## 🛠️ 技术栈

- **语言**：C++17
- **平台**：Linux（WSL / Ubuntu）
- **核心技术**：`epoll`、非阻塞 IO、TCP socket 编程、Reactor 模式
- **编译**：`g++` / CMake（可选）

### 环境要求

- Linux 环境（推荐 WSL / Ubuntu）
- `g++` 支持 C++17

### 编译

```bash
g++ -std=c++17 echo_server.cpp -o echo_server
