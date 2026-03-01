#pragma once
#include <google/protobuf/service.h>
#include <map>
#include <string>

class RpcProvider {
public:
    // 注册服务：把用户实现的服务对象注册到框架里
    template<typename Service>
    void NotifyService(Service *service);

    // 启动 RPC 服务器
    void Run();

    // 【新增】声明发送响应的成员函数
    // 参数：connfd (连接句柄), response (响应消息指针)
    void SendResponse(int connfd, google::protobuf::Message* response);

private:
    // 存储服务的映射表：服务名 -> (方法名 -> 服务对象指针)
    std::map<std::string, std::map<std::string, google::protobuf::Service*>> service_map_;
    
    // 处理客户端请求的函数
    void OnMessage(int connfd);
};

// 模板函数的实现必须写在头文件里
template<typename Service>
void RpcProvider::NotifyService(Service *service) {
    // 1. 获取服务描述符 (Protobuf 生成的元数据)
    const google::protobuf::ServiceDescriptor* sd = service->GetDescriptor();
    std::string service_name = sd->name();

    // 2. 遍历该服务的所有方法
    for (int i = 0; i < sd->method_count(); ++i) {
        const google::protobuf::MethodDescriptor* md = sd->method(i);
        std::string method_name = md->name();
        
        // 3. 存入映射表
        service_map_[service_name][method_name] = service;
    }
}