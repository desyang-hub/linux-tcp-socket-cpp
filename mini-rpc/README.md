没问题！做一个**轻量级 RPC 框架**是 C++ 校招面试中的**“核武器”**。它比 Web Server 更能体现你对**网络编程、序列化、动态代理、并发模型**的综合理解。

别被“RPC”这个词吓到，它的核心逻辑其实非常简单：
> **RPC = 本地函数调用 + 网络传输 + 数据序列化/反序列化**

我们将分 **4 个阶段**，用 **C++11/14** 标准，从零手写一个支持同步/异步调用的 RPC 框架。这个项目做完，你的简历上将有一个非常硬核的亮点。

---

### 🏗️ 项目架构设计 (Mini-RPC)

我们的目标不是造一个完美的 gRPC，而是实现核心流程。
**技术栈选型：**
*   **网络模型**：`Epoll` + `线程池` (复用你之前的代码)
*   **序列化协议**：`Protobuf` (工业界标准，必学) 或 `JSON` (简单，但慢)。推荐 **Protobuf**。
*   **通信协议**：自定义简单的 TCP 协议头 (魔数 + 数据长度 + 数据)。
*   **服务注册/发现**：本地映射表 (简化版，先不做 Zookeeper/Etcd)。
*   **动态代理**：利用 C++ 模板和 `std::function` 模拟。

#### 核心模块划分
1.  **Common**: 日志、宏定义、错误码。
2.  **RpcProvider**: 服务端，负责注册服务、监听端口、接收请求、反射调用本地函数。
3.  **RpcController**: 控制调用状态（成功/失败）。
4.  **RpcChannel**: 客户端通道，负责序列化请求、发送网络包、接收响应、反序列化。
5.  **RpcStub**: 客户端桩代码（自动生成或手动封装），让用户像调用本地函数一样调用远程函数。

---

### 🚀 第一阶段：定义服务协议 (Proto 文件)

首先，我们需要定义服务长什么样。假设我们要提供一个 `UserService`，有 `Login` 方法。

创建 `user.proto`:
```protobuf
syntax = "proto3";

package fixbug;

// 登录请求
message LoginRequest {
    string name = 1;
    string pwd = 2;
}

// 登录响应
message LoginResponse {
    int32 code = 1; // 0成功，-1失败
    string msg = 2;
}

// 定义服务接口
service UserService {
    // 登录方法
    rpc Login(LoginRequest) returns (LoginResponse);
    
    // 获取用户信息
    rpc GetUserInfo(GetUserInfoRequest) returns (GetUserInfoResponse);
}

message GetUserInfoRequest {
    int32 id = 1;
}

message GetUserInfoResponse {
    int32 code = 1;
    string name = 2;
    bool is_vip = 3;
}
```
*操作：安装 `protoc` 编译器，生成 `.cc` 和 `.h` 文件。*
```bash
protoc --cpp_out=. user.proto
```

---

### 🚀 第二阶段：实现 RpcProvider (服务端)

这是最核心的部分。我们需要一个类，它能：
1.  接收用户编写的服务类（如 `UserServiceImp`）。
2.  解析 TCP 请求。
3.  根据方法名，找到对应的本地函数并执行。
4.  返回结果。

**核心代码逻辑 (`RpcProvider.h`):**

```cpp
class RpcProvider {
public:
    // 发布服务方法：将服务对象注册到框架
    template<typename Service>
    void NotifyService(Service *service);

    // 启动服务
    void Run();

private:
    // 处理客户端请求的回调函数
    void OnMessage(int connfd, google::protobuf::IoStream *is);
    
    // 具体的服务调用逻辑
    void CallMethod(const google::protobuf::MethodDescriptor* method, 
                    google::protobuf::Message* request,
                    google::protobuf::Message* response,
                    google::protobuf::Closure* done);
    
    // 存储服务描述符和服务对象的映射
    std::map<std::string, std::map<std::string, google::protobuf::Service*>> service_map_;
};
```

**关键实现思路 (`NotifyService`):**
利用 Protobuf 的反射机制 (`GetDescriptor()`)，遍历服务中的所有方法，将 `服务名.方法名` 映射到具体的 `Service` 对象指针上。

```cpp
template<typename Service>
void RpcProvider::NotifyService(Service *service) {
    // 1. 获取服务描述符
    const google::protobuf::ServiceDescriptor* sd = service->GetDescriptor();
    std::string service_name = sd->name();
    
    // 2. 遍历所有方法
    for (int i = 0; i < sd->method_count(); ++i) {
        const google::protobuf::MethodDescriptor* md = sd->method(i);
        std::string method_name = md->name();
        
        // 3. 存入映射表：service_name -> method_name -> service_obj
        service_map_[service_name][method_name] = service;
    }
}
```

**`OnMessage` 处理流程:**
1.  读取网络数据。
2.  反序列化出 `ServiceName`, `MethodName`, `RequestData`。
3.  从 `service_map_` 找到对应的 `Service` 对象。
4.  调用 `service->CallMethod(method, request, response, ...)`.
5.  序列化 `response` 并发送回客户端。

---

### 🚀 第三阶段：实现 RpcChannel & RpcStub (客户端)

这是让用户“无感”调用的关键。用户代码看起来是这样的：
```cpp
UserService_Stub stub(&channel);
stub.Login(&controller, &request, &response, nullptr);
```

