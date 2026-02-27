# Epoll 实战：构建高性能 TCP 服务器

欢迎来到 Linux 网络编程的**高光时刻**！我们将用 `epoll` 重写服务器，实现**单线程处理成千上万个连接**。

---

## 🛠️ 一、Epoll 三大核心 API 详解

在写代码前，必须彻底理解这三个函数。

### 1. `epoll_create`：创建监控中心

```cpp
#include <sys/epoll.h>

int epoll_create(int size);
// Linux 2.6.8 之后，size 参数被忽略，只要大于 0 即可
```

- **作用**：创建一个 epoll 实例，返回一个文件描述符 `epfd`。
- **注意**：这个 `epfd` 也需要在最后 `close()`。

### 2. `epoll_ctl`：注册/修改/删除监听事件

```cpp
int epoll_ctl(int epfd, int op, int fd, struct epoll_event *event);
```

- **`epfd`**：`epoll_create` 返回的 ID。
- **`op`**：操作类型
  - `EPOLL_CTL_ADD`：注册新 FD。
  - `EPOLL_CTL_MOD`：修改已有 FD 的事件。
  - `EPOLL_CTL_DEL`：删除 FD。
- **`fd`**：你要监控的那个 Socket _fd。
- **`event`**：告诉内核你关心什么事件（读？写？错误？）。

**`struct epoll_event` 结构体：**
```cpp
struct epoll_event {
    uint32_t     events;      // 关注的事件 (EPOLLIN, EPOLLOUT, EPOLLET...)
    epoll_data_t data;        // 用户数据 (通常存放 fd 指针或上下文)
};

typedef union epoll_data {
    void    *ptr;
    int      fd;
    uint32_t u32;
    uint64_t u64;
} epoll_data_t;
```
> **技巧**：通常我们将 `data.fd` 设置为当前的 socket fd，这样当 `epoll_wait` 返回时，我们可以直接知道是哪个 fd 就绪了。

### 3. `epoll_wait`：等待事件发生

```cpp
int epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout);
```

- **`events`**：一个数组，用于存储内核返回的就绪事件。
- **`maxevents`**：数组的大小（一次最多获取多少个事件）。
- **`timeout`**：超时时间（毫秒）。`-1` 表示永久阻塞。
- **返回值**：就绪事件的数量。

---

## 💻 二、完整代码实现：Epoll TCP 服务器 (LT 模式)

这是一个**单线程**服务器，但能同时处理无数连接。

