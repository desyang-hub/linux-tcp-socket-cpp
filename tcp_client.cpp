#include <iostream>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

const int PORT = 8080;
const int BUFFER_SIZE = 1024;

void tcp_client() {
    // 1. 创建socket
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        perror("socket error");
        exit(1);
    }

    // 2. 设置服务器地址
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    server_addr.sin_port = htons(PORT); // 端口类型转换

    // 3. 向服务器发起连接请求
    if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        perror("connect error");
        close(sockfd);
        exit(1);
    }

    std::cout << "success connect to Server" << std::endl;

    // 4. 发请求和接受回复
    char buffer[BUFFER_SIZE];
    while (true)
    {
        std::cout << "Enter message: ";
        memset(buffer, 0, BUFFER_SIZE);
        // 从键盘读入msg
        std::cin.getline(buffer, BUFFER_SIZE);

        if (strcmp(buffer, "quit") == 0) {
            std::cout << "user quit!" << std::endl;
            break;
        }

        // 将消息发送给服务端
        send(sockfd, buffer, strlen(buffer), 0);

        // 重置缓冲器
        memset(buffer, 0, BUFFER_SIZE);

        // 从socket中接收消息
        int bytes_num = recv(sockfd, buffer, BUFFER_SIZE, 0);
        if (bytes_num <= 0) {
            std::cout << "Server disconnect" << std::endl;
            break;
        }
        
        std::cout << "Receive message: " << buffer << std::endl;
    }
    
    // 5. 关闭socket
    close(sockfd);

};

int main() {
    tcp_client();

    return 0;
}