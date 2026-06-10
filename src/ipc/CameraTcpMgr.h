#pragma once

#include "TcpServerWrapper.h"
#include "MessageDefine.h"
#include <memory>
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <queue>
#include <chrono>
#include <QObject>
#include <QTimer>
#include "comm/CSingleton.h"




/**
 * @brief 相机软件TCP客户端信息结构体
 */
struct CameraTcpClientInfo
{
	int socketId;                       // Socket连接ID
	std::string deviceId;               // 设备ID
	std::string deviceStatus;           // 设备状态 "online/offline/destory"
	bool isConnected;                   // 是否连接
	long long lastHeartbeatTime;        // 最后心跳时间
	long long connectTime;              // 连接时间
	TcpConnection connection;           // TCP连接信息

	// 任务状态
	std::string currentTaskId;          // 当前任务ID
	bool isTaskRunning;                 // 是否有任务在运行

	CameraTcpClientInfo()
		: socketId(-1)
		, deviceStatus("offline")
		, isConnected(false),
		lastHeartbeatTime(0)
		, connectTime(0)
		, isTaskRunning(false) {}
};

/**
 * @brief 消息缓存结构
 */
struct CachedMessage
{
	std::string deviceId;               // 目标设备ID
	CameraJsonMessage message;          // 缓存的消息
	long long cacheTime;                // 缓存时间
	int retryCount;                     // 重试次数

	CachedMessage(const std::string& devId, const CameraJsonMessage& msg)
		: deviceId(devId)
		, message(msg)
		, retryCount(0)
	{
		cacheTime = std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::system_clock::now().time_since_epoch()).count();
	}
};



//----------------------------CameraTcpManagerSignals----------------------------------------------
//----------------------------CameraTcpManagerSignals----------------------------------------------
//----------------------------CameraTcpManagerSignals----------------------------------------------

/**
 * @brief 相机软件TCP客户端管理器信号类
 */
class CameraTcpManagerSignals : public QObject
{
	Q_OBJECT

signals:
	// 客户端事件信号
	void SigCameraConnected(const std::string& deviceId);
	void SigTest(const std::string& deviceId);

	void SigCameraDisconnected(const std::string& deviceId);
	void SigCameraDeviceStatus(const std::string& deviceId, const std::string& status);
	void SigCameraTaskFinished(const std::string& deviceId, const std::vector<CameraScanData>& scanData);
	void SigCameraTaskError(const std::string& deviceId, const std::string& errorMsg);

	// 管理器状态信号
	void SigManagerError(int errorCode, const std::string& errorMessage);
};


//----------------------------CameraTcpManager----------------------------------------------
//----------------------------CameraTcpManager----------------------------------------------
//----------------------------CameraTcpManager----------------------------------------------

/**
 * @brief 相机软件TCP客户端管理器类（单例）
 * @author hard2Ser Team
 * @brief 专门管理相机软件TCP连接（端口6001），实现相机软件JSON协议
 */
class CameraTcpManager : public QObject//, public CSingleton<CameraTcpManager>
{

	Q_OBJECT
	//friend class CSingleton<CameraTcpManager>;

public:
	static CameraTcpManager& GetInstance();
	static void DestroyInstance();
	~CameraTcpManager();


	// 服务器管理接口
	bool StartServer(int port = 6001);
	void StopServer();
	bool IsServerRunning() const;
	int GetServerPort() const;

	// 客户端管理接口
	bool SendMessageToCamera(const std::string& deviceId, const CameraJsonMessage& message);
	bool SendTaskStartToCamera(const std::string& deviceId);
	bool SendTaskCancelToCamera(const std::string& deviceId, const std::string& taskId);  // 新增：发送任务取消命令
	bool SendDeviceStatusResponse(const std::string& deviceId, bool success = true, const std::string& msg = "success");
	bool BroadcastMessage(const CameraJsonMessage& message);
	bool DisconnectCamera(const std::string& deviceId);

	// 查询接口
	int GetConnectedCameraCount() const;
	std::vector<CameraTcpClientInfo> GetAllCameras() const;
	CameraTcpClientInfo GetCameraByDeviceId(const std::string& deviceId) const;
	std::vector<std::string> GetOnlineCameraDeviceIds() const;
	bool IsCameraOnline(const std::string& deviceId) const;

	// 配置管理
	void SetHeartbeatTimeout(int timeoutSeconds);
	void SetMaxRetryCount(int maxRetry);
	void EnableMessageCache(bool enable);

	// ★ 回调函数类型定义
	using CameraConnectedCallback = std::function<void(const std::string&)>;
	using CameraDisconnectedCallback = std::function<void(const std::string&)>;
	using CameraTaskFinishedCallback = std::function<void(const std::string&, const std::vector<CameraScanData>&)>;
	using CameraTaskCancelledCallback = std::function<void(const std::string&, bool)>;  // 新增：任务取消回调
	using CameraTaskErrorCallback = std::function<void(const std::string&, const std::string&)>;
	using CameraDeviceStatusCallback = std::function<void(const std::string&, const std::string&)>;
	using CameraManagerErrorCallback = std::function<void(int, const std::string&)>;