```cpp
/**
 * @file tcp_server_epoll.cpp
 * @brief 基于 Epoll (水平触发 LT) 的高性能 TCP 服务器
 * @date 2026-02-22
 */

#include <iostream>
#include <vector>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <errno.h>

const int PORT = 8080;
const int BUFFER_SIZE = 1024;
const int MAX_EVENTS = 1024; // 一次最多处理的事件数

// 设置非阻塞 IO (Epoll 最佳搭档)
void set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int main() {
    // 1. 创建监听 Socket
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd == -1) {
        perror("socket error");
        return -1;
    }

    // 地址复用
    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // 绑定
    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(listen_fd, (struct sockaddr*)&address, sizeof(address)) == -1) {
        perror("bind error");
        close(listen_fd);
        return -1;
    }

    // 监听
    if (listen(listen_fd, 1024) == -1) {
        perror("listen error");
        close(listen_fd);
        return -1;
    }

    // 设置listen_fd 为非阻塞
    set_nonblocking(listen_fd);

    std::cout << "=== Epoll TCP Server (LT Mode) ===" << std::endl;
    std::cout << "Listening on port " << PORT << std::endl;

    // 2. 创建 epoll 实例
    int epfd = epoll_create(1); 
    if (epfd == -1) {
        perror("epoll_create error");
        close(listen_fd);
        return -1;
    }

    // 3. 注册监听 Socket 到 epoll
    struct epoll_event ev;
    ev.events = EPOLLIN;          // 关注可读事件
    ev.data.fd = listen_fd;       // 保存 fd，方便后续取出

    if (epoll_ctl(epfd, EPOLL_CTL_ADD, listen_fd, &ev) == -1) {
        perror("epoll_ctl add listen_fd error");
        close(epfd);
        close(listen_fd);
        return -1;
    }

    // 4. 事件循环
    struct epoll_event events[MAX_EVENTS]; // 存储就绪事件
    
    while (true) {
        // 等待事件发生 (阻塞)
        // 返回值为就绪的 fd 数量
        int n = epoll_wait(epfd, events, MAX_EVENTS, -1);
        
        if (n == -1) {
            if (errno == EINTR) continue; // 被信号中断，继续
            perror("epoll_wait error");
            break;
        }

        // 遍历所有就绪的事件
        for (int i = 0; i < n; i++) {
            int sockfd = events[i].data.fd;

            // 情况 A: 监听 Socket 就绪 (有新连接)
            if (sockfd == listen_fd) {
                struct sockaddr_in client_addr;
                socklen_t client_len = sizeof(client_addr);
                
                // 循环 accept，直到没有新连接 (防止积压)
                while (true) {
                    int client_fd = accept(listen_fd, (struct sockaddr*)&client_addr, &client_len);
                    if (client_fd == -1) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                            break; // 没有更多连接了
                        }
                        perror("accept error");
                        break;
                    }

                    std::cout << "[Epoll] New client connected: " 
                              << inet_ntoa(client_addr.sin_addr) 
                              << ":" << ntohs(client_addr.sin_port) 
                              << " (fd=" << client_fd << ")" << std::endl;

                    // ⭐ 关键：将新连接的 Socket 设为非阻塞
                    set_nonblocking(client_fd);

                    // 注册新连接到 epoll
                    ev.events = EPOLLIN; 
                    ev.data.fd = client_fd;
                    
                    if (epoll_ctl(epfd, EPOLL_CTL_ADD, client_fd, &ev) == -1) {
                        perror("epoll_ctl add client_fd error");
                        close(client_fd);
                    }
                }
            } 
            // 情况 B: 客户端 Socket 就绪 (有数据可读 或 断开)
            else {
                char buffer[BUFFER_SIZE];
                while (true) { // 循环读取，直到读完 (LT 模式下即使不循环也没事，但习惯上读完)
                    memset(buffer, 0, BUFFER_SIZE);
                    ssize_t bytes_read = recv(sockfd, buffer, BUFFER_SIZE, 0);

                    if (bytes_read > 0) {
                        // 收到数据，回显
                        std::cout << "[Epoll] Received from fd=" << sockfd 
                                  << ": " << buffer << std::endl;
                        send(sockfd, buffer, bytes_read, 0);
                    } 
                    else if (bytes_read == 0) {
                        // 客户端正常关闭
                        std::cout << "[Epoll] Client disconnected (fd=" << sockfd << ")" << std::endl;
                        close(sockfd);
                        // ⭐ 重要：从 epoll 中移除该 fd (虽然 close 会自动移除，但显式管理是好习惯)
                        epoll_ctl(epfd, EPOLL_CTL_DEL, sockfd, NULL);
                        break;
                    } 
                    else {
                        // 出错或无数据 (非阻塞下返回 EAGAIN/EWOULDBLOCK 表示本次读完)
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                            break; // 数据读完，退出内层循环，继续处理下一个事件
                        }
                        // 真正错误
                        perror("recv error");
                        close(sockfd);
                        epoll_ctl(epfd, EPOLL_CTL_DEL, sockfd, NULL);
                        break;
                    }
                }
            }
        }
    }

    close(listen_fd);
    close(epfd);
    return 0;
}
```

---

## 🔍 三、代码关键点解析

### 1. 为什么要设非阻塞 (`set_nonblocking`)？
即使在使用 **LT (水平触发)** 模式时，也**强烈建议**将 Socket 设为非阻塞。
- **原因**：防止某个恶意的客户端连接后不发数据，导致 `recv` 阻塞整个线程（虽然 LT 模式下 `epoll` 会再次通知，但如果代码逻辑有误，仍可能阻塞）。
- **ET 模式必须**：如果是 ET 模式，**必须**设为非阻塞，否则一旦数据没读完，`recv` 会永久阻塞，因为没有新事件触发了。

