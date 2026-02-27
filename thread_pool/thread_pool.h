#pragma once

#include <iostream>
#include <vector>
#include <functional>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <utility>
#include <atomic>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <unistd.h>
#include <cstring>

std::atomic<int> g_client_count{0};

const int THREAD_POOL_SIZE = 4;
const int BUFFER_SIZE = 1024;

class ThreadPool
{
private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> jobs; // 提交无需参数的函数
    std::mutex m_mutex;
    std::condition_variable m_condition;
    bool stop;
public:
    ThreadPool(int pool_size = THREAD_POOL_SIZE);
    ~ThreadPool();

    template<class FUNC>
    void enqueue(FUNC &&f);
};

ThreadPool::ThreadPool(int pool_size) : stop{false}
{
    // 1. 创建线程
    for (size_t i = 0; i < pool_size; i++)
    {
        workers.emplace_back([this](){
            while (true) {
                // 线程需要做的事情
                std::function<void()> task = nullptr;
                // 1. 获取锁
                {
                    std::unique_lock<std::mutex> lock(m_mutex);

                    // 等待获取任务
                    m_condition.wait(lock, [this](){
                        return !jobs.empty() || stop;
                    });
                    
                    // 如果停止，直接结束线程
                    if (stop) {
                        return;
                    }

                    // 否则取出任务
                    task = std::move(jobs.front());
                    jobs.pop();
                } // 自动释放锁

                // 执行任务
                task();
            }
        }); // 使用原地构造
    }
    
}

ThreadPool::~ThreadPool()
{
    // 通知线程结束
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        stop = true;
    }
    // 通知所有的线程进入停机
    m_condition.notify_all();

    // TODO: thread 要么detach分离运行，要么join到主线程结束，否则就会异常退出
    // 所有的workers join
    for (auto &thread_item : workers) {
        if (thread_item.joinable()) {
            thread_item.join();
        }
    }

    std::cout << "[ThreadPool] All worker threads stopped." << std::endl;
}

template<class FUNC>
void ThreadPool::enqueue(FUNC &&f)
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        jobs.emplace(std::forward<FUNC>(f));
    }
    // 唤醒一个线程来执行该task
    m_condition.notify_one(); // 通知一个工作线程
}


// 定义一个处理用户连接请求task
void client_handler_task(int client_fd, const sockaddr_in &client_addr) {
    g_client_count++;
    std::cout << "[client] Client connected: " << inet_ntoa(client_addr.sin_addr) << ":" << ntohs(client_addr.sin_port) << " | Total: " << g_client_count << std::endl;

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

    std::cout << "[client] Close | Total: " << g_client_count << std::endl;
}