	// ★ 回调函数注册接口
	void RegisterCameraConnectedCallback(CameraConnectedCallback callback);
	void RegisterCameraDisconnectedCallback(CameraDisconnectedCallback callback);
	void RegisterCameraTaskFinishedCallback(CameraTaskFinishedCallback callback);
	void RegisterCameraTaskCancelledCallback(CameraTaskCancelledCallback callback);  // 新增：注册任务取消回调
	void RegisterCameraTaskErrorCallback(CameraTaskErrorCallback callback);
	void RegisterCameraDeviceStatusCallback(CameraDeviceStatusCallback callback);
	void RegisterCameraManagerErrorCallback(CameraManagerErrorCallback callback);


	// 信号发送器（保持兼容性，可选使用）
	// 信号访问器
	CameraTcpManagerSignals* GetSignals() { return &m_signals; }

signals:
	void SigTest(std::string data);
	void SigTest2();

private slots:
	// TCP服务器事件处理
	void OnTcpConnection(const TcpConnection& connection);
	void OnTcpDisconnection(int socketId);
	void OnTcpMessage(const OldTcpMessage& message);
	void OnTcpError(int errorCode, const std::string& errorMessage);

	// 定时器处理
	void OnCleanupTimer();
	void OnCacheRetryTimer();

private:
	// 私有构造函数（单例模式）
	explicit CameraTcpManager(QObject* parent = nullptr);


	// 禁用拷贝构造和赋值操作
	CameraTcpManager(const CameraTcpManager&) = delete;
	CameraTcpManager& operator=(const CameraTcpManager&) = delete;

	// 消息处理
	void HandleCameraMessage(int socketId, const std::string& messageData);
	void HandleDeviceStatusMessage(int socketId, const CameraJsonMessage& message);
	void HandleTaskFinishMessage(int socketId, const CameraJsonMessage& message);
	void HandleTaskErrorMessage(int socketId, const CameraJsonMessage& message);
	void HandleTaskCancelAckMessage(int socketId, const CameraJsonMessage& message);  // 新增：处理任务取消确认消息
	void HandleTaskStartAckMessage(int socketId, const CameraJsonMessage& message);  // 新增：处理任务取消确认消息


	// 客户端管理
	void RegisterCamera(int socketId, const std::string& deviceId);
	void UnregisterCamera(int socketId);
	void UpdateCameraHeartbeat(int socketId);
	void UpdateCameraDeviceStatus(const std::string& deviceId, const std::string& status);

	// 缓存管理
	void CacheMessage(const std::string& deviceId, const CameraJsonMessage& message);
	void ProcessCachedMessages();
	void ClearExpiredCache();

	// 辅助函数
	long long GetCurrentTimestamp();
	void SendResponse(int socketId, const CameraJsonMessage& response);
	bool IsValidDeviceId(const std::string& deviceId) const;

private:
	// 单例相关
	static std::unique_ptr<CameraTcpManager> s_instance;
	static std::mutex s_instanceMutex;

	// TCP服务器实例
	std::unique_ptr<TcpServerWrapper> m_tcpServer;

	// 客户端管理
	mutable std::mutex m_camerasMutex;
	std::unordered_map<int, CameraTcpClientInfo> m_socketToCamera;        // socketId -> CameraTcpClientInfo
	std::unordered_map<std::string, int> m_deviceToSocket;               // deviceId -> socketId

	// 消息缓存
	mutable std::mutex m_cacheMutex;
	std::queue<CachedMessage> m_messageCache;                            // 消息缓存队列
	std::atomic<bool> m_cacheEnabled;                                    // 是否启用缓存
	static constexpr size_t MAX_CACHE_SIZE = 1000;                      // 最大缓存大小
	static constexpr int MAX_CACHE_AGE_MS = 300000;                     // 最大缓存时间(5分钟)

	// 配置参数
	std::atomic<int> m_heartbeatTimeout;    // 心跳超时时间(秒)
	std::atomic<int> m_maxRetryCount;       // 最大重试次数

	// 定时器
	QTimer* m_cleanupTimer;                 // 清理定时器
	QTimer* m_cacheRetryTimer;              // 缓存重试定时器

	// 信号对象
	CameraTcpManagerSignals m_signals;

	// ★ 回调函数存储
	std::vector<CameraConnectedCallback> m_connectedCallbacks;
	std::vector<CameraDisconnectedCallback> m_disconnectedCallbacks;
	std::vector<CameraTaskFinishedCallback> m_taskFinishedCallbacks;
	std::vector<CameraTaskCancelledCallback> m_taskCancelledCallbacks;  // 新增：任务取消回调存储
	std::vector<CameraTaskErrorCallback> m_taskErrorCallbacks;
	std::vector<CameraDeviceStatusCallback> m_deviceStatusCallbacks;
	std::vector<CameraManagerErrorCallback> m_managerErrorCallbacks;

	// 回调函数互斥锁
	mutable std::mutex m_callbacksMutex;

	// ★ 回调函数通知方法
	void NotifyCameraConnected(const std::string& deviceId);
	void NotifyCameraDisconnected(const std::string& deviceId);
	void NotifyCameraTaskFinished(const std::string& deviceId, const std::vector<CameraScanData>& scanData);
	void NotifyCameraTaskCancelled(const std::string& deviceId, bool success);  // 新增：通知任务取消结果
	void NotifyCameraTaskError(const std::string& deviceId, const std::string& errorMsg);
	void NotifyCameraDeviceStatus(const std::string& deviceId, const std::string& status);
	void NotifyCameraManagerError(int errorCode, const std::string& errorMessage);

};