**`RpcChannel` 的核心逻辑 (`RpcChannel.cc`):**

```cpp
void RpcChannel::CallMethod(const google::protobuf::MethodDescriptor* method,
                            google::protobuf::RpcController* controller,
                            const google::protobuf::Message* request,
                            google::protobuf::Message* response,
                            google::protobuf::Closure* done) {
    // 1. 获取服务名和方法名
    std::string service_name = method->service()->full_name();
    std::string method_name = method->name();

    // 2. 序列化请求数据
    uint32_t req_size = 0;
    std::string req_str;
    if (request->SerializePartialToString(&req_str)) {
        req_size = req_str.size();
    } else {
        // 处理错误
        return;
    }

    // 3. 构造协议头 (自定义：服务名长度 + 服务名 + 方法名长度 + 方法名 + 数据长度 + 数据)
    // 这里省略具体的字节流拼接代码，需注意大小端转换
    
    // 4. 发送 TCP 请求 (使用 socket send)
    // int client_fd = socket(...); connect(...); send(...);
    
    // 5. 接收响应
    // recv(...); 
    
    // 6. 反序列化响应
    // response->ParseFromString(...);
}
```

**技巧**：为了让用户代码更简洁，我们可以利用 Protobuf 生成的 `Stub` 类，它内部已经持有了 `RpcChannel`。你只需要继承 `google::protobuf::RpcChannel` 并实现 `CallMethod` 即可。

---

### 🚀 第四阶段：整合与优化 (面试加分项)

当基本流程跑通后，你需要做以下优化，这才是**面试官想听的**：

1.  **引入线程池**：
    *   在 `RpcProvider::OnMessage` 中，不要直接处理业务逻辑，而是将任务打包提交给你之前写的 **ThreadPool**。
    *   *面试点*：如何处理高并发？如何避免阻塞 IO 线程？

2.  **自定义协议头**：
    *   解决 TCP 粘包/拆包问题。
    *   协议格式：`[HeaderLen(4B)][HeaderStr][BodyLen(4B)][BodyStr]`。
    *   *面试点*：TCP 流式特性如何处理？

3.  **异步调用支持**：
    *   目前的 `CallMethod` 是同步阻塞的（send -> wait -> recv）。
    *   尝试实现异步：发送后注册回调函数 (`Closure`)，收到响应后在线程池中执行回调。

4.  **服务注册与发现 (进阶)**：
    *   当前是硬编码 IP。
    *   改进：启动时向 Redis 或 Zookeeper 注册 `ServiceName -> IP:Port`。
    *   客户端调用前先从注册中心获取 IP。
    *   *面试点*：分布式系统的基本组件。

---

### 📅 你的执行计划 (2 周冲刺)

鉴于你 8 月就要秋招，这个项目不能拖太久。

*   **第 1-3 天：环境搭建与 Proto 定义**
    *   安装 `protobuf` 库。
    *   编写 `.proto` 文件，生成代码。
    *   写一个简单的 Server 和 Client，测试 Protobuf 序列化/反序列化是否正常。

*   **第 4-7 天：实现 RpcProvider (服务端)**
    *   实现 `NotifyService` 模板函数。
    *   实现 TCP Server (Epoll + 线程池)。
    *   实现 `OnMessage` 解析协议并反射调用。
    *   **里程碑**：Client 发请求，Server 能打印出参数，并返回固定字符串。

*   **第 8-10 天：实现 RpcChannel (客户端)**
    *   继承 `google::protobuf::RpcChannel`。
    *   实现 `CallMethod` 中的网络发送和接收。
    *   **里程碑**：Client 调用 `stub.Login()`，Server 执行真实逻辑，Client 拿到正确返回值。

*   **第 11-14 天：优化与博客撰写**
    *   接入你的 ThreadPool。
    *   处理粘包问题。
    *   **写博客**：
        1.  《从零手写 C++ RPC 框架 (一)：架构设计与 Protobuf 集成》
        2.  《从零手写 C++ RPC 框架 (二)：服务端反射调用与线程池模型》
        3.  《从零手写 C++ RPC 框架 (三)：客户端动态代理与网络通信》
        4.  《踩坑记录：RPC 框架开发中的 TCP 粘包与内存管理》

### 💡 为什么这个项目能帮你拿 Offer？

1.  **覆盖面广**：涵盖了网络 (TCP/Epoll)、并发 (线程池)、数据结构 (Map/Queue)、设计模式 (单例/工厂/代理)、第三方库 (Protobuf)。
2.  **深度足够**：你可以聊反射、聊序列化原理、聊 TCP 状态机、聊零拷贝。
3.  **差异化**：大部分应届生只会 Spring Boot 或简单的 HTTP Server。手写过 RPC 框架的人凤毛麟角。
4.  **结合背景**：对于军工/研究所，这种底层通信框架的理解非常重要（他们很多私有协议也是基于 TCP 的）。

### ️ 需要我提供具体的代码片段吗？
比如 `RpcProvider` 的完整实现，或者如何处理 TCP 粘包的代码？你可以告诉我你想先看哪一部分，我直接给你写出核心逻辑，你在此基础上修改和完善。加油，这个项目做完，你的秋招稳了！