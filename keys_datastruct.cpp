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