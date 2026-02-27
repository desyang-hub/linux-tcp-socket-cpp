#include "ThreadPool.h"

void job_handler(int i);

int main() {
    ThreadPool pool;
    for (size_t i = 0; i < 1000; i++)
    {
        pool.submit([i](){
            job_handler(i);
        });
    }
    while (true);
    std::cout << "Program is endding." << std::endl;
    
}

void job_handler(int i) {
    std::cout << "[log] Task = " << i << " execute success." << std::endl;
}
