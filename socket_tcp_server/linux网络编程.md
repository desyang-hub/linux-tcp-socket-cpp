# Linux C++ Socket 网络编程入门：从零实现 TCP 客户端与服务器

> **导读**：从零开始学习 Linux 下的 C++ Socket 网络编程，通过完整的 TCP 客户端与服务器代码示例，掌握网络编程的核心概念与实践技巧。

---

## 🎯 前言：为什么学习 Socket 编程

Socket 是网络通信的基石，几乎所有网络应用都建立在 Socket 之上：

```
Web 服务器    →  Socket
微信/WhatsApp →  Socket
在线游戏      →  Socket
分布式系统    →  Socket
```

掌握 Socket 编程，你就掌握了**网络通信的底层能力**。

---

## 📚 核心概念速览

### 1. Socket 是什么？

```
Socket = IP地址 + 端口号 + 协议
```

可以把 Socket 想象成**网络通信的电话**：
- **IP 地址** = 电话号码（找到对方）
- **端口号** = 分机号（找到具体应用）
- **协议** = 通信语言（TCP/UDP）

### 2. TCP vs UDP

| 特性 | TCP | UDP |
|------|-----|-----|
| 连接 | 面向连接 | 无连接 |
| 可靠性 | 可靠传输 | 不保证到达 |
| 顺序 | 保证顺序 | 不保证顺序 |
| 速度 | 较慢 | 较快 |
| 典型应用 | HTTP、FTP、SSH | 视频流、DNS、游戏 |

### 3. TCP 通信流程

```
┌─────────────┐                    ┌─────────────┐
│   服务器     │                    │   客户端     │
├─────────────┤                    ├─────────────┤
│  socket()   │                    │  socket()   │
│  bind()     │                    │  connect()  │ ──┐
│  listen()   │                    │             │   │
│  accept()   │ ◄───────────────── │             │   │ 三次握手
│  recv()     │ ─────────────────► │  send()     │   │
│  send()     │ ◄───────────────── │  recv()     │   │
│  close()    │                    │  close()    │ ◄─┘
└─────────────┘                    └─────────────┘
         四次挥手
```

---

## 关键结构体介绍
```cpp
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

// IPv4 地址结构
struct sockaddr_in {
    sa_family_t    sin_family;  // 地址族 (AF_INET)
    uint16_t       sin_port;    // 端口号 (网络字节序)
    struct in_addr sin_addr;    // IP地址
};

// 通用地址结构
struct sockaddr {
    sa_family_t sa_family;
    char        sa_data[14];
};
```

## 💻 TCP 服务器完整实现

