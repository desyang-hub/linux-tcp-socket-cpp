/**
 * Epoll TCP 服务器 (LT 模式)
 * 这是一个单线程服务器，但能同时处理无数连接。
 */
#include <fcntl.h>
#include <sys/socket.h>
#include <cstdlib>
#include <unistd.h>
#include <stdio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <iostream>
#include <cstring>
#include <sys/epoll.h>

const int PORT = 8080;
const int BUFFER_SIZE = 1024;
const int MAX_EVENTS = 1024; // 一次最大处理的事件数量


// 设置非阻塞IO （epoll最佳搭档）
void set_nonblocking(int fd);
void epoll_tcp_server();

int main() {
    epoll_tcp_server();

    return 0;
}

// ✅ 正确的函数名和实现
void set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        perror("fcntl get error");
        return;
    }
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

void epoll_tcp_server() {
    // 1. 创建socket
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        perror("failed create socket.");
        exit(1);
    }

    // 2. 设置地址复用，避免端口占用
    int opt = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // 3. 绑定
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY; // 监听所有网卡请求
    address.sin_port = htons(PORT); // 设置监听端口 TODO: 必须转换数据类型

    if (bind(sockfd, (struct sockaddr*)&address, sizeof(address)) == -1) {
        perror("bind error");
        close(sockfd);
        exit(1);
    }

    // 4. listen 监听
    if (listen(sockfd, 1024) == -1) { // listen 第二个参数的含义是当连接数小于设置值时会进入等待队列，若超过该值，则直接拒绝服务请求
        perror("listen error");
        close(sockfd);
        exit(1);
    }

    // 设置为非阻塞
    set_nonblocking(sockfd);

    std::cout << "============== Epoll TCP Server (LT MODE)==============" << std::endl;
    std::cout << "listen net port: " << PORT << std::endl;

    
    // 5. 创建epoll实例
    int epfd = epoll_create(1);

    if (epfd == -1) {
        perror("epoll create error");
        close(sockfd);
        exit(1);
    }

    // 6. 注册监听Socket到epoll
    epoll_event event;
    event.events = EPOLLIN; // 关注可读事件
    event.data.fd = sockfd; // 保存fd, 方便后续取出

    // 注册
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, sockfd, &event) == -1) {
        perror("epoll_ctl error");
        close(sockfd);
        close(epfd);
        exit(1);
    }

    // 7. 事件循环
    epoll_event events[MAX_EVENTS];

    while (true) {
        // 阻塞等待
        // 返回待处理的事件数量
        int n = epoll_wait(epfd, events, MAX_EVENTS, -1); // -1 表示无限阻塞等待

        if (n == -1) {
            if (errno == EINTR) continue; // 被信号中断继续
            perror("epoll_wait error");
            break;
        }

        // 遍历所有的就绪事件
        for (size_t i = 0; i < n; i++)
        {
            int c_fd = events[i].data.fd;

            // 情况A: 监听socket事件（有新连接）
            if (c_fd == sockfd) {
                struct sockaddr_in client_addr;
                socklen_t sock_len = sizeof(client_addr);

                // 循环遍历accept
                while (true) {
                    int client_fd = accept(sockfd, (struct sockaddr*)&client_addr, &sock_len);

                    if (client_fd == -1) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                            break; // 没有更多连接了
                        }

                        perror("accept error");
                        exit(1);
                    }

                    // 打印当前连接信息
                    std::cout << "[Epoll] New client connected " <<
                    inet_ntoa(client_addr.sin_addr) << ":" << ntohs(client_addr.sin_port) << " (fd = " << client_fd << ")" << std::endl;

                    // 关键：TODO: 将新连接设置为非阻塞
                    set_nonblocking(client_fd);
                    
                    // 注册新连接到epoll
                    event.events = EPOLLIN;
                    event.data.fd = client_fd;

                    if (epoll_ctl(epfd, EPOLL_CTL_ADD, client_fd, &event) == -1) {
                        perror("epoll_ctl error");
                        close(client_fd);
                    }

                }
            }
            else { // 情况B: 客户端Socket fd 就绪（可读或断开）
                char buffer[BUFFER_SIZE];

                while (true) { // 循环读取
                    memset(buffer, 0, BUFFER_SIZE);

                    ssize_t bytes_read = recv(c_fd, buffer, BUFFER_SIZE, 0);

                    if (bytes_read > 0) { // 正常，进行数据回显
                        send(c_fd, buffer, bytes_read, 0);
                    }
                    else if (bytes_read == 0) { // 断开连接
                        // 正常断开连接
                        std::cout << "Client " << c_fd << " disconneted" << std::endl;
                        close(c_fd);
                        // TODO: 重要！ 从epoll中移除close的fd（随人close的fd会自动移除，但是显示处理是好习惯）
                        epoll_ctl(epfd, EPOLL_CTL_DEL, c_fd, NULL);
                        break;
                    }
                    else {
                        // 在非阻塞状态下 EAGAIN 和 EWOULDBLOCK 表示本次读完
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                            break;
                        }

                        // 发生了错误
                        perror("recv error");
                        close(c_fd);
                        // 移除发生错误的fd
                        epoll_ctl(epfd, EPOLL_CTL_DEL, c_fd, NULL);
                        break;
                    }
                }
            }
        }
        
    }


    // 关闭socket
    close(sockfd);
    close(epfd);
}