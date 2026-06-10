#include "TcpServerWrapper.h"
#include "MessageDefine.h"
#include <memory>
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <chrono>
#include <QObject>


/**
 * @brief TCP�ͻ�����Ϣ�ṹ�壨�ڲ�ʹ�ã�
 */
struct TcpClientInfo
{
	int socketId;                       // Socket����ID
	std::string clientId;               // �ͻ���ΨһID
	std::string deviceId;               // �豸ID
	TcpClientType clientType;           // �ͻ�������
	std::string clientVersion;          // �ͻ��˰汾
	TcpThreadStatus threadStatus;       // ��ǰ�߳�״̬
	std::string clientName;             // �ͻ�������
	bool isRegistered;                  // �Ƿ���ע��
	long long lastHeartbeatTime;        // �������ʱ��
	long long registerTime;             // ע��ʱ��
	TcpConnection connection;           // TCP������Ϣ

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
 * @brief TCP�ͻ��˹������ź���
 */
class TcpClientManagerSignals : public QObject
{
	Q_OBJECT

signals:
	// �ͻ����¼��ź�
	void SigClientConnected(const TcpClientInfo& clientInfo);
	void SigClientDisconnected(const std::string& deviceId);
	void SigClientRegistered(const TcpClientInfo& clientInfo);
	void SigClientHeartbeat(const TcpClientHeartbeat& heartbeat);
	void SigClientStatusUpdate(const TcpClientDeviceStatus& deviceStatus);
	void SigClientTaskStatusUpdate(const TcpClientTaskStatus& taskStatus);

	// ������״̬�ź�
	void SigManagerError(int errorCode, const std::string& errorMessage);
};


//----------------------------TcpClientManager----------------------------------------------
//----------------------------TcpClientManager----------------------------------------------
//----------------------------TcpClientManager----------------------------------------------


/**
 * @brief TCP�ͻ��˹�������
 * @author hard2Ser Team
 * @brief ����TCP�ͻ������ӣ�ά���豸ID��Socket ID��ӳ���ϵ
 */
class TcpClientManager : public QObject
{
	Q_OBJECT

public:
	explicit TcpClientManager(QObject* parent = nullptr);
	~TcpClientManager();

	// ����������ӿ�
	bool StartServer(const std::string& ip = "0.0.0.0", int port = 9000);
	void StopServer();
	bool IsServerRunning() const;
	int GetServerPort() const;

	// �ͻ��˹���ӿ�
	bool SendDataToDevice(const std::string& deviceId, const std::string& data);
	bool SendTaskDispatch(const std::string& deviceId, const TcpServerTaskDispatch& taskDispatch);
	bool SendTaskControl(const std::string& deviceId, const TcpServerTaskControl& taskControl);
	bool BroadcastData(const std::string& data);
	bool DisconnectClient(const std::string& deviceId);

	// ��ѯ�ӿ�
	int GetConnectedClientCount() const;
	std::vector<TcpClientInfo> GetAllClients() const;
	TcpClientInfo GetClientByDeviceId(const std::string& deviceId) const;
	TcpClientInfo GetClientBySocketId(int socketId) const;
	std::vector<TcpClientInfo> GetClientsByType(TcpClientType clientType) const;

	// ���ù���
	void SetHeartbeatTimeout(int timeoutSeconds);
	void SetMaxClients(int maxClients);
	void EnableAutoCleanup(bool enable, int cleanupInterval = 60);

	// �źŷ�����
	TcpClientManagerSignals* GetSignals() { return &m_signals; }

private slots:
	// TCP�������¼�����
	void OnTcpConnection(const TcpConnection& connection);
	void OnTcpDisconnection(int socketId);
	void OnTcpMessage(const TcpMessage& message);
	void OnTcpError(int errorCode, const std::string& errorMessage);

private:
	// ��Ϣ����
	void HandleClientRegister(int socketId, const std::string& payload);
	void HandleClientHeartbeat(int socketId, const std::string& payload);
	void HandleClientDeviceStatus(int socketId, const std::string& payload);
	void HandleClientTaskStatus(int socketId, const std::string& payload);

	// �ͻ��˹���
	bool RegisterClient(int socketId, const TcpClientRegisterInfo& registerInfo);
	void UnregisterClient(int socketId);
	void UpdateClientHeartbeat(int socketId, const TcpClientHeartbeat& heartbeat);

	// ��������
	std::string GenerateMessageId();
	long long GetCurrentTimestamp();
	void CleanupTimeoutClients();
	void SendHeartbeatAck(int socketId, const std::string& clientId);
	bool IsValidDeviceId(const std::string& deviceId) const;
	bool IsClientTypeAllowed(TcpClientType clientType) const;

private:
	// TCP������ʵ��
	std::unique_ptr<TcpServerWrapper> m_tcpServer;

	// �ͻ��˹���
	mutable std::mutex m_clientsMutex;
	std::unordered_map<int, TcpClientInfo> m_socketToClient;        // socketId -> TcpClientInfo
	std::unordered_map<std::string, int> m_deviceToSocket;          // deviceId -> socketId
	std::unordered_map<std::string, int> m_clientIdToSocket;        // clientId -> socketId

	// ���ò���
	std::atomic<int> m_heartbeatTimeout;    // ������ʱʱ��(��)
	std::atomic<int> m_maxClients;          // ���ͻ�����
	std::atomic<bool> m_autoCleanup;        // �Ƿ������Զ�����
	std::atomic<int> m_cleanupInterval;     // ������(��)

	// ͳ����Ϣ
	std::atomic<int> m_messageIdCounter;    // ��ϢID������
	std::atomic<long long> m_totalMessages; // ����Ϣ��

	// ��ʱ��
	QTimer* m_cleanupTimer;

	// �źŶ���
	TcpClientManagerSignals m_signals;
};



//----------------------------TcpClientManagerInstance----------------------------------------------
//----------------------------TcpClientManagerInstance----------------------------------------------
//----------------------------TcpClientManagerInstance----------------------------------------------

/**
 * @brief TCP�ͻ��˹�����ʵ���ࣨ����ģʽ��
 * @brief �ṩȫ�ַ��ʵ�TCP�ͻ��˹������
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

