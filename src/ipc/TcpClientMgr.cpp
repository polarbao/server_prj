#include "TcpClientMgr.h"
#include "global.h"
#include <QTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <algorithm>

// ��̬��Ա����
std::unique_ptr<TcpClientManager> TcpClientManagerInstance::s_instance = nullptr;
std::mutex TcpClientManagerInstance::s_instanceMutex;

//----------------------------TcpClientManagerSignals----------------------------------------------
//----------------------------TcpClientManagerSignals----------------------------------------------
//----------------------------TcpClientManagerSignals----------------------------------------------






//----------------------------TcpClientManager----------------------------------------------
//----------------------------TcpClientManager----------------------------------------------
//----------------------------TcpClientManager----------------------------------------------







TcpClientManager::TcpClientManager(QObject* parent /*= nullptr*/)
	: QObject(parent)
	, m_tcpServer(std::make_unique<TcpServerWrapper>())
	, m_heartbeatTimeout(30)      // 30��������ʱ
	, m_maxClients(100)           // ���100���ͻ���
	, m_autoCleanup(true)         // �����Զ�����
	, m_cleanupInterval(60)       // 60��������
	, m_messageIdCounter(0)
	, m_totalMessages(0)
	, m_cleanupTimer(new QTimer(this))
{
	// ����TCP�������ص�
	m_tcpServer->SetOnConnectionCallback([this](const TcpConnection& conn) 
	{
		OnTcpConnection(conn);
	});

	m_tcpServer->SetOnDisconnectionCallback([this](int socketId) 
	{
		OnTcpDisconnection(socketId);
	});

	//TcpMessage ���������ظ�
	m_tcpServer->SetOnMessageCallback([this](const OldTcpMessage& tcpMsg)
	{
		// ת��Ϊ��Ŀ�ж����TcpMessage�ṹ
		TcpMessage message;
		message.messageId = std::to_string(tcpMsg.connId);
		message.sourceId = std::to_string(tcpMsg.connId);
		message.payload = tcpMsg.data;
		message.timestamp = GetCurrentTimestamp();

		OnTcpMessage(message);
	});

	m_tcpServer->SetOnErrorCallback([this](int errorCode, const std::string& errorMsg) 
	{
		OnTcpError(errorCode, errorMsg);
	});

	// �����Զ������ʱ��
	connect(m_cleanupTimer, &QTimer::timeout, this, [this]() 
{
		CleanupTimeoutClients();
	});

	LOG_INFO("TCP�ͻ��˹�������ʼ�����");
}


TcpClientManager::~TcpClientManager()
{
	StopServer();
	LOG_INFO("TCP�ͻ��˹�����������");
}












bool TcpClientManager::StartServer(const std::string& ip /*= "0.0.0.0"*/, int port /*= 9000*/)
{
	if (!m_tcpServer) 
	{
		LOG_INFO("TCP������ʵ����Ч");
		return false;
	}

	// ���÷���������
	m_tcpServer->SetServerConfig(m_maxClients, true, m_heartbeatTimeout);

	if (m_tcpServer->Start(ip, port)) 
	{
		LOG_INFO(QString("TCP�ͻ��˹������������ɹ�: %1:%2").arg(ip.c_str()).arg(port));

		// ����Զ������ʱ��
		if (m_autoCleanup) 
		{
			m_cleanupTimer->start(m_cleanupInterval * 1000);
			LOG_INFO(QString("�Զ������ʱ������������: %1��").arg(m_cleanupInterval.load()));
		}

		return true;
	}
	else 
	{
		LOG_INFO(QString("TCP�ͻ��˹�����������ʧ��: %1:%2").arg(ip.c_str()).arg(port));
		return false;
	}
}

