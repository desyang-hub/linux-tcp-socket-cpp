**核心原理**
```cpp
pid_t pid = fork();

if (pid == 0) {
    // 子进程：服务客户端
    // ...
    exit(0);  // 服务完成后退出
} else if (pid > 0) {
    // 父进程：继续 accept 新连接
    close(client_fd);  // 父进程不需要这个 socket
    continue;
}
```