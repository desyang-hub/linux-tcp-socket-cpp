#include "user.pb.h"
#include <iostream>

// 继承自 Protobuf 生成的 UserService 基类
class UserServiceImpl : public fixbug::UserService {
public:
    // 实现 Login 方法
    void Login(google::protobuf::RpcController* controller,
               const ::fixbug::LoginRequest* request,
               ::fixbug::LoginResponse* response,
               google::protobuf::Closure* done) override {
        
        std::string name = request->name();
        std::string pwd = request->pwd();

        std::cout << "[Business Logic] Login called. Name: " << name << ", Pwd: " << pwd << std::endl;

        // 模拟业务判断
        if (name == "zhangsan" && pwd == "123456") {
            response->set_code(0);
            response->set_msg("Login successful!");
        } else {
            response->set_code(1);
            response->set_msg("Invalid username or password.");
        }

        // 执行回调，发送响应
        if (done != nullptr) {
            done->Run();
        }
    }
};