void TcpClientManager::StopServer()
{
	if (m_tcpServer && m_tcpServer->IsRunning()) 
	{
		// ֹͣ�Զ�����
		m_cleanupTimer->stop();

		// �������пͻ���
		{
			std::lock_guard<std::mutex> lock(m_clientsMutex);
			m_socketToClient.clear();
			m_deviceToSocket.clear();
			m_clientIdToSocket.clear();
		}

		// ֹͣ������
		m_tcpServer->Stop();
		LOG_INFO("TCP�ͻ��˹����������ֹͣ");
	}
}

bool TcpClientManager::IsServerRunning() const
{
	return m_tcpServer && m_tcpServer->IsRunning();
}

int TcpClientManager::GetServerPort() const
{
	return m_tcpServer ? m_tcpServer->GetPort() : 0;
}

bool TcpClientManager::SendDataToDevice(const std::string& deviceId, const std::string& data)
{
	std::lock_guard<std::mutex> lock(m_clientsMutex);

	auto it = m_deviceToSocket.find(deviceId);
	if (it == m_deviceToSocket.end()) 
	{
		LOG_INFO(QString("�豸IDδ�ҵ�: %1").arg(deviceId.c_str()));
		return false;
	}

	int socketId = it->second;
	bool result = m_tcpServer->SendData(socketId, data);

	if (result) 
	{
		m_totalMessages++;
		LOG_INFO(QString("�����ѷ��͵��豸 %1 (Socket: %2), ��С: %3 bytes")
			.arg(deviceId.c_str()).arg(socketId).arg(data.size()));
	}
	else 
	{
		LOG_INFO(QString("���ݷ���ʧ�ܵ��豸 %1 (Socket: %2)")
			.arg(deviceId.c_str()).arg(socketId));
	}

	return result;
}

bool TcpClientManager::SendTaskDispatch(const std::string& deviceId, const TcpServerTaskDispatch& taskDispatch)
{
	TcpMessage message;
	message.messageType = MessageType::TCP_SERVER_TASK_DISPATCH;
	message.messageId = GenerateMessageId();
	message.sourceId = "SERVER";
	message.targetId = deviceId;
	message.payload = taskDispatch.toJson();
	message.timestamp = GetCurrentTimestamp();
	message.version = "1.0";

	std::string jsonData = message.toJson();
	return SendDataToDevice(deviceId, jsonData);
}

bool TcpClientManager::SendTaskControl(const std::string& deviceId, const TcpServerTaskControl& taskControl)
{
	TcpMessage message;

	// ���ݿ�������ѡ����Ϣ����
	if (taskControl.controlType == "START") 
	{
		message.messageType = MessageType::TCP_SERVER_TASK_START;
	}
	else if (taskControl.controlType == "CANCEL") 
	{
		message.messageType = MessageType::TCP_SERVER_TASK_CANCEL;
	}
	else 
	{
		LOG_INFO(QString("��֧�ֵ������������: %1").arg(taskControl.controlType.c_str()));
		return false;
	}

	message.messageId = GenerateMessageId();
	message.sourceId = "SERVER";
	message.targetId = deviceId;
	message.payload = taskControl.toJson();
	message.timestamp = GetCurrentTimestamp();
	message.version = "1.0";

	std::string jsonData = message.toJson();
	return SendDataToDevice(deviceId, jsonData);
}

bool TcpClientManager::BroadcastData(const std::string& data)
{
	if (!m_tcpServer) 
	{
		return false;
	}

	int count = m_tcpServer->BroadcastData(data);
	m_totalMessages += count;

	LOG_INFO(QString("�㲥���ݵ� %1 ���ͻ���, ��С: %2 bytes")
		.arg(count).arg(data.size()));

	return count > 0;
}

bool TcpClientManager::DisconnectClient(const std::string& deviceId)
{
	std::lock_guard<std::mutex> lock(m_clientsMutex);

	auto it = m_deviceToSocket.find(deviceId);
	if (it == m_deviceToSocket.end()) 
	{
		return false;
	}

	int socketId = it->second;
	return m_tcpServer->DisconnectClient(socketId);
}