```cpp
#include <iostream>
#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstring>
#include <unistd.h>
#include <cstdlib>

const int PORT = 8080;
const int BUFFER_SIZE = 1024;

void tcp_server() {
    // ==================== 1. 创建 socket ====================
    // AF_INET: IPv4 协议族
    // SOCK_STREAM: TCP 协议（流式 socket）
    // 0: 自动选择默认协议
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        perror("failed create socket.");
        exit(1);
    }
    std::cout << "Socket created, fd = " << sockfd << std::endl;

    // ==================== 2. 设置地址复用 ====================
    // 避免服务器重启时出现 "Address already in use" 错误
    int opt = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // ==================== 3. 绑定地址 ====================
    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));  // 重要：初始化结构体
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;  // 监听所有网卡接口
    address.sin_port = htons(PORT);        // ⚠️ 必须转换为网络字节序

    if (bind(sockfd, (struct sockaddr*)&address, sizeof(address)) == -1) {
        perror("bind error");
        close(sockfd);
        exit(1);
    }
    std::cout << "Bind successful on port " << PORT << std::endl;

    // ==================== 4. 开始监听 ====================
    // 第二个参数 3: 等待队列最大长度（超过则拒绝连接）
    if (listen(sockfd, 3) == -1) {
        perror("listen error");
        close(sockfd);
        exit(1);
    }
    std::cout << "Listening on port: " << PORT << std::endl;
    std::cout << "Waiting for client connections..." << std::endl;

    int client_fd;
    
    // ==================== 主循环：持续接受连接 ====================
    while (true) {
        // ---------- 5. 接受客户端连接 ----------
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        client_fd = accept(sockfd, (struct sockaddr*)&client_addr, &client_len);
        
        if (client_fd == -1) {
            perror("accept error");
            close(sockfd);
            exit(1);
        }

        std::cout << "================================" << std::endl;
        std::cout << "Client connected: " << inet_ntoa(client_addr.sin_addr) 
                  << ":" << ntohs(client_addr.sin_port) << std::endl;

        // ---------- 6. 数据收发循环 ----------
        char buffer[BUFFER_SIZE];
        while (true) {
            memset(buffer, 0, BUFFER_SIZE);

            // 接收数据（阻塞式）
            int bytes_num = recv(client_fd, buffer, BUFFER_SIZE, 0);

            if (bytes_num <= 0) {
                if (bytes_num == 0) {
                    std::cout << "Client gracefully disconnected" << std::endl;
                } else {
                    perror("recv error");
                }
                break;
            }

            std::cout << "Received [" << bytes_num << " bytes]: " << buffer << std::endl;
            
            // 回显数据给客户端
            ssize_t sent = send(client_fd, buffer, bytes_num, 0);
            if (sent == -1) {
                perror("send error");
                break;
            }
            std::cout << "Echoed [" << sent << " bytes]" << std::endl;
        }

        // 关闭当前客户端连接
        close(client_fd);
        std::cout << "Client connection closed" << std::endl;
        std::cout << "================================" << std::endl;
    }

    // 7. 关闭服务器 socket（实际不会执行到这里）
    close(sockfd);
}

int main() {
    std::cout << "=== TCP Echo Server Starting ===" << std::endl;
    tcp_server();
    return 0;
}
```

---

## 💻 TCP 客户端完整实现

```cpp
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstdlib>

const int PORT = 8080;
const int BUFFER_SIZE = 1024;

void tcp_client() {
    // ==================== 1. 创建 socket ====================
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        perror("socket error");
        exit(1);
    }
    std::cout << "Socket created, fd = " << sockfd << std::endl;

    // ==================== 2. 设置服务器地址 ====================
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");  // 本地回环地址
    server_addr.sin_port = htons(PORT);                    // 网络字节序

    // ==================== 3. 连接服务器 ====================
    std::cout << "Connecting to server 127.0.0.1:" << PORT << "..." << std::endl;
    if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        perror("connect error");
        close(sockfd);
        exit(1);
    }
    std::cout << "✓ Success connected to Server" << std::endl;
    std::cout << "================================" << std::endl;

    // ==================== 4. 数据收发循环 ====================
    char buffer[BUFFER_SIZE];
    while (true) {
        std::cout << "> Enter message (type 'quit' to exit): ";
        memset(buffer, 0, BUFFER_SIZE);
        
        // 从键盘读取输入
        std::cin.getline(buffer, BUFFER_SIZE);

        // 检查退出命令
        if (strcmp(buffer, "quit") == 0) {
            std::cout << "User quit!" << std::endl;
            break;
        }

        // 发送数据到服务器
        ssize_t sent = send(sockfd, buffer, strlen(buffer), 0);
        if (sent == -1) {
            perror("send error");
            break;
        }
        std::cout << "Sent [" << sent << " bytes]" << std::endl;

        // 接收服务器响应
        memset(buffer, 0, BUFFER_SIZE);
        int bytes_num = recv(sockfd, buffer, BUFFER_SIZE, 0);
        if (bytes_num <= 0) {
            if (bytes_num == 0) {
                std::cout << "Server disconnected" << std::endl;
            } else {
                perror("recv error");
            }
            break;
        }
        
        std::cout << "Receive message: " << buffer << std::endl;
        std::cout << "================================" << std::endl;
    }
    
    // ==================== 5. 关闭 socket ====================
    close(sockfd);
    std::cout << "Socket closed, goodbye!" << std::endl;
}

int main() {
    std::cout << "=== TCP Echo Client Starting ===" << std::endl;
    tcp_client();
    return 0;
}
```