### 2. `epoll_ctl` 的增删改
- **新连接**：`EPOLL_CTL_ADD`。
- **断开连接**：`EPOLL_CTL_DEL` (或者直接 `close`，内核会自动清理，但显式删除更清晰)。
- **修改事件**：比如想监听可写事件时，用 `EPOLL_CTL_MOD`。

### 3. 循环 `accept` 和 `recv`
- **循环 `accept`**：因为可能一瞬间来了 10 个连接，`epoll` 只通知了一次“监听 socket 可读”。如果不循环 `accept`，剩下的 9 个连接要等到下一次事件循环才能被处理。
- **循环 `recv`**：为了尽可能一次性读完缓冲区数据，减少系统调用次数。

---

## 🔨 四、编译与运行

### 编译
```bash
g++ -std=c++11 -Wall -o server_epoll tcp_server_epoll.cpp
# 不需要 -pthread，因为这是单线程模型！
```

### 运行与测试
```bash
# 启动服务器
./server_epoll

# 打开 10 个终端模拟客户端
for i in {1..10}; do
    telnet localhost 8080 &
done

# 或者使用 stress 工具压测 (如果安装了)
# apt-get install stress
```

**观察现象：**
- 服务器只有一个进程，CPU 占用极低。
- 可以同时和 10 个、100 个甚至 1000 个客户端聊天，互不干扰。
- 没有线程切换的开销。

---

## ⚔️ 五、LT (水平触发) vs ET (边缘触发) 代码差异

如果你想改为 **ET 模式**（性能更高，但代码要求更严），只需修改两处：

1. **注册事件时加上 `EPOLLET`**：
   ```cpp
   ev.events = EPOLLIN | EPOLLET; // 加上 EPOLLET
   ```

2. **`recv` 循环必须直到返回 `EAGAIN`**：
   ```cpp
   while (true) {
       ssize_t bytes_read = recv(sockfd, buffer, BUFFER_SIZE, 0);
       if (bytes_read > 0) {
           // 处理数据
       } else if (bytes_read == 0) {
           // 断开
       } else {
           if (errno == EAGAIN || errno == EWOULDBLOCK) {
               // ⭐ 关键：在 ET 模式下，只有读到 EAGAIN 才表示本次数据真正读完
               break; 
           }
           // 其他错误处理
       }
   }
   ```

> **建议**：初学者先用 **LT 模式**，逻辑简单且不易出错。熟练后再尝试 ET 模式挖掘极致性能。

---

## 📝 六、总结与下一步

### 本节成就
- ✅ 理解了 `select/poll` 的瓶颈。
- ✅ 掌握了 `epoll_create/ctl/wait` 三大 API。
- ✅ 实现了单线程高并发 Echo 服务器。
- ✅ 理解了非阻塞 IO 与 Epoll 的配合。

### 性能对比
| 模型 | 100 连接 | 10,000 连接 | 内存占用 | CPU 占用 |
| :--- | :--- | :--- | :--- | :--- |
| **多线程** | 良好 | 卡顿/崩溃 | 高 (栈内存) | 高 (切换) |
| **Epoll** | 极佳 | **极佳** | 低 (仅 Socket 结构) | **极低** |

### 🚀 终极挑战：Epoll + 线程池 (Reactor 模式)

现在的 Epoll 服务器是**单线程**的。如果某个客户端的业务逻辑非常耗时（比如计算斐波那契数列、读写大文件），会阻塞整个事件循环，导致其他客户端也无法响应。

**下一步计划**：
将 **Epoll (IO 处理)** 与 **线程池 (业务逻辑)** 结合。
- **主线程 (Reactor)**：只负责 `epoll_wait` 接收数据，将数据包丢入任务队列。
- **工作线程 (Worker)**：从队列取数据，执行耗时业务，然后发送响应。

这就是 Nginx、Redis 等高性能服务器的核心架构！**想继续学习这个终极架构吗？**