int TcpClientManager::GetConnectedClientCount() const
{
	std::lock_guard<std::mutex> lock(m_clientsMutex);
	return static_cast<int>(m_socketToClient.size());
}

std::vector<TcpClientInfo> TcpClientManager::GetAllClients() const
{
	std::lock_guard<std::mutex> lock(m_clientsMutex);

	std::vector<TcpClientInfo> clients;
	clients.reserve(m_socketToClient.size());

	for (const auto& pair : m_socketToClient) {
		clients.push_back(pair.second);
	}

	return clients;
}

TcpClientInfo TcpClientManager::GetClientByDeviceId(const std::string& deviceId) const
{
	std::lock_guard<std::mutex> lock(m_clientsMutex);

	auto it = m_deviceToSocket.find(deviceId);
	if (it != m_deviceToSocket.end()) 
	{
		auto clientIt = m_socketToClient.find(it->second);
		if (clientIt != m_socketToClient.end()) 
		{
			return clientIt->second;
		}
	}

	return TcpClientInfo(); // ����Ĭ�Ϲ������Ч�ͻ�����Ϣ
}

TcpClientInfo TcpClientManager::GetClientBySocketId(int socketId) const
{
	std::lock_guard<std::mutex> lock(m_clientsMutex);

	auto it = m_socketToClient.find(socketId);
	if (it != m_socketToClient.end()) 
	{
		return it->second;
	}

	return TcpClientInfo(); // ����Ĭ�Ϲ������Ч�ͻ�����Ϣ
}

std::vector<TcpClientInfo> TcpClientManager::GetClientsByType(TcpClientType clientType) const
{
	std::lock_guard<std::mutex> lock(m_clientsMutex);

	std::vector<TcpClientInfo> clients;

	for (const auto& pair : m_socketToClient) 
	{
		if (pair.second.clientType == clientType) 
		{
			clients.push_back(pair.second);
		}
	}

	return clients;
}

void TcpClientManager::SetHeartbeatTimeout(int timeoutSeconds)
{
	m_heartbeatTimeout = timeoutSeconds;
	LOG_INFO(QString("������ʱʱ������Ϊ: %1��").arg(timeoutSeconds));

}

void TcpClientManager::SetMaxClients(int maxClients)
{
	m_maxClients = maxClients;
	LOG_INFO(QString("���ͻ���������Ϊ: %1").arg(maxClients));
}

void TcpClientManager::EnableAutoCleanup(bool enable, int cleanupInterval /*= 60*/)
{
	m_autoCleanup = enable;
	m_cleanupInterval = cleanupInterval;

	if (enable && IsServerRunning()) 
	{
		m_cleanupTimer->start(cleanupInterval * 1000);
	}
	else 
	{
		m_cleanupTimer->stop();
	}

	LOG_INFO(QString("�Զ�����%1, ���: %2��")
		.arg(enable ? "������" : "�ѽ���").arg(cleanupInterval));
}

void TcpClientManager::OnTcpConnection(const TcpConnection& connection)
{
	LOG_INFO(QString("��TCP����: ID=%1, IP=%2:%3")
		.arg(connection.connId)
		.arg(connection.clientIp.c_str())
		.arg(connection.clientPort));

	std::lock_guard<std::mutex> lock(m_clientsMutex);

	// ����Ƿ񳬹����ͻ�����
	if (m_socketToClient.size() >= static_cast<size_t>(m_maxClients)) {
		LOG_INFO(QString("�������ͻ���������(%1)���ܾ����� ID=%2")
			.arg(m_maxClients.load()).arg(connection.connId));

		m_tcpServer->DisconnectClient(connection.connId);
		return;
	}

	// �����ͻ�����Ϣ
	TcpClientInfo clientInfo;
	clientInfo.socketId = connection.connId;
	clientInfo.connection = connection;
	clientInfo.lastHeartbeatTime = GetCurrentTimestamp();

	// ��ӵ�ӳ������ʱ��δע�ᣬ�ȴ��ͻ��˷���ע����Ϣ��
	m_socketToClient[connection.connId] = clientInfo;

	emit m_signals.SigClientConnected(clientInfo);
}

