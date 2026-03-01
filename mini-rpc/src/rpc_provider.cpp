#include "rpc_provider.h"
#include "user.pb.h" // 引入生成的头文件
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <thread>

// 简单的日志宏
#define LOG_INFO(fmt, ...) printf("[INFO] " fmt "\n", ##__VA_ARGS__)

void RpcProvider::Run() {
    // 1. 创建 socket
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket create failed");
        exit(EXIT_FAILURE);
    }

    // 2. 绑定地址
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY; // 监听所有网卡
    server_addr.sin_port = htons(8888);       // 端口 8888

    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    // 3. 监听
    if (listen(server_fd, 5) < 0) {
        perror("listen failed");
        exit(EXIT_FAILURE);
    }

    LOG_INFO("RPC Server started on port 8888 ...");

    // 4. 接受连接 (简化版：单线程，一个接一个处理)
    while (true) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int connfd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
        
        if (connfd < 0) {
            perror("accept failed");
            continue;
        }

        LOG_INFO("New client connected!");
        
        // 处理请求 (实际项目中会放到线程池，这里为了简单直接处理)
        OnMessage(connfd);
        
        close(connfd);
    }
}

// ... 其他 include ...

// 【新增】实现 SendResponse 函数
void RpcProvider::SendResponse(int connfd, google::protobuf::Message* response) {
    std::string resp_str;
    if (response->SerializeToString(&resp_str)) {
        // 发送响应：[长度][数据]
        uint32_t len = resp_str.size();
        // 注意：实际生产环境需要 htonl(len) 处理网络字节序
        send(connfd, (char*)&len, sizeof(len), 0);
        send(connfd, resp_str.c_str(), len, 0);
        
        char buf[100];
        snprintf(buf, sizeof(buf), "Response sent (size: %d)", len);
        LOG_INFO("%s", buf);
    } else {
        LOG_INFO("Failed to serialize response");
    }
    
    // 释放内存
    delete response;
}

void RpcProvider::OnMessage(int connfd) {
    // ... (前面的解析代码保持不变) ...
    
    // 1. 读取网络数据 (略)
    char buffer[4096] = {0}; // 增大缓冲区以防万一
    int n = recv(connfd, buffer, sizeof(buffer), 0);
    if (n <= 0) return;

    // 2. 解析协议 (略，保持你之前的代码)
    // 假设你已经解析出了 service_name, method_name, req_data_str
    // 为了演示，我这里简化写一下解析逻辑，请替换为你之前完整的解析代码
    uint32_t service_name_len = 0;
    memcpy(&service_name_len, buffer, sizeof(uint32_t));
    std::string service_name(buffer + sizeof(uint32_t), service_name_len);
    
    uint32_t method_name_len = 0;
    size_t offset = sizeof(uint32_t) + service_name_len;
    memcpy(&method_name_len, buffer + offset, sizeof(uint32_t));
    std::string method_name(buffer + offset + sizeof(uint32_t), method_name_len);
    
    uint32_t req_data_len = 0;
    offset += sizeof(uint32_t) + method_name_len;
    memcpy(&req_data_len, buffer + offset, sizeof(uint32_t));
    std::string req_data_str(buffer + offset + sizeof(uint32_t), req_data_len);

    LOG_INFO("Recv request: Service=%s, Method=%s", service_name.c_str(), method_name.c_str());

    // 3. 查找服务 (略，保持原样)
    if (service_map_.find(service_name) == service_map_.end()) return;
    auto& method_map = service_map_[service_name];
    if (method_map.find(method_name) == method_map.end()) return;

    google::protobuf::Service* service = method_map[method_name];
    const google::protobuf::ServiceDescriptor* sd = service->GetDescriptor();
    const google::protobuf::MethodDescriptor* md = sd->FindMethodByName(method_name);

    // 4. 创建对象 (略，保持原样)
    google::protobuf::Message* request = service->GetRequestPrototype(md).New();
    google::protobuf::Message* response = service->GetResponsePrototype(md).New();

    if (!request->ParseFromString(req_data_str)) {
        LOG_INFO("Parse failed");
        delete request;
        delete response;
        return;
    }

    // 【修改点】这里不再使用 lambda，而是使用 NewCallback 绑定成员函数
    // 语法：NewCallback(对象指针, &成员函数, 参数1, 参数2, ...)
    google::protobuf::Closure* done = google::protobuf::NewCallback(
        this,               // 对象指针 (this)
        &RpcProvider::SendResponse, // 成员函数指针
        connfd,             // 传递给 SendResponse 的参数 1
        response            // 传递给 SendResponse 的参数 2
    );

    // 5. 调用业务逻辑
    service->CallMethod(md, nullptr, request, response, done);

    // 注意：request 在这里可以删除了，因为 CallMethod 是同步拷贝或者已经使用完毕
    // 但在某些异步实现中可能需要保留，这里简单起见，我们在 CallMethod 返回后删除 request
    // 实际上 Protobuf 的 CallMethod 通常是同步执行用户逻辑，然后异步执行 done
    // 为了安全，我们让 done 回调里或者 CallMethod 之后处理 request 的生命周期
    // 简单做法：在 CallMethod 之后立即删除 request (因为数据已经拷贝到内部或者处理完了)
    delete request; 
    
    // response 会在 SendResponse 中被删除
}