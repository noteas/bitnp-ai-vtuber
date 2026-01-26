#include "client.h"
#include <iostream>
#include <memory>

/**
 * @example client_example.cpp
 * FrontendAgent 使用示例
 *
 * 展示如何安全地使用 FrontendAgent 进行 WebSocket 通信
 */

int main() {
	// 使用 std::make_unique 确保内存安全
	auto agent = std::make_unique<FrontendAgent>("ws://localhost:8080", "chatbot");

	// 注册事件监听器
	agent->on("connection", [](const json &data) {
		std::cout << "Connection event: " << data["status"] << std::endl;
	});

	agent->on("message", [](const json &data) {
		std::cout << "Received message: " << data.dump() << std::endl;
	});

	agent->on("error", [](const json &data) {
		std::cerr << "Error: " << data["error"] << std::endl;
	});

	// 连接到服务器
	agent->connect();

	// 等待连接建立
	std::this_thread::sleep_for(std::chrono::seconds(2));

	// 发送消息
	if (agent->isConnected()) {
		agent->sendMessage("chat", { { "text", "Hello, server!" }, { "userId", "user123" } });
	}

	// 获取连接状态
	std::cout << "Connection state: " << agent->getConnectionStateString() << std::endl;

	// 保持运行一段时间
	std::this_thread::sleep_for(std::chrono::seconds(5));

	// 断开连接
	agent->disconnect();

	// agent 会在作用域结束时自动析构并清理资源

	return 0;
}