void TcpClientManager::OnTcpDisconnection(int socketId)
{
	LOG_INFO(QString("TCP���ӶϿ�: Socket ID=%1").arg(socketId));

	UnregisterClient(socketId);
}

void TcpClientManager::OnTcpMessage(const TcpMessage& message)
{
	m_totalMessages++;

	LOG_INFO(QString("�յ�TCP��Ϣ: Socket=%1, ��С=%2 bytes")
		.arg(message.sourceId.c_str()).arg(message.payload.size()));

	try {
		// ������Ϣ����
		TcpMessage parsedMessage = TcpMessage::fromJson(message.payload);
		int socketId = std::stoi(message.sourceId);

		switch (parsedMessage.messageType) {
		case MessageType::TCP_CLIENT_REGISTER:
			HandleClientRegister(socketId, parsedMessage.payload);
			break;

		case MessageType::TCP_CLIENT_HEARTBEAT:
			HandleClientHeartbeat(socketId, parsedMessage.payload);
			break;

		case MessageType::TCP_CLIENT_DEVICE_STATUS:
			HandleClientDeviceStatus(socketId, parsedMessage.payload);
			break;

		case MessageType::TCP_CLIENT_TASK_STATUS:
			HandleClientTaskStatus(socketId, parsedMessage.payload);
			break;

		default:
			LOG_INFO(QString("�յ�δ֪��Ϣ����: %1").arg(static_cast<int>(parsedMessage.messageType)));
			break;
		}

	}
	catch (const std::exception& e) {
		LOG_INFO(QString("����TCP��Ϣ�쳣: %1").arg(e.what()));
	}
}

void TcpClientManager::OnTcpError(int errorCode, const std::string& errorMessage)
{
	LOG_INFO(QString("TCP���������� [%1]: %2").arg(errorCode).arg(errorMessage.c_str()));
	emit m_signals.SigManagerError(errorCode, errorMessage);
}

void TcpClientManager::HandleClientRegister(int socketId, const std::string& payload)
{
	try 
	{
		TcpClientRegisterInfo registerInfo = TcpClientRegisterInfo::fromJson(payload);

		if (RegisterClient(socketId, registerInfo)) 
		{
			LOG_INFO(QString("�ͻ���ע��ɹ�: Socket=%1, Device=%2, Type=%3")
				.arg(socketId)
				.arg(registerInfo.deviceId.c_str())
				.arg(static_cast<int>(registerInfo.clientType)));
		}

	}
	catch (const std::exception& e) 
	{
		LOG_INFO(QString("�ͻ���ע���쳣: %1").arg(e.what()));
	}
}

void TcpClientManager::HandleClientHeartbeat(int socketId, const std::string& payload)
{
	try {
		TcpClientHeartbeat heartbeat = TcpClientHeartbeat::fromJson(payload);
		UpdateClientHeartbeat(socketId, heartbeat);

		// ��������Ӧ��
		SendHeartbeatAck(socketId, heartbeat.clientId);

		emit m_signals.SigClientHeartbeat(heartbeat);

	}
	catch (const std::exception& e) {
		LOG_INFO(QString("����������Ϣ�쳣: %1").arg(e.what()));
	}
}

void TcpClientManager::HandleClientDeviceStatus(int socketId, const std::string& payload)
{
	try {
		TcpClientDeviceStatus deviceStatus = TcpClientDeviceStatus::fromJson(payload);

		// ���¿ͻ���״̬
		{
			std::lock_guard<std::mutex> lock(m_clientsMutex);
			auto it = m_socketToClient.find(socketId);
			if (it != m_socketToClient.end()) {
				it->second.threadStatus = deviceStatus.threadStatus;
			}
		}

		LOG_INFO(QString("�豸״̬����: Device=%1, Status=%2")
			.arg(deviceStatus.deviceId.c_str())
			.arg(static_cast<int>(deviceStatus.deviceStatus)));

		emit m_signals.SigClientStatusUpdate(deviceStatus);

	}
	catch (const std::exception& e) {
		LOG_INFO(QString("�����豸״̬�쳣: %1").arg(e.what()));
	}

}

