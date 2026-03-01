#include "login.pb.h"
#include <iostream>

using namespace std;
using namespace user;

/**
 * 示例测试代码
 */

int main() {

    string data;

    {
        // 客户端代码
        LoginRequest request;
        request.set_service_name("UserService");
        request.set_method_name("Login");
        request.mutable_userinfo()->set_name("zhangsan");
        request.mutable_userinfo()->set_password("123456");
        data = request.SerializeAsString();
        
    }

    {
        // 服务端代码
        LoginRequest request;
        request.ParseFromString(data);
        std::cout << request.service_name() << "." << request.method_name() << "(" << request.userinfo().name() << "," << request.userinfo().password() << ")" << std::endl;
    }

    return 0;
}