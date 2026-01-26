# FrontendAgent C++ 实现指南

## 概述

这是 JavaScript `FrontendAgent` 类的C++实现，用于Godot游戏引擎中的WebSocket通信。重点关注**内存安全**和**资源管理**。

## 核心设计

### 1. 内存安全特性

#### 1.1 RAII (Resource Acquisition Is Initialization)

```cpp
// ✅ 推荐：使用智能指针
auto agent = std::make_unique<FrontendAgent>("ws://localhost:8080", "agent");
// 作用域结束时自动析构，无需手动delete

// ❌ 避免：原始指针
FrontendAgent* agent = new FrontendAgent("ws://localhost:8080", "agent");
delete agent; // 容易忘记或出现异常时泄漏
```

#### 1.2 禁用拷贝操作

```cpp
// 禁用不安全的深拷贝
FrontendAgent(const FrontendAgent&) = delete;
FrontendAgent& operator=(const FrontendAgent&) = delete;

// 原因：
// - WebSocket连接不应被复制
// - 事件监听器映射管理复杂
// - 线程管理复杂

// ✅ 推荐：移动操作
FrontendAgent agent = std::move(other_agent);
```

#### 1.3 显式资源释放

```cpp
// 析构函数的关键职责：
~FrontendAgent() {
    shouldStop_ = true;        // 停止后台线程
    disconnect();              // 关闭WebSocket
    reconnectThread_->join();  // 等待线程完成
    eventListeners_.clear();   // 清理事件监听器
}
```

### 2. 线程安全

#### 2.1 互斥锁保护共享资源

```cpp
// 事件监听器映射受互斥锁保护
std::map<std::string, std::vector<EventCallback>> eventListeners_;
mutable std::mutex eventListenersMutex_;

// 使用范围锁确保自动解锁
{
    std::lock_guard<std::mutex> lock(eventListenersMutex_);
    eventListeners_[eventType].push_back(callback);
} // 自动释放锁
```

#### 2.2 原子操作

```cpp
// 线程间的无锁同步
std::atomic<ConnectionState> connectionState_;
std::atomic<int> reconnectAttempts_;
std::atomic<bool> shouldStop_;

// 这些操作是线程安全的，无需额外加锁
connectionState_ = ConnectionState::CONNECTED;
```

#### 2.3 条件变量

```cpp
std::condition_variable messageQueueCV_;

// 等待消息队列不为空（可用于消息处理线程）
std::unique_lock<std::mutex> lock(messageQueueMutex_);
messageQueueCV_.wait(lock, [this] { return !messageQueue_.empty(); });
```

### 3. 异常安全

#### 3.1 异常处理

```cpp
void FrontendAgent::sendData(const json& data) {
    try {
        // 检查前置条件
        if (!isConnected()) {
            throw std::runtime_error("WebSocket not connected");
        }
        
        // 执行操作
        json message = data;
        message["timestamp"] = std::chrono::system_clock::now()
                                 .time_since_epoch().count();
        
        // 发送消息
        // ...
        
        return true;
    } catch (const std::exception& e) {
        // 捕获并处理异常，不会导致资源泄漏
        std::cerr << "Error: " << e.what() << std::endl;
        return false;
    }
}
```

#### 3.2 RAII与异常

```cpp
// 即使异常发生，互斥锁也会自动释放
void dispatchEvent(const std::string& eventType, const json& data) {
    {
        std::lock_guard<std::mutex> lock(eventListenersMutex_);
        auto it = eventListeners_.find(eventType);
        if (it != eventListeners_.end()) {
            for (const auto& callback : it->second) {
                callback(data); // 即使callback抛异常，lock仍会释放
            }
        }
    } // lock_guard析构，自动解锁
}
```

### 4. 后台线程管理

#### 4.1 线程生命周期