void TcpClientManager::HandleClientTaskStatus(int socketId, const std::string& payload)
{
	try {
		TcpClientTaskStatus taskStatus = TcpClientTaskStatus::fromJson(payload);

		LOG_INFO(QString("����״̬����: Task=%1, Device=%2, Status=%3, Progress=%4%%")
			.arg(taskStatus.taskId.c_str())
			.arg(taskStatus.deviceId.c_str())
			.arg(static_cast<int>(taskStatus.taskStatus))
			.arg(taskStatus.progressPercent));

		emit m_signals.SigClientTaskStatusUpdate(taskStatus);

	}
	catch (const std::exception& e) {
		LOG_INFO(QString("��������״̬�쳣: %1").arg(e.what()));
	}
}

bool TcpClientManager::RegisterClient(int socketId, const TcpClientRegisterInfo& registerInfo)
{
	std::lock_guard<std::mutex> lock(m_clientsMutex);

	// ��֤�豸ID��Ч��
	if (!IsValidDeviceId(registerInfo.deviceId)) {
		LOG_INFO(QString("��Ч���豸ID: %1").arg(registerInfo.deviceId.c_str()));
		return false;
	}

	// ��֤�ͻ�������
	if (!IsClientTypeAllowed(registerInfo.clientType)) {
		LOG_INFO(QString("������Ŀͻ�������: %1").arg(static_cast<int>(registerInfo.clientType)));
		return false;
	}

	// ����豸ID�Ƿ��ѱ������ͻ���ʹ��
	auto deviceIt = m_deviceToSocket.find(registerInfo.deviceId);
	if (deviceIt != m_deviceToSocket.end() && deviceIt->second != socketId) {
		LOG_INFO(QString("�豸ID�ѱ�ʹ��: %1").arg(registerInfo.deviceId.c_str()));
		return false;
	}

	// ���¿ͻ�����Ϣ
	auto clientIt = m_socketToClient.find(socketId);
	if (clientIt != m_socketToClient.end()) {
		TcpClientInfo& clientInfo = clientIt->second;
		clientInfo.clientId = registerInfo.clientId;
		clientInfo.deviceId = registerInfo.deviceId;
		clientInfo.clientType = registerInfo.clientType;
		clientInfo.clientVersion = registerInfo.clientVersion;
		clientInfo.threadStatus = registerInfo.threadStatus;
		clientInfo.clientName = registerInfo.clientName;
		clientInfo.isRegistered = true;
		clientInfo.registerTime = GetCurrentTimestamp();

		// ����ӳ���
		m_deviceToSocket[registerInfo.deviceId] = socketId;
		m_clientIdToSocket[registerInfo.clientId] = socketId;

		emit m_signals.SigClientRegistered(clientInfo);
		return true;
	}

	return false;
}

void TcpClientManager::UnregisterClient(int socketId)
{
	std::lock_guard<std::mutex> lock(m_clientsMutex);

	auto it = m_socketToClient.find(socketId);
	if (it != m_socketToClient.end()) {
		const TcpClientInfo& clientInfo = it->second;

		// ��ӳ������Ƴ�
		if (!clientInfo.deviceId.empty()) {
			m_deviceToSocket.erase(clientInfo.deviceId);
			emit m_signals.SigClientDisconnected(clientInfo.deviceId);
		}

		if (!clientInfo.clientId.empty()) {
			m_clientIdToSocket.erase(clientInfo.clientId);
		}

		// �Ƴ��ͻ�����Ϣ
		m_socketToClient.erase(it);

		LOG_INFO(QString("�ͻ�����ע��: Socket=%1, Device=%2")
			.arg(socketId).arg(clientInfo.deviceId.c_str()));
	}
}

