#pragma once

#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <utility>
#include <unistd.h>
#include <thread>
#include <functional>
#include <iostream>

const int THREAD_POOL_SIZE = 4;
using Job = std::function<void()>;

class ThreadPool
{
private:
    std::vector<std::thread> workers;
    std::queue<Job> jobs;
    std::mutex m_mutex;
    std::condition_variable m_condition;
    bool stop;

public:
    ThreadPool(int pool_size = THREAD_POOL_SIZE);
    ~ThreadPool();

public:
    template<class FUNC>
    void submit(FUNC &&job);
};

// 提交任务，注意job是一个无参函数
template<class FUNC>
void ThreadPool::submit(FUNC &&job) {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        jobs.emplace(std::forward<FUNC>(job));
    }

    // 唤醒一个线程,这个动作无需锁
    m_condition.notify_one();
}