```cpp
// 创建线程（只有当需要重连时）
if (!reconnectThread_ || !reconnectThread_->joinable()) {
    reconnectThread_ = std::make_unique<std::thread>(
        &FrontendAgent::reconnectLoop, this);
}

// 在析构时等待线程完成
~FrontendAgent() {
    shouldStop_ = true;
    if (reconnectThread_ && reconnectThread_->joinable()) {
        reconnectThread_->join();
    }
}
```

#### 4.2 优雅关闭

```cpp
void FrontendAgent::reconnectLoop() {
    // 循环直到达到最大重试次数或收到停止信号
    while (!shouldStop_ && reconnectAttempts_ < MAX_RECONNECT_ATTEMPTS) {
        std::this_thread::sleep_for(std::chrono::milliseconds(RECONNECT_INTERVAL_MS));
        
        if (shouldStop_) break; // 检查停止标志
        
        try {
            connect();
            if (isConnected()) return;
        } catch (const std::exception& e) {
            std::cerr << "Reconnect failed: " << e.what() << std::endl;
        }
    }
}
```

## 使用指南

### 基本用法

```cpp
#include "client.h"

int main() {
    // 方式1：使用unique_ptr（推荐）
    auto agent = std::make_unique<FrontendAgent>("ws://localhost:8080", "chat");
    
    // 方式2：栈上对象（当作用域明确时使用）
    {
        FrontendAgent agent("ws://localhost:8080", "chat");
        // ... 使用agent
    } // 自动析构
    
    return 0;
}
```

### 事件监听

```cpp
// 注册连接事件
agent->on("connection", [](const json& data) {
    if (data["status"] == "connected") {
        std::cout << "Connected!" << std::endl;
    }
});

// 注册消息事件
agent->on("message", [](const json& data) {
    std::cout << "Message: " << data.dump() << std::endl;
});

// 注册错误事件
agent->on("error", [](const json& data) {
    std::cerr << "Error: " << data["error"] << std::endl;
});

// 移除监听器
agent->off("connection");
```

### 发送数据

```cpp
// 方法1：发送消息
agent->sendMessage("chat", {
    {"text", "Hello!"},
    {"userId", "user123"}
});

// 方法2：发送原始JSON
agent->sendData({
    {"type", "command"},
    {"action", "play_animation"}
});
```

## 依赖项

### 必需库

- **nlohmann/json**: C++ JSON库

  ```bash
  # 使用vcpkg安装
  vcpkg install nlohmann-json:x64-windows
  ```

### 可选库（需集成实际WebSocket库）

- **libwebsockets**: C WebSocket库
- **asio**: Boost.Asio（异步I/O库）
- **websocketpp**: 只有头文件的C++ WebSocket库

## 集成WebSocket库

### 使用websocketpp（推荐）

```cpp
#include <websocketpp/config/asio_client.hpp>
#include <websocketpp/client.hpp>

// 在createWebSocketConnection中使用
typedef websocketpp::client<websocketpp::config::asio_client> client;

bool FrontendAgent::createWebSocketConnection() {
    try {
        client wsClient;
        
        // 配置客户端
        wsClient.set_access_channels(websocketpp::log::alevel::all);
        wsClient.clear_access_channels(websocketpp::log::alevel::frame_payload);
        
        wsClient.init_asio();
        
        // 设置回调
        wsClient.set_open_handler([this](client::connection_hdl hdl) {
            handleWebSocketOpen();
        });
        
        wsClient.set_message_handler([this](client::connection_hdl hdl,
                                           client::message_ptr msg) {
            onMessageReceived(msg->get_payload());
        });
        
        wsClient.set_close_handler([this](client::connection_hdl hdl) {
            handleWebSocketClose(0, "Connection closed");
        });
        
        // 连接
        websocketpp::lib::error_code ec;
        client::connection_ptr con = wsClient.get_connection(buildWebSocketUrl(), ec);
        
        if (ec) {
            std::cerr << "Connection error: " << ec.message() << std::endl;
            return false;
        }
        
        wsConnection_ = con.get();
        wsClient.connect(con);
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "WebSocket init failed: " << e.what() << std::endl;
        return false;
    }
}
```

