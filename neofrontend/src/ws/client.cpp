#include "client.h"
#include <chrono>
#include <iostream>

/**
 * @brief 构造函数
 */
FrontendAgent::FrontendAgent(const std::string &serverUrl, const std::string &agentName) : serverUrl_(normalizeServerUrl(serverUrl)), agentName_(agentName), connectionState_(ConnectionState::DISCONNECTED), reconnectAttempts_(0), shouldStop_(false), wsConnection_(nullptr) {
	// 初始化时不自动连接，需要显式调用connect()
	std::cout << "FrontendAgent initialized" << std::endl
			  << "  Server URL: " << serverUrl_ << std::endl
			  << "  Agent Name: " << agentName_ << std::endl;
}

/**
 * @brief 析构函数
 */
FrontendAgent::~FrontendAgent() {
	// 停止重连线程
	shouldStop_ = true;

	// 断开连接
	disconnect();

	// 等待重连线程完成
	if (reconnectThread_ && reconnectThread_->joinable()) {
		reconnectThread_->join();
	}

	// 清理事件监听器
	{
		std::lock_guard<std::mutex> lock(eventListenersMutex_);
		eventListeners_.clear();
	}

	std::cout << "FrontendAgent destroyed" << std::endl;
}

/**
 * @brief 移动构造函数
 */
FrontendAgent::FrontendAgent(FrontendAgent &&other) noexcept
	: serverUrl_(std::move(other.serverUrl_)),
	  agentName_(std::move(other.agentName_)),
	  connectionState_(other.connectionState_.load()),
	  reconnectAttempts_(other.reconnectAttempts_.load()),
	  shouldStop_(other.shouldStop_.load()),
	  wsConnection_(other.wsConnection_) {
	other.wsConnection_ = nullptr;
	other.shouldStop_ = true;
}

/**
 * @brief 移动赋值操作符
 */
FrontendAgent &FrontendAgent::operator=(FrontendAgent &&other) noexcept {
	if (this != &other) {
		disconnect();
		shouldStop_ = true;

		serverUrl_ = std::move(other.serverUrl_);
		agentName_ = std::move(other.agentName_);
		connectionState_ = other.connectionState_.load();
		reconnectAttempts_ = other.reconnectAttempts_.load();
		shouldStop_ = other.shouldStop_.load();
		wsConnection_ = other.wsConnection_;

		other.wsConnection_ = nullptr;
		other.shouldStop_ = true;
	}
	return *this;
}

/**
 * @brief 规范化服务器URL
 */
std::string FrontendAgent::normalizeServerUrl(const std::string &url) const {
	std::string result = url;

	// 移除前导斜杠
	size_t start = 0;
	while (start < result.length() && result[start] == '/') {
		start++;
	}
	result = result.substr(start);

	// 移除尾部斜杠
	size_t end = result.length();
	while (end > 0 && result[end - 1] == '/') {
		end--;
	}
	result = result.substr(0, end);

	// 检查是否已经有协议前缀
	if (result.substr(0, 6) == "wss://" || result.substr(0, 5) == "ws://") {
		return result;
	}

	// 如果没有协议前缀，根据HTTPS/HTTP决定使用wss/ws
	// 这里假设在HTTPS页面上使用wss，HTTP页面上使用ws
	// 由于是C++程序，我们默认使用ws://（可根据实际需求修改）
	return "ws://" + result;
}

/**
 * @brief 构建完整的WebSocket URL
 */
std::string FrontendAgent::buildWebSocketUrl() const {
	return serverUrl_ + "/ws/frontend/" + agentName_;
}

/**
 * @brief 连接到WebSocket服务器
 */
void FrontendAgent::connect() {
	try {
		connectionState_ = ConnectionState::CONNECTING;

		if (!createWebSocketConnection()) {
			std::cerr << "Failed to create WebSocket connection" << std::endl;
			connectionState_ = ConnectionState::DISCONNECTED;
			dispatchEvent("error", { { "error", "Failed to create connection" }, { "originalError", "WebSocket creation failed" } });
			return;
		}

		std::cout << "WebSocket connection initiated: " << buildWebSocketUrl() << std::endl;
	} catch (const std::exception &e) {
		std::cerr << "Exception during connect: " << e.what() << std::endl;
		connectionState_ = ConnectionState::DISCONNECTED;
		dispatchEvent("error", { { "error", "Failed to create connection" }, { "originalError", e.what() } });
	}
}

