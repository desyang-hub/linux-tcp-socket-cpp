#include <iostream>
#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstring>
#include <unistd.h>
#include <thread>

#include <atomic> // 用于进行原子操作

// build bash: g++ -pthread tcp_server_multithread.cpp -o tcp_server_multithread

const int PORT = 8080;
const int BUFFER_SIZE = 1024;
const int THREAD_LIMIT = 10;

std::atomic<int> g_client_count{0};

// 需要一个同步机制，用于计数，当前产生的线程数，用于控制仅使用有限的线程来处理连接请求，超出部分拒绝服务

void tcp_server();
void client_handler(int client_fd, const struct sockaddr_in &server_addr);


int main() {

    tcp_server();
    return 0;
}

void client_handler(int client_fd, const struct sockaddr_in &server_addr) {
    std::cout << "Client connected: " << inet_ntoa(server_addr.sin_addr) << std::endl;

    // 6. send, recv 收发数据，这里只对接受的数据，进行回显即可
    char buffer[BUFFER_SIZE];
    while (true) {
        memset(buffer, 0, BUFFER_SIZE); // 重置缓冲区

        // 接收数据
        int bytes_num = recv(client_fd, buffer, BUFFER_SIZE, 0);

        if (bytes_num <= 0) {
            if (bytes_num == 0) {
                std::cout << "Connect disconnected" << std::endl;
            }
            else {
                perror("recv error");
            }
            break;
        }

        // 将收到的数据回显至客户端
        send(client_fd, buffer, bytes_num, 0);
    }

    // 关闭socket
    close(client_fd);
    g_client_count--;
}

void tcp_server() {
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
    if (listen(sockfd, 3) == -1) { // listen 第二个参数的含义是当连接数小于设置值时会进入等待队列，若超过该值，则直接拒绝服务请求
        perror("listen error");
        close(sockfd);
        exit(1);
    }

    std::cout << "Multi-Thread TCP Server" << std::endl;
    std::cout << "listen net port: " << PORT << std::endl;

    int client_fd;
    while (true) {
        // 5. accept 接受请求并服务
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        client_fd = accept(sockfd, (struct sockaddr*)&client_addr, &client_len);
        
        if (client_fd == -1) {
            perror("accept error");
            close(sockfd);
            close(client_fd);
            exit(1);
        }
        
        // 如果现存的线程数量超过预设数量，则拒绝处理
        if (g_client_count > THREAD_LIMIT) {
            close(sockfd);
            close(client_fd);
            continue;
        }

        g_client_count++;
        // 创建子线程用于处理用户连接请求
        std::thread t1(client_handler, client_fd, client_addr);
        t1.detach(); // 分离线程
    }

    // 7. 关闭socket
    close(sockfd);
}