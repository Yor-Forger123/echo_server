# Echo Server (基于 epoll + 内存池的高并发回声服务器) 🚀

## 🌟 项目简介

一个使用 C++17 和 Linux `epoll` 实现的高并发 TCP Echo 服务器。  
**集成自研高性能内存池管理连接对象**，单线程即可同时处理数千个客户端连接，是理解 **I/O 多路复用**、**事件驱动模型** 与 **底层资源管理** 的绝佳实践。

## ✨ 核心特性

- **单线程高并发**：基于 `epoll` 边缘触发 (EPOLLET) 模式，一个线程同时监听多个客户端
- **内存池管理连接**：连接对象由自研内存池分配，**O(1)** 分配与回收，避免频繁 `new`/`delete`
- **RAII 自动归还**：连接断开时 `unique_ptr` 自动析构，对象归还池中，杜绝内存泄漏
- **非阻塞 IO**：所有 socket 均设置为 `O_NONBLOCK`，避免线程被单个连接阻塞
- **事件驱动**：采用 Reactor 模式，`epoll_wait` 阻塞等待事件，事件就绪后立即处理
- **轻量级**：无任何第三方依赖，仅使用 Linux 系统调用

## 🛠️ 技术栈

- **语言**：C++17
- **平台**：Linux（WSL / Ubuntu）
- **核心技术**：`epoll`、非阻塞 IO、TCP socket 编程、Reactor 模式、自研内存池、RAII、`unique_ptr` 自定义删除器
- **编译**：`g++`

## 🚀 快速开始

### 环境要求

- Linux 环境（推荐 WSL / Ubuntu）
- `g++` 支持 C++17

### 编译

```bash
g++ -std=c++17 "echo_server with ObjectPool.cpp" -o echo_server