void TcpClientManager::UpdateClientHeartbeat(int socketId, const TcpClientHeartbeat& heartbeat)
{
	std::lock_guard<std::mutex> lock(m_clientsMutex);

	auto it = m_socketToClient.find(socketId);
	if (it != m_socketToClient.end()) {
		it->second.lastHeartbeatTime = GetCurrentTimestamp();
		it->second.threadStatus = heartbeat.threadStatus;
	}
}

std::string TcpClientManager::GenerateMessageId()
{
	return "MSG_" + std::to_string(++m_messageIdCounter) + "_" + std::to_string(GetCurrentTimestamp());

}

long long TcpClientManager::GetCurrentTimestamp()
{
	return std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::system_clock::now().time_since_epoch()).count();
}

void TcpClientManager::CleanupTimeoutClients()
{
	std::lock_guard<std::mutex> lock(m_clientsMutex);

	long long currentTime = GetCurrentTimestamp();
	long long timeoutMs = m_heartbeatTimeout * 1000;
	int cleanupCount = 0;

	auto it = m_socketToClient.begin();
	while (it != m_socketToClient.end()) {
		const TcpClientInfo& clientInfo = it->second;

		if (clientInfo.isRegistered &&
			(currentTime - clientInfo.lastHeartbeatTime) > timeoutMs) {

			LOG_INFO(QString("�����ʱ�ͻ���: Socket=%1, Device=%2, ��ʱ=%3��")
				.arg(it->first)
				.arg(clientInfo.deviceId.c_str())
				.arg((currentTime - clientInfo.lastHeartbeatTime) / 1000));

			// �Ͽ�����
			m_tcpServer->DisconnectClient(it->first);

			// ��ӳ����Ƴ�
			if (!clientInfo.deviceId.empty()) {
				m_deviceToSocket.erase(clientInfo.deviceId);
			}
			if (!clientInfo.clientId.empty()) {
				m_clientIdToSocket.erase(clientInfo.clientId);
			}

			it = m_socketToClient.erase(it);
			cleanupCount++;
		}
		else {
			++it;
		}
	}

	if (cleanupCount > 0) {
		LOG_INFO(QString("������ %1 ����ʱ�ͻ��ˣ�ʣ��ͻ�����: %2")
			.arg(cleanupCount).arg(m_socketToClient.size()));
	}
}

void TcpClientManager::SendHeartbeatAck(int socketId, const std::string& clientId)
{
	TcpMessage message;
	message.messageType = MessageType::TCP_SERVER_HEARTBEAT_ACK;
	message.messageId = GenerateMessageId();
	message.sourceId = "SERVER";
	message.targetId = clientId;
	message.payload = R"({"status":"ok","timestamp":)" + std::to_string(GetCurrentTimestamp()) + "}";
	message.timestamp = GetCurrentTimestamp();
	message.version = "1.0";

	std::string jsonData = message.toJson();
	m_tcpServer->SendData(socketId, jsonData);
}

bool TcpClientManager::IsValidDeviceId(const std::string& deviceId) const
{
	return !deviceId.empty() && deviceId.length() >= 3 && deviceId.length() <= 50;
}

bool TcpClientManager::IsClientTypeAllowed(TcpClientType clientType) const
{
	return clientType != TcpClientType::UNKNOWN;

}

// ����ʵ��
//----------------------------TcpClientManagerInstance----------------------------------------------
//----------------------------TcpClientManagerInstance----------------------------------------------
//----------------------------TcpClientManagerInstance----------------------------------------------


TcpClientManager& TcpClientManagerInstance::GetInstance()
{
	std::lock_guard<std::mutex> lock(s_instanceMutex);

	if (!s_instance) {
		s_instance = std::make_unique<TcpClientManager>();
	}

	return *s_instance;
}

void TcpClientManagerInstance::DestroyInstance()
{
	std::lock_guard<std::mutex> lock(s_instanceMutex);
	s_instance.reset();
}

