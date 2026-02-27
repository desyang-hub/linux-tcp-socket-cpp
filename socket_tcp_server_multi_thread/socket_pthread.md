**核心原理**
```cpp
#include <thread.h>

void client_handler(int client_fd, const struct sockaddr_in &server_addr);

// 主线程
// 创建子线程用于处理用户连接请求
std::thread t1(client_handler, client_fd, client_addr);
t1.detach(); // 分离线程
```