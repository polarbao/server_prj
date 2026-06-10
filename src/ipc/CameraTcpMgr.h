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
 * @brief ������TCP�ͻ�����Ϣ�ṹ��
 */
struct CameraTcpClientInfo
{
	int socketId;                       // Socket����ID
	std::string deviceId;               // �豸ID
	std::string deviceStatus;           // �豸״̬ "online/offline/destory"
	bool isConnected;                   // �Ƿ�����
	long long lastHeartbeatTime;        // �������ʱ��
	long long connectTime;              // ����ʱ��
	TcpConnection connection;           // TCP������Ϣ

	// ����״̬
	std::string currentTaskId;          // ��ǰ����ID
	bool isTaskRunning;                 // �Ƿ�������������

	CameraTcpClientInfo()
		: socketId(-1)
		, deviceStatus("offline")
		, isConnected(false),
		lastHeartbeatTime(0)
		, connectTime(0)
		, isTaskRunning(false) {}
};

/**
 * @brief ��Ϣ����ṹ
 */
struct CachedMessage
{
	std::string deviceId;               // Ŀ���豸ID
	CameraJsonMessage message;          // �������Ϣ
	long long cacheTime;                // ����ʱ��
	int retryCount;                     // ���Դ���

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
 * @brief ������TCP�ͻ��˹������ź���
 */
class CameraTcpManagerSignals : public QObject
{
	Q_OBJECT

signals:
	// �ͻ����¼��ź�
	void SigCameraConnected(const std::string& deviceId);
	void SigTest(const std::string& deviceId);

	void SigCameraDisconnected(const std::string& deviceId);
	void SigCameraDeviceStatus(const std::string& deviceId, const std::string& status);
	void SigCameraTaskFinished(const std::string& deviceId, const std::vector<CameraScanData>& scanData);
	void SigCameraTaskError(const std::string& deviceId, const std::string& errorMsg);

	// ������״̬�ź�
	void SigManagerError(int errorCode, const std::string& errorMessage);
};


//----------------------------CameraTcpManager----------------------------------------------
//----------------------------CameraTcpManager----------------------------------------------
//----------------------------CameraTcpManager----------------------------------------------

/**
 * @brief ������TCP�ͻ��˹������ࣨ������
 * @author hard2Ser Team
 * @brief ר�Ź���������TCP���ӣ��˿�6001����ʵ��������JSONЭ��
 */
class CameraTcpManager : public QObject//, public CSingleton<CameraTcpManager>
{

	Q_OBJECT
	//friend class CSingleton<CameraTcpManager>;

public:
	static CameraTcpManager& GetInstance();
	static void DestroyInstance();
	~CameraTcpManager();


	// ����������ӿ�
	bool StartServer(int port = 6001);
	void StopServer();
	bool IsServerRunning() const;
	int GetServerPort() const;

	// �ͻ��˹���ӿ�
	bool SendMessageToCamera(const std::string& deviceId, const CameraJsonMessage& message);
	bool SendTaskStartToCamera(const std::string& deviceId);
	bool SendTaskCancelToCamera(const std::string& deviceId, const std::string& taskId);  // ��������������ȡ������
	bool SendDeviceStatusResponse(const std::string& deviceId, bool success = true, const std::string& msg = "success");
	bool BroadcastMessage(const CameraJsonMessage& message);
	bool DisconnectCamera(const std::string& deviceId);

	// ��ѯ�ӿ�
	int GetConnectedCameraCount() const;
	std::vector<CameraTcpClientInfo> GetAllCameras() const;
	CameraTcpClientInfo GetCameraByDeviceId(const std::string& deviceId) const;
	std::vector<std::string> GetOnlineCameraDeviceIds() const;
	bool IsCameraOnline(const std::string& deviceId) const;

	// ���ù���
	void SetHeartbeatTimeout(int timeoutSeconds);
	void SetMaxRetryCount(int maxRetry);
	void EnableMessageCache(bool enable);

	// �� �ص��������Ͷ���
	using CameraConnectedCallback = std::function<void(const std::string&)>;
	using CameraDisconnectedCallback = std::function<void(const std::string&)>;
	using CameraTaskFinishedCallback = std::function<void(const std::string&, const std::vector<CameraScanData>&)>;
	using CameraTaskCancelledCallback = std::function<void(const std::string&, bool)>;  // ����������ȡ���ص�
	using CameraTaskErrorCallback = std::function<void(const std::string&, const std::string&)>;
	using CameraDeviceStatusCallback = std::function<void(const std::string&, const std::string&)>;
	using CameraManagerErrorCallback = std::function<void(int, const std::string&)>;


	// �� �ص�����ע��ӿ�
	void RegisterCameraConnectedCallback(CameraConnectedCallback callback);
	void RegisterCameraDisconnectedCallback(CameraDisconnectedCallback callback);
	void RegisterCameraTaskFinishedCallback(CameraTaskFinishedCallback callback);
	void RegisterCameraTaskCancelledCallback(CameraTaskCancelledCallback callback);  // ������ע������ȡ���ص�
	void RegisterCameraTaskErrorCallback(CameraTaskErrorCallback callback);
	void RegisterCameraDeviceStatusCallback(CameraDeviceStatusCallback callback);
	void RegisterCameraManagerErrorCallback(CameraManagerErrorCallback callback);


