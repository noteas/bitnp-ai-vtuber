#ifndef FRONTEND_AGENT_H
#define FRONTEND_AGENT_H

#include <atomic>
#include <condition_variable>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <queue>
#include <string>
#include <thread>

using json = nlohmann::json;

// 事件回调类型定义
using EventCallback = std::function<void(const json &)>;

/**
 * @class FrontendAgent
 * @brief WebSocket客户端，用于与后端通信
 *
 * 实现了类似JavaScript FrontendAgent的功能，包括：
 * - WebSocket连接管理
 * - 自动重连机制
 * - 事件系统
 * - 内存安全的资源管理
 */
class FrontendAgent {
public:
	/**
	 * @brief 连接状态枚举
	 */
	enum class ConnectionState {
		DISCONNECTED,
		CONNECTING,
		CONNECTED,
		CLOSING,
		UNKNOWN
	};

	/**
	 * @brief 构造函数
	 * @param serverUrl WebSocket服务器地址
	 * @param agentName Agent名称
	 */
	explicit FrontendAgent(const std::string &serverUrl,
		const std::string &agentName);

	/**
	 * @brief 析构函数
	 * 确保连接被正确关闭，资源被释放
	 */
	~FrontendAgent();

	// 禁用拷贝操作（不安全）
	FrontendAgent(const FrontendAgent &) = delete;
	FrontendAgent &operator=(const FrontendAgent &) = delete;

	// 允许移动操作
	FrontendAgent(FrontendAgent &&other) noexcept;
	FrontendAgent &operator=(FrontendAgent &&other) noexcept;

	/**
	 * @brief 连接到WebSocket服务器
	 */
	void connect();

	/**
	 * @brief 断开连接
	 */
	void disconnect();

	/**
	 * @brief 发送数据
	 * @param data 要发送的JSON数据
	 * @return true 发送成功，false 发送失败
	 */
	bool sendData(const json &data);

	/**
	 * @brief 发送特定类型的消息
	 * @param type 消息类型
	 * @param payload 消息内容
	 * @return true 发送成功，false 发送失败
	 */
	bool sendMessage(const std::string &type, const json &payload);

	/**
	 * @brief 获取当前连接状态
	 * @return ConnectionState 连接状态
	 */
	ConnectionState getConnectionState() const;

	/**
	 * @brief 获取连接状态字符串表示
	 * @return std::string 状态字符串
	 */
	std::string getConnectionStateString() const;

	/**
	 * @brief 注册事件监听器
	 * @param eventType 事件类型
	 * @param callback 回调函数
	 * @return bool 注册成功返回true
	 */
	bool on(const std::string &eventType, EventCallback callback);

	/**
	 * @brief 移除事件监听器
	 * @param eventType 事件类型
	 * @return bool 移除成功返回true
	 */
	bool off(const std::string &eventType);

	/**
	 * @brief 处理接收到的消息
	 * 此方法需要从WebSocket库的回调中调用
	 * @param message 接收到的消息JSON
	 */
	void onMessageReceived(const std::string &messageStr);

	/**
	 * @brief 检查是否已连接
	 * @return bool 已连接返回true
	 */
	bool isConnected() const;

	/**
	 * @brief 获取服务器URL
	 * @return const std::string& 服务器URL
	 */
	const std::string &getServerUrl() const;

	/**
	 * @brief 获取Agent名称
	 * @return const std::string& Agent名称
	 */
	const std::string &getAgentName() const;

private:
	// 配置常量
	static constexpr int MAX_RECONNECT_ATTEMPTS = 5;
	static constexpr int RECONNECT_INTERVAL_MS = 3000;
	static constexpr int MESSAGE_QUEUE_MAX_SIZE = 1000;

	// 成员变量
	std::string serverUrl_;
	std::string agentName_;
	std::atomic<ConnectionState> connectionState_;
	std::atomic<int> reconnectAttempts_;

	// 线程管理
	std::unique_ptr<std::thread> reconnectThread_;
	std::atomic<bool> shouldStop_;

	// 事件管理
	std::map<std::string, std::vector<EventCallback>> eventListeners_;
	mutable std::mutex eventListenersMutex_;

	// 消息队列
	std::queue<json> messageQueue_;
	mutable std::mutex messageQueueMutex_;
	std::condition_variable messageQueueCV_;

	/**
	 * @brief 触发事件
	 * @param eventType 事件类型
	 * @param data 事件数据
	 */
	void dispatchEvent(const std::string &eventType, const json &data);

	/**
	 * @brief 重连逻辑
	 */
	void reconnectLoop();

	/**
	 * @brief 发送WebSocket连接请求
	 * @return bool 成功返回true
	 */
	bool createWebSocketConnection();

	/**
	 * @brief 处理WebSocket打开事件
	 */
	void handleWebSocketOpen();

	/**
	 * @brief 处理WebSocket关闭事件
	 * @param code 关闭码
	 * @param reason 关闭原因
	 */
	void handleWebSocketClose(int code, const std::string &reason);

	/**
	 * @brief 处理WebSocket错误事件
	 * @param error 错误信息
	 */
	void handleWebSocketError(const std::string &error);

	/**
	 * @brief 验证并规范化服务器URL
	 * @param url 原始URL
	 * @return std::string 规范化后的URL
	 */
	std::string normalizeServerUrl(const std::string &url) const;

	/**
	 * @brief 构建WebSocket完整URL
	 * @return std::string 完整的WebSocket URL
	 */
	std::string buildWebSocketUrl() const;

	// WebSocket连接指针（实际实现可使用libwebsockets或asio）
	// 此处作为占位符，实际需要集成WebSocket库
	void *wsConnection_;
};

#endif // FRONTEND_AGENT_H
