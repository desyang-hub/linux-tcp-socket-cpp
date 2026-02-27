#include <iostream>
#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstring>
#include <unistd.h>
#include <sys/wait.h>

const int PORT = 8080;
const int BUFFER_SIZE = 1024;

void sigchld_handler(int sig);
void tcp_server();
void handle_client(int client_fd, struct sockaddr_in &client_addr);


int main() {

    tcp_server();
    return 0;
}


/**
 * pid_t waitpid(pid_t pid, int *status, int options);
 * 
 * pid	    等待哪个子进程	    -1 = 任意子进程
 * status	存储退出状态	    NULL = 不关心
 * options	选项	            WNOHANG = 非阻塞
 * 
 * 返回值	含义
 * > 0	    成功回收的子进程 PID
 * = 0	    有子进程但未退出（非阻塞模式）
 * -1	    错误或没有子进程
 */
void sigchld_handler(int sig) {
    while (waitpid(-1, NULL, WNOHANG) > 0); // >0 就一直进行子进程回收
}


// 处理client请求
void handle_client(int client_fd, struct sockaddr_in &client_addr) {
    std::cout << "[Child " << getpid() << "] client connected: " << inet_ntoa(client_addr.sin_addr) << std::endl; 

    // 6. send, recv 收发数据，这里只对接受的数据，进行回显即可
    char buffer[BUFFER_SIZE];
    while (true) {
        memset(buffer, 0, BUFFER_SIZE); // 重置缓冲区

        // 接收数据
        int bytes_num = recv(client_fd, buffer, BUFFER_SIZE, 0);

        if (bytes_num <= 0) {
            if (bytes_num == 0) {
                std::cout << "[Child " << getpid() << "] client disconnected" << std::endl;
            }
            else {
                perror("recv error");
            }
            
            break;
        }

        std::cout << "[Child " << getpid() << "] client Receive: " << buffer << std::endl;

        // 将收到的数据回显至客户端
        send(client_fd, buffer, bytes_num, 0);
    }

    // 关闭socket
    close(client_fd);
    // 退出进程
    exit(0);
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
    if (listen(sockfd, 10) == -1) { // listen 第二个参数的含义是当连接数小于设置值时会进入等待队列，若超过该值，则直接拒绝服务请求
        perror("listen error");
        close(sockfd);
        exit(1);
    }

    std::cout << "Multi-Process TCP Server" << std::endl;
    std::cout << "listen net port: " << PORT << std::endl;
    std::cout << "Server pid: " << getpid() << std::endl;

    // 注册信号处理函数，自动回收子进程
    signal(SIGCHLD, sigchld_handler);

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

        std::cout << "Client connected: " << inet_ntoa(client_addr.sin_addr) << std::endl;

        // fork 进程
        pid_t pid = fork();

        // 创建进程失败
        if (pid == -1) { 
            perror("fork error");
            close(client_fd);
            continue;
        }

        if (pid == 0) {
            // ========= 子进程 ==========
            close(sockfd); // 子进程无需监听socket
            // 对客户端请求进行处理
            handle_client(client_fd, client_addr);
        }
        else {
            // ========= 父进程 ==========
            close(client_fd); // 父进程只负责accept
        }
    }

    

    // 7. 关闭socket
    close(sockfd);
    close(client_fd);
}