/**
 * @brief 创建WebSocket连接
 */
bool FrontendAgent::createWebSocketConnection() {
	try {
		// 这里需要集成实际的WebSocket库（如libwebsockets或asio）
		// 下面是伪代码示例

		std::string wsUrl = buildWebSocketUrl();

		// TODO: 使用WebSocket库创建连接
		// wsConnection_ = websocket_connect(wsUrl.c_str());

		// 模拟连接成功
		handleWebSocketOpen();
		return true;
	} catch (const std::exception &e) {
		std::cerr << "WebSocket connection failed: " << e.what() << std::endl;
		return false;
	}
}

/**
 * @brief 处理WebSocket打开事件
 */
void FrontendAgent::handleWebSocketOpen() {
	connectionState_ = ConnectionState::CONNECTED;
	reconnectAttempts_ = 0;

	json eventData = {
		{ "status", "connected" }
	};
	dispatchEvent("connection", eventData);
	std::cout << "WebSocket connection opened" << std::endl;
}

/**
 * @brief 处理WebSocket关闭事件
 */
void FrontendAgent::handleWebSocketClose(int code, const std::string &reason) {
	connectionState_ = ConnectionState::DISCONNECTED;

	json eventData = {
		{ "status", "disconnected" },
		{ "code", code },
		{ "reason", reason }
	};
	dispatchEvent("connection", eventData);
	std::cout << "WebSocket connection closed: " << code << " - " << reason << std::endl;

	// 启动重连逻辑
	if (reconnectAttempts_ < MAX_RECONNECT_ATTEMPTS) {
		if (!reconnectThread_ || !reconnectThread_->joinable()) {
			reconnectThread_ = std::make_unique<std::thread>(&FrontendAgent::reconnectLoop, this);
		}
	}
}

/**
 * @brief 处理WebSocket错误事件
 */
void FrontendAgent::handleWebSocketError(const std::string &error) {
	json eventData = {
		{ "error", "WebSocket connection error" },
		{ "originalError", error }
	};
	dispatchEvent("error", eventData);
	std::cerr << "WebSocket error: " << error << std::endl;
}

/**
 * @brief 重连逻辑
 */
void FrontendAgent::reconnectLoop() {
	while (!shouldStop_ && reconnectAttempts_ < MAX_RECONNECT_ATTEMPTS) {
		std::chrono::milliseconds waitTime(RECONNECT_INTERVAL_MS);
		std::this_thread::sleep_for(waitTime);

		if (shouldStop_) {
			break;
		}

		reconnectAttempts_++;
		std::cout << "Attempting to reconnect (" << reconnectAttempts_
				  << "/" << MAX_RECONNECT_ATTEMPTS << ")..." << std::endl;

		try {
			connect();
			if (isConnected()) {
				return; // 重连成功
			}
		} catch (const std::exception &e) {
			std::cerr << "Reconnect attempt failed: " << e.what() << std::endl;
		}
	}

	if (reconnectAttempts_ >= MAX_RECONNECT_ATTEMPTS) {
		std::cout << "Max reconnection attempts reached" << std::endl;
	}
}

/**
 * @brief 断开连接
 */
void FrontendAgent::disconnect() {
	if (connectionState_ == ConnectionState::DISCONNECTED) {
		return;
	}

	connectionState_ = ConnectionState::CLOSING;
	reconnectAttempts_ = MAX_RECONNECT_ATTEMPTS; // 停止自动重连

	try {
		// TODO: 使用WebSocket库关闭连接
		// if (wsConnection_) {
		//     websocket_close(wsConnection_);
		//     wsConnection_ = nullptr;
		// }

		connectionState_ = ConnectionState::DISCONNECTED;
		std::cout << "WebSocket disconnected" << std::endl;
	} catch (const std::exception &e) {
		std::cerr << "Error during disconnect: " << e.what() << std::endl;
	}
}

/**
 * @brief 发送数据
 */
