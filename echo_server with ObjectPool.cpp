#include <iostream>
#include <string>
#include <unistd.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <memory>
#include <functional>
#include <unordered_map>
#include "memory_pool.h"

// 服务器监听端口
const int PORT = 8888;
// epoll_wait 一次最多返回的事件数
const int MAX_EVENTS = 1024;
// 接收缓冲区大小（字节）
const int BUFFER_SIZE = 1024;

/**
 * 客户端连接对象
 * 每个连接对应一个 Connection 对象，由内存池统一管理
 */
struct Connection {
    int fd;  // socket 文件描述符

    Connection(int f) : fd(f) {
        std::cout << "Connection 构造: fd=" << fd << std::endl;
    }

    ~Connection() {
        std::cout << "Connection 析构" << std::endl;
    }
};

/**
 * 将文件描述符设置为非阻塞模式
 * 参数 fd：要设置的文件描述符（socket）
 * 返回：设置成功返回 0，失败返回 -1
 */
int set_nonblocking(int fd) {
    // 获取当前文件描述符的标志位
    int flags = fcntl(fd, F_GETFL, 0);
    // 在原有标志基础上，加上 O_NONBLOCK（非阻塞）
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int main() {
    // ========== 0. 准备连接管理容器和内存池 ==========
    // connections：保存所有活跃连接（fd -> Connection 的 unique_ptr）
    // 当连接断开时，unique_ptr 自动析构，归还对象到内存池
    std::unordered_map<int, std::unique_ptr<Connection, std::function<void(Connection*)>>> connections;

    // 创建管理 Connection 的内存池，每块放 256 个对象
    memory_pool<Connection> conn_pool(256);

    // ========== 1. 创建监听 socket ==========
    // AF_INET：使用 IPv4 协议
    // SOCK_STREAM：使用 TCP 协议
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        std::cerr << "socket 失败" << std::endl;
    }

    // ========== 2. 绑定端口 ==========
    sockaddr_in addr{};
    addr.sin_family = AF_INET;              // 协议族：IPv4
    addr.sin_addr.s_addr = INADDR_ANY;      // 监听本机所有网卡
    addr.sin_port = htons(PORT);            // 端口号（htons 把主机字节序转成网络字节序）

    if (bind(listen_fd, (sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "bind 失败" << std::endl;
    }

    // ========== 3. 开始监听 ==========
    // SOMAXCONN：系统允许的最大连接队列长度
    if (listen(listen_fd, SOMAXCONN) < 0) {
        std::cerr << "listen 失败" << std::endl;
    }

    // ========== 4. 创建 epoll 实例 ==========
    int epoll_fd = epoll_create1(0);
    if (epoll_fd < 0) {
        std::cerr << "epoll_create 失败" << std::endl;
        return 1;
    }

    // ========== 5. 把监听 socket 加入 epoll ==========
    // 告诉 epoll：“帮我盯着 listen_fd，有可读事件就通知我”
    epoll_event ev{};
    ev.events = EPOLLIN;        // 关心“可读”事件（有新连接进来）
    ev.data.fd = listen_fd;     // 事件关联的文件描述符
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_fd, &ev);

    std::cout << "Echo 服务器启动，监听端口 " << PORT << std::endl;

    // ========== 6. 事件循环 ==========
    epoll_event events[MAX_EVENTS];  // 就绪事件数组
    char buffer[BUFFER_SIZE];        // 接收缓冲区

    while (true) {
        // 阻塞等待事件发生
        // 返回值 n：本次有多少个事件就绪
        int n = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        if (n < 0) {
            std::cerr << "epoll_wait 失败" << std::endl;
            break;
        }

        // 遍历所有就绪的事件
        for (int i = 0; i < n; i++) {
            int fd = events[i].data.fd;  // 取出事件对应的文件描述符

            // ========== 情况A：监听 socket 有新连接 ==========
            if (fd == listen_fd) {
                int client_fd = accept(listen_fd, nullptr, nullptr);
                if (client_fd < 0) {
                    continue;
                }

                // 把新连接的 socket 设置为非阻塞
                set_nonblocking(client_fd);

                // 从内存池获取一个 Connection 对象
                auto conn = conn_pool.acquire(client_fd);
                // 把所有权转移到 connections 容器中
                connections[client_fd] = std::move(conn);

                // 把新连接加入 epoll，告诉它：“帮我盯着这个新连接”
                epoll_event client_ev{};
                client_ev.events = EPOLLIN | EPOLLET;  // 可读事件 + 边缘触发
                client_ev.data.fd = client_fd;
                epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &client_ev);

                std::cout << "新客户端连接: fd=" << client_fd << std::endl;
            }
            // ========== 情况B：客户端有数据可读 ==========
            else {
                char temp[BUFFER_SIZE];
                // read：从客户端读取数据
                // 返回值 len：实际读到的字节数
                // <= 0：表示客户端断开连接
                int len = read(fd, temp, BUFFER_SIZE - 1);
                if (len <= 0) {
                    // 客户端断开，从 epoll 中移除并关闭 socket
                    std::cout << "客户端断开: fd=" << fd << std::endl;
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, nullptr);
                    close(fd);
                    // 从 connections 中移除，unique_ptr 自动析构，归还对象到内存池
                    connections.erase(fd);
                }
                else {
                    // 收到数据，加上字符串结束符
                    temp[len] = '\0';
                    std::cout << "收到: " << temp;

                    // write：把收到的数据原样发回去（Echo）
                    write(fd, temp, len);
                }
            }
        }
    }

    // 关闭文件描述符（实际上 while(true) 不会走到这里）
    close(listen_fd);
    close(epoll_fd);
    return 0;
}