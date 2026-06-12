#pragma once

#include "iTcpServerWrapper.h"
#include "hv/TcpServer.h"

#include <memory>
#include <atomic>
#include <mutex>
#include <functional>
#include <queue>
#include <chrono>
#include "comm/StdTimer.h"



class TcpServerWrapperImpl;


/**
 * @brief TCP服务器包装类
 * @author hard2Ser Team
 * @brief 基于libhv实现的TCP服务器，提供PIMPL设计模式的封装
 */
class TcpServerWrapper : public ITcpServerWrapper
{
public:
	TcpServerWrapper();

	~TcpServerWrapper() override;

	bool Start(const std::string& ip = "127.0.0.1", int port = 6001) override;
	void Stop() override;
	bool IsRunning() const override;
	int GetPort() const override;

	bool SendData(int connId, const std::string& data) override;
	int BroadcastData(const std::string& data) override;
	bool DisconnectClient(int connId);

	int GetConnectionCount() const override;
	std::map<int, TcpConnection> GetAllConnections() const override;

	void SetOnConnectionCallback(std::function<void(const TcpConnection&)> callback) override;
	void SetOnDisconnectionCallback(std::function<void(int)> callback) override;
	void SetOnMessageCallback(std::function<void(const OldTcpMessage &)> callback) override;
	void SetOnErrorCallback(std::function<void(int, const std::string&)> callback) override;

	void SetServerConfig(int maxConnection = 1000, bool keepAlive = true, int timeOut = 60) override;


private:
	std::unique_ptr<TcpServerWrapperImpl> m_pImpl;

};


/**
 * @brief 异步发送消息结构体
 */

struct AsyncSendItem
{
	int connId;
	std::string data;
	long long timestamp;
	int retryCount;

	AsyncSendItem(int id, const std::string& msg)
		: connId(id)
		, data(msg)
		, retryCount(0)
	{
		timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::system_clock::now().time_since_epoch()).count();
	}

};


/**
 * @brief TCP服务器实现类
 * @author hard2Ser Team
 * @brief 基于libhv的TcpServer实现具体功能
 */
class TcpServerWrapperImpl :  public ITcpServerWrapper //,public QObject
{
	//Q_OBJECT
public:
	TcpServerWrapperImpl();
	~TcpServerWrapperImpl() override;

	bool Start(const std::string& ip = "127.0.0.1", int port = 8080) override;
	void Stop() override;
	bool IsRunning() const override;
	int GetPort() const override;

	bool SendData(int connId, const std::string& data) override;
	int BroadcastData(const std::string& data) override;

	bool DisconnectClient(int connId) override;

	int GetConnectionCount() const override;
	std::map<int, TcpConnection> GetAllConnections() const override;

	void SetOnConnectionCallback(std::function<void(const TcpConnection&)> callback) override;
	void SetOnDisconnectionCallback(std::function<void(int)> callback) override;
	void SetOnMessageCallback(std::function<void(const OldTcpMessage&)> callback) override;
	void SetOnErrorCallback(std::function<void(int, const std::string&)> callback) override;

	void SetServerConfig(int maxConnections = 1000, bool keepAlive = true, int timeout = 60) override;

	//异步发送机制
	bool SendDataAsync(int connId, const std::string& data);
	void EnableAsyncSend(bool enable = true);
	void SetAsyncSendConfig(int maxQueueSize = 5000, int processInterval = 50);

	//处理新增连接、断开连接操作
	void HandleNewConnection(const hv::SocketChannelPtr& channel);
	void HandleDisconnection(const hv::SocketChannelPtr& channel);

private:
	// libhv回调函数
	void OnConnection(const hv::SocketChannelPtr& channel);
	void OnMessage(const hv::SocketChannelPtr& channel, hv::Buffer* buf);
	void OnWriteComplete(const hv::SocketChannelPtr& channel, hv::Buffer* buf);

	// 辅助函数
	void HandleError(int errorCode, const std::string& errorMessage);
	void CreateConnectionInfo(const hv::SocketChannelPtr& channel, TcpConnection& conn);
	int GenerateConnectionId();

private:
	void OnAsyncSendTimer();

private:
	std::unique_ptr<hv::TcpServer> m_server;

	std::string m_bindIp;
	int m_bindPort;
	std::atomic<bool> m_isRunning;
	std::atomic<int> m_connectionIdCounter;

	// 连接管理
	mutable std::mutex m_connectionsMutex;
	std::map<int, TcpConnection> m_connections;            // connId -> TcpConnection
	std::map<hv::SocketChannelPtr, int> m_channelToConnId; // channel -> connId
	std::map<int, hv::SocketChannelPtr> m_connIdToChannel; // connId -> channel

	// 配置参数
	int m_maxConnections;
	bool m_keepAlive;
	int m_timeout;

	// 回调函数
	std::function<void(const TcpConnection&)> m_onConnectionCallback;
	std::function<void(int)> m_onDisconnectionCallback;
	std::function<void(const OldTcpMessage&)> m_onMessageCallback;
	std::function<void(int, const std::string&)> m_onErrorCallback;

	// 异步发送机制
	std::atomic<bool> m_asyncSendEnabled;
	std::atomic<int> m_maxAsyncQueueSize;
	std::atomic<int> m_asyncProcessInterval;
	mutable std::mutex m_asyncSendMutex;
	std::queue<AsyncSendItem> m_asyncSendQueue;
	std::unique_ptr<StdTimer> m_asyncSendTimer;
	static constexpr int MAX_RETRY_COUNT = 3;
	static constexpr long long MESSAGE_TIMEOUT_MS = 300000;// 5分钟超时
};