bool FrontendAgent::sendData(const json &data) {
	if (!isConnected()) {
		std::cerr << "WebSocket is not connected" << std::endl;
		dispatchEvent("error", { { "error", "WebSocket is not connected" } });
		return false;
	}

	try {
		// 构建完整消息
		json message = data;
		message["agentName"] = agentName_;
		message["timestamp"] = std::chrono::system_clock::now().time_since_epoch().count();

		std::string messageStr = message.dump();

		// TODO: 使用WebSocket库发送消息
		// if (websocket_send(wsConnection_, messageStr.c_str(), messageStr.length()) != 0) {
		//     throw std::runtime_error("Failed to send message");
		// }

		std::cout << "Data sent successfully: " << messageStr << std::endl;
		return true;
	} catch (const std::exception &e) {
		std::cerr << "Error sending data: " << e.what() << std::endl;
		dispatchEvent("error", { { "error", "Failed to send data" }, { "originalError", e.what() } });
		return false;
	}
}

/**
 * @brief 发送特定类型的消息
 */
bool FrontendAgent::sendMessage(const std::string &type, const json &payload) {
	return sendData({ { "type", type },
		{ "payload", payload } });
}

/**
 * @brief 获取连接状态
 */
FrontendAgent::ConnectionState FrontendAgent::getConnectionState() const {
	return connectionState_.load();
}

/**
 * @brief 获取连接状态字符串
 */
std::string FrontendAgent::getConnectionStateString() const {
	switch (getConnectionState()) {
		case ConnectionState::CONNECTING:
			return "connecting";
		case ConnectionState::CONNECTED:
			return "connected";
		case ConnectionState::CLOSING:
			return "closing";
		case ConnectionState::DISCONNECTED:
			return "disconnected";
		case ConnectionState::UNKNOWN:
		default:
			return "unknown";
	}
}

/**
 * @brief 注册事件监听器
 */
bool FrontendAgent::on(const std::string &eventType, EventCallback callback) {
	try {
		std::lock_guard<std::mutex> lock(eventListenersMutex_);
		eventListeners_[eventType].push_back(callback);
		std::cout << "Event listener registered for: " << eventType << std::endl;
		return true;
	} catch (const std::exception &e) {
		std::cerr << "Error registering event listener: " << e.what() << std::endl;
		return false;
	}
}

/**
 * @brief 移除事件监听器
 */
bool FrontendAgent::off(const std::string &eventType) {
	try {
		std::lock_guard<std::mutex> lock(eventListenersMutex_);
		auto it = eventListeners_.find(eventType);
		if (it != eventListeners_.end()) {
			eventListeners_.erase(it);
			std::cout << "Event listener removed for: " << eventType << std::endl;
			return true;
		}
		return false;
	} catch (const std::exception &e) {
		std::cerr << "Error removing event listener: " << e.what() << std::endl;
		return false;
	}
}

/**
 * @brief 触发事件
 */
void FrontendAgent::dispatchEvent(const std::string &eventType, const json &data) {
	try {
		std::lock_guard<std::mutex> lock(eventListenersMutex_);
		auto it = eventListeners_.find(eventType);
		if (it != eventListeners_.end()) {
			for (const auto &callback : it->second) {
				try {
					callback(data);
				} catch (const std::exception &e) {
					std::cerr << "Error in event callback: " << e.what() << std::endl;
				}
			}
		}
	} catch (const std::exception &e) {
		std::cerr << "Error dispatching event: " << e.what() << std::endl;
	}
}

/**
 * @brief 处理接收到的消息
 */
void FrontendAgent::onMessageReceived(const std::string &messageStr) {
	try {
		json message = json::parse(messageStr);

		std::cout << "Message received: " << message.dump() << std::endl;

		// 检查是否有特定的消息类型
		if (message.contains("data") && message["data"].is_object() &&
			message["data"].contains("type")) {
			std::string msgType = message["data"]["type"];

			// 触发特定类型的事件
			dispatchEvent(msgType, message["data"]);
		}

		// 同时触发通用消息事件
		dispatchEvent("message", message);
	} catch (const json::exception &e) {
		std::cerr << "Error parsing message JSON: " << e.what() << std::endl;
		dispatchEvent("error", { { "error", "Failed to parse message" }, { "originalData", messageStr } });
	} catch (const std::exception &e) {
		std::cerr << "Error handling message: " << e.what() << std::endl;
	}
}

/**
 * @brief 检查是否已连接
 */
bool FrontendAgent::isConnected() const {
	return getConnectionState() == ConnectionState::CONNECTED;
}

/**
 * @brief 获取服务器URL
 */
const std::string &FrontendAgent::getServerUrl() const {
	return serverUrl_;
}

/**
 * @brief 获取Agent名称
 */
const std::string &FrontendAgent::getAgentName() const {
	return agentName_;
}
