#include "ThreadPool.h"

ThreadPool::ThreadPool(int pool_size)
{
    for (size_t i = 0; i < pool_size; i++)
    {
        workers.emplace_back([this](){
            
            while (true) {
                Job job = nullptr;

                // 此处需要操作队列，而队列是共享资源
                { 
                    std::unique_lock<std::mutex> lock(m_mutex);

                    // 检查条件
                    m_condition.wait(lock, [this](){
                        return !this->jobs.empty() || this->stop;
                    });

                    if (stop) {
                        return; // 退出任务线程
                    }

                    // 取出job
                    job = std::move(jobs.front());
                    jobs.pop();
                }

                // 此时已经释放锁，执行任务
                job();
            }
        });
    }
    
}


ThreadPool::~ThreadPool()
{
    // 回收资源，确保所有线程停止，并join
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        stop = true;
    }
    m_condition.notify_all(); // 通知所有工作线程进行状态检查 此时stop = true; 即将退出

    for (auto &worker : workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }

    // 打印
    std::cout << "[ThreadPool] All worker threads stopped." << std::endl;
}