## 性能考虑

### 1. 事件回调优化

```cpp
// 使用引用避免拷贝
using EventCallback = std::function<void(const json&)>;

// 在回调中避免重复复制JSON
agent->on("message", [](const json& data) {
    // data是引用，无需复制
});
```

### 2. 消息队列限制

```cpp
static constexpr int MESSAGE_QUEUE_MAX_SIZE = 1000;

// 防止无限制的内存增长
if (messageQueue_.size() >= MESSAGE_QUEUE_MAX_SIZE) {
    // 处理队列溢出
}
```

### 3. 锁的粒度

```cpp
// ✅ 推荐：最小化锁的持有时间
{
    std::lock_guard<std::mutex> lock(eventListenersMutex_);
    auto callbacks = eventListeners_[eventType];
} // 立即解锁

for (const auto& callback : callbacks) {
    callback(data); // 在锁外调用回调
}

// ❌ 避免：长时间持有锁
std::lock_guard<std::mutex> lock(eventListenersMutex_);
for (auto& [type, callbacks] : eventListeners_) {
    for (const auto& callback : callbacks) {
        callback(data); // 长时间持有锁
    }
}
```

## 内存泄漏检查

### 使用Valgrind（Linux）

```bash
valgrind --leak-check=full ./program
```

### 使用Visual Studio（Windows）

```cpp
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>

int main() {
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
    
    // 程序代码
    
    return 0;
}
```

## 编译示例（使用CMake）

```cmake
cmake_minimum_required(VERSION 3.10)
project(FrontendAgent)

set(CMAKE_CXX_STANDARD 17)

# 查找依赖
find_package(nlohmann_json REQUIRED)

# 添加库
add_library(frontendagent src/ws/client.cpp)
target_include_directories(frontendagent PUBLIC include)
target_link_libraries(frontendagent nlohmann_json::nlohmann_json)

# 添加示例
add_executable(client_example src/ws/client_example.cpp)
target_link_libraries(client_example frontendagent)
```

## 故障排除

### 连接失败

```cpp
// 检查URL格式
auto agent = std::make_unique<FrontendAgent>(
    "ws://localhost:8080",  // ✅ 正确
    "agent_name"
);

// 确保服务器在线
try {
    agent->connect();
} catch (const std::exception& e) {
    std::cerr << "Failed to connect: " << e.what() << std::endl;
}
```

### 内存泄漏

```cpp
// ✅ 推荐使用智能指针
auto agent = std::make_unique<FrontendAgent>(...);

// ❌ 避免手动内存管理
FrontendAgent* agent = new FrontendAgent(...);
// delete agent; // 容易忘记
```

### 线程死锁

```cpp
// ✅ 使用std::lock_guard和unique_lock自动解锁
{
    std::lock_guard<std::mutex> lock(mutex_);
    // 代码
} // 自动解锁

// ❌ 避免手动加解锁
mutex_.lock();
// ... 代码，可能抛异常
// mutex_.unlock(); // 如果异常发生则不会执行
```

## 最佳实践

1. **始终使用智能指针** - 避免手动内存管理
2. **使用RAII模式** - 资源获取即初始化
3. **及时处理异常** - 使用try-catch确保资源释放
4. **最小化锁的范围** - 减少死锁风险
5. **使用原子操作** - 线程间的同步
6. **文档化回调** - 说明回调执行上下文
7. **定期检查内存** - 使用工具验证无泄漏

## 相关文档

- [C++ 标准库 - 智能指针](https://en.cppreference.com/w/cpp/memory)
- [C++ 线程库](https://en.cppreference.com/w/cpp/thread)
- [nlohmann/json 文档](https://github.com/nlohmann/json)
- [RAII 原则](https://en.cppreference.com/w/cpp/language/raii)
