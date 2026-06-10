#include "TcpServerWrapper.h"
#include "MessageDefine.h"
#include <memory>
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <chrono>
#include <QObject>


/**
 * @brief TCP客户端信息结构体（内部使用）
 */
struct TcpClientInfo
{
	int socketId;                       // Socket连接ID
	std::string clientId;               // 客户端唯一ID
	std::string deviceId;               // 设备ID
	TcpClientType clientType;           // 客户端类型
	std::string clientVersion;          // 客户端版本
	TcpThreadStatus threadStatus;       // 当前线程状态
	std::string clientName;             // 客户端名称
	bool isRegistered;                  // 是否已注册
	long long lastHeartbeatTime;        // 最后心跳时间
	long long registerTime;             // 注册时间
	TcpConnection connection;           // TCP连接信息

	TcpClientInfo()
		: socketId(-1)
		, clientType(TcpClientType::UNKNOWN)
		, threadStatus(TcpThreadStatus::OFFLINE)
		, isRegistered(false)
		, lastHeartbeatTime(0)
		, registerTime(0) {}
};



//----------------------------TcpClientManagerSignals----------------------------------------------
//----------------------------TcpClientManagerSignals----------------------------------------------
//----------------------------TcpClientManagerSignals----------------------------------------------

/**
 * @brief TCP客户端管理器信号类
 */
class TcpClientManagerSignals : public QObject
{
	Q_OBJECT

signals:
	// 客户端事件信号
	void SigClientConnected(const TcpClientInfo& clientInfo);
	void SigClientDisconnected(const std::string& deviceId);
	void SigClientRegistered(const TcpClientInfo& clientInfo);
	void SigClientHeartbeat(const TcpClientHeartbeat& heartbeat);
	void SigClientStatusUpdate(const TcpClientDeviceStatus& deviceStatus);
	void SigClientTaskStatusUpdate(const TcpClientTaskStatus& taskStatus);

	// 管理器状态信号
	void SigManagerError(int errorCode, const std::string& errorMessage);
};


//----------------------------TcpClientManager----------------------------------------------
//----------------------------TcpClientManager----------------------------------------------
//----------------------------TcpClientManager----------------------------------------------


/**
 * @brief TCP客户端管理器类
 * @author hard2Ser Team
 * @brief 管理TCP客户端连接，维护设备ID与Socket ID的映射关系
 */
class TcpClientManager : public QObject
{
	Q_OBJECT

public:
	explicit TcpClientManager(QObject* parent = nullptr);
	~TcpClientManager();

	// 服务器管理接口
	bool StartServer(const std::string& ip = "0.0.0.0", int port = 9000);
	void StopServer();
	bool IsServerRunning() const;
	int GetServerPort() const;

	// 客户端管理接口
	bool SendDataToDevice(const std::string& deviceId, const std::string& data);
	bool SendTaskDispatch(const std::string& deviceId, const TcpServerTaskDispatch& taskDispatch);
	bool SendTaskControl(const std::string& deviceId, const TcpServerTaskControl& taskControl);
	bool BroadcastData(const std::string& data);
	bool DisconnectClient(const std::string& deviceId);

	// 查询接口
	int GetConnectedClientCount() const;
	std::vector<TcpClientInfo> GetAllClients() const;
	TcpClientInfo GetClientByDeviceId(const std::string& deviceId) const;
	TcpClientInfo GetClientBySocketId(int socketId) const;
	std::vector<TcpClientInfo> GetClientsByType(TcpClientType clientType) const;

	// 配置管理
	void SetHeartbeatTimeout(int timeoutSeconds);
	void SetMaxClients(int maxClients);
	void EnableAutoCleanup(bool enable, int cleanupInterval = 60);

	// 信号访问器
	TcpClientManagerSignals* GetSignals() { return &m_signals; }

private slots:
	// TCP服务器事件处理
	void OnTcpConnection(const TcpConnection& connection);
	void OnTcpDisconnection(int socketId);
	void OnTcpMessage(const TcpMessage& message);
	void OnTcpError(int errorCode, const std::string& errorMessage);

private:
	// 消息处理
	void HandleClientRegister(int socketId, const std::string& payload);
	void HandleClientHeartbeat(int socketId, const std::string& payload);
	void HandleClientDeviceStatus(int socketId, const std::string& payload);
	void HandleClientTaskStatus(int socketId, const std::string& payload);

	// 客户端管理
	bool RegisterClient(int socketId, const TcpClientRegisterInfo& registerInfo);
	void UnregisterClient(int socketId);
	void UpdateClientHeartbeat(int socketId, const TcpClientHeartbeat& heartbeat);

	// 辅助函数
	std::string GenerateMessageId();
	long long GetCurrentTimestamp();
	void CleanupTimeoutClients();
	void SendHeartbeatAck(int socketId, const std::string& clientId);
	bool IsValidDeviceId(const std::string& deviceId) const;
	bool IsClientTypeAllowed(TcpClientType clientType) const;

private:
	// TCP服务器实例
	std::unique_ptr<TcpServerWrapper> m_tcpServer;

	// 客户端管理
	mutable std::mutex m_clientsMutex;
	std::unordered_map<int, TcpClientInfo> m_socketToClient;        // socketId -> TcpClientInfo
	std::unordered_map<std::string, int> m_deviceToSocket;          // deviceId -> socketId
	std::unordered_map<std::string, int> m_clientIdToSocket;        // clientId -> socketId

	// 配置参数
	std::atomic<int> m_heartbeatTimeout;    // 心跳超时时间(秒)
	std::atomic<int> m_maxClients;          // 最大客户端数
	std::atomic<bool> m_autoCleanup;        // 是否启用自动清理
	std::atomic<int> m_cleanupInterval;     // 清理间隔(秒)

	// 统计信息
	std::atomic<int> m_messageIdCounter;    // 消息ID计数器
	std::atomic<long long> m_totalMessages; // 总消息数

	// 定时器
	QTimer* m_cleanupTimer;

	// 信号对象
	TcpClientManagerSignals m_signals;
};



//----------------------------TcpClientManagerInstance----------------------------------------------
//----------------------------TcpClientManagerInstance----------------------------------------------
//----------------------------TcpClientManagerInstance----------------------------------------------

/**
 * @brief TCP客户端管理器实现类（单例模式）
 * @brief 提供全局访问的TCP客户端管理服务
 */
class TcpClientManagerInstance
{
public:
	static TcpClientManager& GetInstance();
	static void DestroyInstance();

private:
	TcpClientManagerInstance() = default;
	~TcpClientManagerInstance() = default;

	static std::unique_ptr<TcpClientManager> s_instance;
	static std::mutex s_instanceMutex;
};

