#include<iostream>
#include<string>
#include<unistd.h>
#include<sys/epoll.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<fcntl.h>
#include<memory>
#include<functional>
#include<unordered_map>
#include"memory_pool.h"

const int PORT = 8888;
const int MAX_EVENTS = 1024;
const int BUFFER_SIZE = 1024;

struct Connection {
	int fd;

	Connection(int f) :fd(f) {
		std::cout << "Connection构造：fd=" << fd << std::endl;
	}

	~Connection(){
		std::cout << "Connection析构";
	}
};

int set_nonblocking(int fd) {
	int flags = fcntl(fd, F_GETFL, 0);
	return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int main() {
	std::unordered_map<int, std::unique_ptr<Connection, std::function<void(Connection*)>>> connections;
	memory_pool<Connection> conn_pool(256);

	int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (listen_fd < 0) {
		std::cerr << "socket失败" << std::endl;
	}

	sockaddr_in addr{};
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(PORT);

	if (bind(listen_fd, (sockaddr*)&addr, sizeof(addr)) < 0) {
		std::cerr << "bind失败" << std::endl;
	}

	if (listen(listen_fd, SOMAXCONN) < 0) {
		std::cerr << "listen失败" << std::endl;
	}

	int epoll_fd = epoll_create1(0);
	if (epoll_fd < 0) {
		std::cerr << "epoll_create失败" << std::endl;
		return 1;
	}

	epoll_event ev{};
	ev.events = EPOLLIN;
	ev.data.fd = listen_fd;
	epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_fd, &ev);

	std::cout << "Echo服务器启动，监听端口" << std::endl;

	epoll_event events[MAX_EVENTS];
	char buffer[BUFFER_SIZE];

	while (true) {
		int n = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
		if (n < 0) {
			std::cerr << "epoll_wait失败" << std::endl;
			break;
		}

		for (int i = 0; i < n; i++) {
			int fd = events[i].data.fd;

			if (fd == listen_fd) {
				int client_fd = accept(listen_fd, nullptr, nullptr);
				if (client_fd < 0) {
					continue;
				}

				set_nonblocking(client_fd);

				auto conn = conn_pool.acquire(client_fd);
				connections[client_fd] = std::move(conn);

				epoll_event client_ev{};
				client_ev.events = EPOLLIN | EPOLLET;
				client_ev.data.fd = client_fd;
				epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &client_ev);

				std::cout << "新客户端连接: fd=" << client_fd << std::endl;
			}
			else {
				char temp[BUFFER_SIZE];
				int len = read(fd, temp, BUFFER_SIZE - 1);
				if (len <= 0) {
					std::cout << "客户端断开: fd=" << fd << std::endl;
					epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, nullptr);
					close(fd);
					connections.erase(fd);
				}
				else {
					buffer[len] = '\0';
					std::cout << "收到: " << temp;

					write(fd, temp, len);
				}
			}
		}
	}

	close(listen_fd);
	close(epoll_fd);
	return 0;
}