#include <sys/epoll.h>

/**
 * @brief 创建监控中心
 * 
 * 作用：创建一个 epoll 实例，返回一个文件描述符 epfd。
 * 注意：这个 epfd 也需要在最后 close()。
 */
int epoll_create(int size);
// Linux 2.6.8 之后，size 参数被忽略，只要大于 0 即可


/**
 * @brief 注册|修改|删除 监听事件
 * 
 * @param epfd: epoll_create 返回的epfd
 * @param op：操作类型  
 *  -- EPOLL_CTL_ADD：注册新 FD。  
 *  -- EPOLL_CTL_MOD：修改已有 FD 的事件。  
 *  -- EPOLL_CTL_DEL：删除 FD。  
 * @param fd: 你要监控的那个socket fd
 * @param event: 告诉内核你关心什么事件（读？写？错误？）
 */
int epoll_ctl(int epfd, int op, int fd, struct epoll_event *event);


struct epoll_event {
    uint32_t events;    // 关注的事件[EPOLLIN, EPOLLOUT, EPOLLET...]
    epoll_data_t data;  // 用户数据(通常存放fd指针或者上下文)
};

// 技巧：通常我们将 data.fd 设置为当前的 socket fd，这样当 epoll_wait 返回时，我们可以直接知道是哪个 fd 就绪了。
typedef union epoll_data {
    void *ptr;
    int fd;
    uint32_t u32;
    uint64_t u64;
} epoll_data_t;



/**
 * @brief 等待事件发生
 * 
 * @param event: 一个数组，用于存储内核返回的就绪事件
 * @param maxevents: 数组的大小（一次最多返回多少事件）
 * @param timeout: 超时时间，（-1表示永久阻塞）
 * @return 返回就绪事件的数量。
 * 
 * 
 */
int epoll_wait(int epfd, struct epoll_event *event, int maxevents, int timeout);