---

## 🔨 编译与运行指南

### 编译命令

```bash
# 编译服务器
g++ -std=c++11 -Wall -o server tcp_server.cpp

# 编译客户端
g++ -std=c++11 -Wall -o client tcp_client.cpp
```

### 运行步骤

```bash
# 终端 1：启动服务器
./server

# 终端 2：启动客户端
./client

# 或者使用 telnet 测试
telnet localhost 8080
```

### 预期输出

**服务器端：**
```
=== TCP Echo Server Starting ===
Socket created, fd = 3
Bind successful on port 8080
Listening on port: 8080
Waiting for client connections...
================================
Client connected: 127.0.0.1:54321
Received [5 bytes]: hello
Echoed [5 bytes]
Client connection closed
================================
```

**客户端：**
```
=== TCP Echo Client Starting ===
Socket created, fd = 3
Connecting to server 127.0.0.1:8080...
✓ Success connected to Server
================================
> Enter message (type 'quit' to exit): hello
Sent [5 bytes]
Receive message: hello
================================
> Enter message (type 'quit' to exit): quit
User quit!
Socket closed, goodbye!
```

---

## 📖 关键知识点详解

### 1. 字节序转换（⚠️ 最容易出错！）

```cpp
// 主机字节序 → 网络字节序
htons()  // host to network short (16位)
htonl()  // host to network long (32位)

// 网络字节序 → 主机字节序
ntohs()  // network to host short
ntohl()  // network to host long
```

**为什么需要转换？**
- 网络协议统一使用**大端序**（Big-Endian）
- x86/x64 架构使用**小端序**（Little-Endian）
- 不转换会导致端口号、IP 地址解析错误

### 2. Socket 生命周期

```
创建 → 配置 → 绑定 → 监听/连接 → 通信 → 关闭
socket()  setsockopt()  bind()  listen()/connect()  send()/recv()  close()
```

### 3. 关键系统调用返回值

| 函数 | 成功 | 失败 |
|------|------|------|
| `socket()` | ≥0 (文件描述符) | -1 |
| `bind()` | 0 | -1 |
| `listen()` | 0 | -1 |
| `accept()` | ≥0 (新文件描述符) | -1 |
| `connect()` | 0 | -1 |
| `send()/recv()` | ≥0 (实际字节数) | -1 |
| `close()` | 0 | -1 |

### 4. sockaddr_in 结构体详解

```cpp
struct sockaddr_in {
    sa_family_t    sin_family;  // 地址族 (AF_INET)
    in_port_t      sin_port;    // 端口号 (网络字节序)
    struct in_addr sin_addr;    // IP 地址
    char           sin_zero[8]; // 填充字节 (对齐用)
};

struct in_addr {
    uint32_t s_addr;  // IP 地址 (网络字节序)
};
```

### 5. 常用 IP 地址常量

| 常量 | 值 | 含义 |
|------|-----|------|
| `INADDR_ANY` | 0.0.0.0 | 监听所有网卡 |
| `INADDR_LOOPBACK` | 127.0.0.1 | 本地回环 |
| `INADDR_BROADCAST` | 255.255.255.255 | 广播地址 |

---

## ⚠️ 常见问题与调试

### 问题 1：Address already in use

```bash
bind error: Address already in use
```

**原因：** 端口被占用（服务器刚关闭，端口还在 TIME_WAIT 状态）

**解决：**
```cpp
// 代码中已添加 SO_REUSEADDR
int opt = 1;
setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
```

### 问题 4：recv 返回 0

**含义：** 对方正常关闭连接（不是错误）

**处理：**
```cpp
if (bytes_num == 0) {
    std::cout << "Client gracefully disconnected" << std::endl;
    break;
}
```

---

## 📝 总结
**核心要点：**
1. **字节序转换** - `htons()` 必不可少
2. **错误处理** - 所有系统调用都要检查返回值
3. **资源清理** - 及时 `close()` socket
4. **地址复用** - `SO_REUSEADDR` 避免端口占用