	// �źŷ����������ּ����ԣ���ѡʹ�ã�
	// �źŷ�����
	CameraTcpManagerSignals* GetSignals() { return &m_signals; }

signals:
	void SigTest(std::string data);
	void SigTest2();

private slots:
	// TCP�������¼�����
	void OnTcpConnection(const TcpConnection& connection);
	void OnTcpDisconnection(int socketId);
	void OnTcpMessage(const OldTcpMessage& message);
	void OnTcpError(int errorCode, const std::string& errorMessage);

	// ��ʱ������
	void OnCleanupTimer();
	void OnCacheRetryTimer();

private:
	// ˽�й��캯��������ģʽ��
	explicit CameraTcpManager(QObject* parent = nullptr);


	// ���ÿ�������͸�ֵ����
	CameraTcpManager(const CameraTcpManager&) = delete;
	CameraTcpManager& operator=(const CameraTcpManager&) = delete;

	// ��Ϣ����
	void HandleCameraMessage(int socketId, const std::string& messageData);
	void HandleDeviceStatusMessage(int socketId, const CameraJsonMessage& message);
	void HandleTaskFinishMessage(int socketId, const CameraJsonMessage& message);
	void HandleTaskErrorMessage(int socketId, const CameraJsonMessage& message);
	void HandleTaskCancelAckMessage(int socketId, const CameraJsonMessage& message);  // ��������������ȡ��ȷ����Ϣ
	void HandleTaskStartAckMessage(int socketId, const CameraJsonMessage& message);  // ��������������ȡ��ȷ����Ϣ


	// �ͻ��˹���
	void RegisterCamera(int socketId, const std::string& deviceId);
	void UnregisterCamera(int socketId);
	void UpdateCameraHeartbeat(int socketId);
	void UpdateCameraDeviceStatus(const std::string& deviceId, const std::string& status);

	// �������
	void CacheMessage(const std::string& deviceId, const CameraJsonMessage& message);
	void ProcessCachedMessages();
	void ClearExpiredCache();

	// ��������
	long long GetCurrentTimestamp();
	void SendResponse(int socketId, const CameraJsonMessage& response);
	bool IsValidDeviceId(const std::string& deviceId) const;

private:
	// �������
	static std::unique_ptr<CameraTcpManager> s_instance;
	static std::mutex s_instanceMutex;

	// TCP������ʵ��
	std::unique_ptr<TcpServerWrapper> m_tcpServer;

	// �ͻ��˹���
	mutable std::mutex m_camerasMutex;
	std::unordered_map<int, CameraTcpClientInfo> m_socketToCamera;        // socketId -> CameraTcpClientInfo
	std::unordered_map<std::string, int> m_deviceToSocket;               // deviceId -> socketId

	// ��Ϣ����
	mutable std::mutex m_cacheMutex;
	std::queue<CachedMessage> m_messageCache;                            // ��Ϣ�������
	std::atomic<bool> m_cacheEnabled;                                    // �Ƿ����û���
	static constexpr size_t MAX_CACHE_SIZE = 1000;                      // ��󻺴��С
	static constexpr int MAX_CACHE_AGE_MS = 300000;                     // ��󻺴�ʱ��(5����)

	// ���ò���
	std::atomic<int> m_heartbeatTimeout;    // ������ʱʱ��(��)
	std::atomic<int> m_maxRetryCount;       // ������Դ���

	// ��ʱ��
	QTimer* m_cleanupTimer;                 // �����ʱ��
	QTimer* m_cacheRetryTimer;              // �������Զ�ʱ��

	// �źŶ���
	CameraTcpManagerSignals m_signals;

	// �� �ص������洢
	std::vector<CameraConnectedCallback> m_connectedCallbacks;
	std::vector<CameraDisconnectedCallback> m_disconnectedCallbacks;
	std::vector<CameraTaskFinishedCallback> m_taskFinishedCallbacks;
	std::vector<CameraTaskCancelledCallback> m_taskCancelledCallbacks;  // ����������ȡ���ص��洢
	std::vector<CameraTaskErrorCallback> m_taskErrorCallbacks;
	std::vector<CameraDeviceStatusCallback> m_deviceStatusCallbacks;
	std::vector<CameraManagerErrorCallback> m_managerErrorCallbacks;

	// �ص�����������
	mutable std::mutex m_callbacksMutex;

	// �� �ص�����֪ͨ����
	void NotifyCameraConnected(const std::string& deviceId);
	void NotifyCameraDisconnected(const std::string& deviceId);
	void NotifyCameraTaskFinished(const std::string& deviceId, const std::vector<CameraScanData>& scanData);
	void NotifyCameraTaskCancelled(const std::string& deviceId, bool success);  // ������֪ͨ����ȡ�����
	void NotifyCameraTaskError(const std::string& deviceId, const std::string& errorMsg);
	void NotifyCameraDeviceStatus(const std::string& deviceId, const std::string& status);
	void NotifyCameraManagerError(int errorCode, const std::string& errorMessage);

};

