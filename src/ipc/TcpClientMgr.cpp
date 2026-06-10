#include "TcpClientMgr.h"
#include "global.h"
#include <QTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <algorithm>

// 静态成员定义
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
	, m_heartbeatTimeout(30)      // 30秒心跳超时
	, m_maxClients(100)           // 最大100个客户端
	, m_autoCleanup(true)         // 启用自动清理
	, m_cleanupInterval(60)       // 60秒清理间隔
	, m_messageIdCounter(0)
	, m_totalMessages(0)
	, m_cleanupTimer(new QTimer(this))
{
	// 设置TCP服务器回调
	m_tcpServer->SetOnConnectionCallback([this](const TcpConnection& conn) 
	{
		OnTcpConnection(conn);
	});

	m_tcpServer->SetOnDisconnectionCallback([this](int socketId) 
	{
		OnTcpDisconnection(socketId);
	});

	//TcpMessage 数据类型重复
	m_tcpServer->SetOnMessageCallback([this](const OldTcpMessage& tcpMsg)
	{
		// 转换为项目中定义的TcpMessage结构
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

	// 设置自动清理定时器
	connect(m_cleanupTimer, &QTimer::timeout, this, [this]() 
{
		CleanupTimeoutClients();
	});

	LOG_INFO("TCP客户端管理器初始化完成");
}


TcpClientManager::~TcpClientManager()
{
	StopServer();
	LOG_INFO("TCP客户端管理器已销毁");
}












bool TcpClientManager::StartServer(const std::string& ip /*= "0.0.0.0"*/, int port /*= 9000*/)
{
	if (!m_tcpServer) 
	{
		LOG_INFO("TCP服务器实例无效");
		return false;
	}

	// 配置服务器参数
	m_tcpServer->SetServerConfig(m_maxClients, true, m_heartbeatTimeout);

	if (m_tcpServer->Start(ip, port)) 
	{
		LOG_INFO(QString("TCP客户端管理服务器启动成功: %1:%2").arg(ip.c_str()).arg(port));

		// 启动自动清理定时器
		if (m_autoCleanup) 
		{
			m_cleanupTimer->start(m_cleanupInterval * 1000);
			LOG_INFO(QString("自动清理定时器已启动，间隔: %1秒").arg(m_cleanupInterval.load()));
		}

		return true;
	}
	else 
	{
		LOG_INFO(QString("TCP客户端管理服务器启动失败: %1:%2").arg(ip.c_str()).arg(port));
		return false;
	}
}

void TcpClientManager::StopServer()
{
	if (m_tcpServer && m_tcpServer->IsRunning()) 
	{
		// 停止自动清理
		m_cleanupTimer->stop();

		// 清理所有客户端
		{
			std::lock_guard<std::mutex> lock(m_clientsMutex);
			m_socketToClient.clear();
			m_deviceToSocket.clear();
			m_clientIdToSocket.clear();
		}

		// 停止服务器
		m_tcpServer->Stop();
		LOG_INFO("TCP客户端管理服务器已停止");
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
		LOG_INFO(QString("设备ID未找到: %1").arg(deviceId.c_str()));
		return false;
	}

	int socketId = it->second;
	bool result = m_tcpServer->SendData(socketId, data);

	if (result) 
	{
		m_totalMessages++;
		LOG_INFO(QString("数据已发送到设备 %1 (Socket: %2), 大小: %3 bytes")
			.arg(deviceId.c_str()).arg(socketId).arg(data.size()));
	}
	else 
	{
		LOG_INFO(QString("数据发送失败到设备 %1 (Socket: %2)")
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

	// 根据控制类型选择消息类型
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
		LOG_INFO(QString("不支持的任务控制类型: %1").arg(taskControl.controlType.c_str()));
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

	LOG_INFO(QString("广播数据到 %1 个客户端, 大小: %2 bytes")
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

	return TcpClientInfo(); // 返回默认构造的无效客户端信息
}

TcpClientInfo TcpClientManager::GetClientBySocketId(int socketId) const
{
	std::lock_guard<std::mutex> lock(m_clientsMutex);

	auto it = m_socketToClient.find(socketId);
	if (it != m_socketToClient.end()) 
	{
		return it->second;
	}

	return TcpClientInfo(); // 返回默认构造的无效客户端信息
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
	LOG_INFO(QString("心跳超时时间设置为: %1秒").arg(timeoutSeconds));

}

void TcpClientManager::SetMaxClients(int maxClients)
{
	m_maxClients = maxClients;
	LOG_INFO(QString("最大客户端数设置为: %1").arg(maxClients));
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

	LOG_INFO(QString("自动清理%1, 间隔: %2秒")
		.arg(enable ? "已启用" : "已禁用").arg(cleanupInterval));
}

void TcpClientManager::OnTcpConnection(const TcpConnection& connection)
{
	LOG_INFO(QString("新TCP连接: ID=%1, IP=%2:%3")
		.arg(connection.connId)
		.arg(connection.clientIp.c_str())
		.arg(connection.clientPort));

	std::lock_guard<std::mutex> lock(m_clientsMutex);

	// 检查是否超过最大客户端数
	if (m_socketToClient.size() >= static_cast<size_t>(m_maxClients)) {
		LOG_INFO(QString("超过最大客户端数限制(%1)，拒绝连接 ID=%2")
			.arg(m_maxClients.load()).arg(connection.connId));

		m_tcpServer->DisconnectClient(connection.connId);
		return;
	}

	// 创建客户端信息
	TcpClientInfo clientInfo;
	clientInfo.socketId = connection.connId;
	clientInfo.connection = connection;
	clientInfo.lastHeartbeatTime = GetCurrentTimestamp();

	// 添加到映射表（此时还未注册，等待客户端发送注册消息）
	m_socketToClient[connection.connId] = clientInfo;

	emit m_signals.SigClientConnected(clientInfo);
}

void TcpClientManager::OnTcpDisconnection(int socketId)
{
	LOG_INFO(QString("TCP连接断开: Socket ID=%1").arg(socketId));

	UnregisterClient(socketId);
}

void TcpClientManager::OnTcpMessage(const TcpMessage& message)
{
	m_totalMessages++;

	LOG_INFO(QString("收到TCP消息: Socket=%1, 大小=%2 bytes")
		.arg(message.sourceId.c_str()).arg(message.payload.size()));

	try {
		// 解析消息类型
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
			LOG_INFO(QString("收到未知消息类型: %1").arg(static_cast<int>(parsedMessage.messageType)));
			break;
		}

	}
	catch (const std::exception& e) {
		LOG_INFO(QString("处理TCP消息异常: %1").arg(e.what()));
	}
}

void TcpClientManager::OnTcpError(int errorCode, const std::string& errorMessage)
{
	LOG_INFO(QString("TCP服务器错误 [%1]: %2").arg(errorCode).arg(errorMessage.c_str()));
	emit m_signals.SigManagerError(errorCode, errorMessage);
}

void TcpClientManager::HandleClientRegister(int socketId, const std::string& payload)
{
	try 
	{
		TcpClientRegisterInfo registerInfo = TcpClientRegisterInfo::fromJson(payload);

		if (RegisterClient(socketId, registerInfo)) 
		{
			LOG_INFO(QString("客户端注册成功: Socket=%1, Device=%2, Type=%3")
				.arg(socketId)
				.arg(registerInfo.deviceId.c_str())
				.arg(static_cast<int>(registerInfo.clientType)));
		}

	}
	catch (const std::exception& e) 
	{
		LOG_INFO(QString("客户端注册异常: %1").arg(e.what()));
	}
}

void TcpClientManager::HandleClientHeartbeat(int socketId, const std::string& payload)
{
	try {
		TcpClientHeartbeat heartbeat = TcpClientHeartbeat::fromJson(payload);
		UpdateClientHeartbeat(socketId, heartbeat);

		// 发送心跳应答
		SendHeartbeatAck(socketId, heartbeat.clientId);

		emit m_signals.SigClientHeartbeat(heartbeat);

	}
	catch (const std::exception& e) {
		LOG_INFO(QString("处理心跳消息异常: %1").arg(e.what()));
	}
}

void TcpClientManager::HandleClientDeviceStatus(int socketId, const std::string& payload)
{
	try {
		TcpClientDeviceStatus deviceStatus = TcpClientDeviceStatus::fromJson(payload);

		// 更新客户端状态
		{
			std::lock_guard<std::mutex> lock(m_clientsMutex);
			auto it = m_socketToClient.find(socketId);
			if (it != m_socketToClient.end()) {
				it->second.threadStatus = deviceStatus.threadStatus;
			}
		}

		LOG_INFO(QString("设备状态更新: Device=%1, Status=%2")
			.arg(deviceStatus.deviceId.c_str())
			.arg(static_cast<int>(deviceStatus.deviceStatus)));

		emit m_signals.SigClientStatusUpdate(deviceStatus);

	}
	catch (const std::exception& e) {
		LOG_INFO(QString("处理设备状态异常: %1").arg(e.what()));
	}

}

void TcpClientManager::HandleClientTaskStatus(int socketId, const std::string& payload)
{
	try {
		TcpClientTaskStatus taskStatus = TcpClientTaskStatus::fromJson(payload);

		LOG_INFO(QString("任务状态更新: Task=%1, Device=%2, Status=%3, Progress=%4%%")
			.arg(taskStatus.taskId.c_str())
			.arg(taskStatus.deviceId.c_str())
			.arg(static_cast<int>(taskStatus.taskStatus))
			.arg(taskStatus.progressPercent));

		emit m_signals.SigClientTaskStatusUpdate(taskStatus);

	}
	catch (const std::exception& e) {
		LOG_INFO(QString("处理任务状态异常: %1").arg(e.what()));
	}
}

bool TcpClientManager::RegisterClient(int socketId, const TcpClientRegisterInfo& registerInfo)
{
	std::lock_guard<std::mutex> lock(m_clientsMutex);

	// 验证设备ID有效性
	if (!IsValidDeviceId(registerInfo.deviceId)) {
		LOG_INFO(QString("无效的设备ID: %1").arg(registerInfo.deviceId.c_str()));
		return false;
	}

	// 验证客户端类型
	if (!IsClientTypeAllowed(registerInfo.clientType)) {
		LOG_INFO(QString("不允许的客户端类型: %1").arg(static_cast<int>(registerInfo.clientType)));
		return false;
	}

	// 检查设备ID是否已被其他客户端使用
	auto deviceIt = m_deviceToSocket.find(registerInfo.deviceId);
	if (deviceIt != m_deviceToSocket.end() && deviceIt->second != socketId) {
		LOG_INFO(QString("设备ID已被使用: %1").arg(registerInfo.deviceId.c_str()));
		return false;
	}

	// 更新客户端信息
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

		// 更新映射表
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

		// 从映射表中移除
		if (!clientInfo.deviceId.empty()) {
			m_deviceToSocket.erase(clientInfo.deviceId);
			emit m_signals.SigClientDisconnected(clientInfo.deviceId);
		}

		if (!clientInfo.clientId.empty()) {
			m_clientIdToSocket.erase(clientInfo.clientId);
		}

		// 移除客户端信息
		m_socketToClient.erase(it);

		LOG_INFO(QString("客户端已注销: Socket=%1, Device=%2")
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

			LOG_INFO(QString("清理超时客户端: Socket=%1, Device=%2, 超时=%3秒")
				.arg(it->first)
				.arg(clientInfo.deviceId.c_str())
				.arg((currentTime - clientInfo.lastHeartbeatTime) / 1000));

			// 断开连接
			m_tcpServer->DisconnectClient(it->first);

			// 从映射表移除
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
		LOG_INFO(QString("清理了 %1 个超时客户端，剩余客户端数: %2")
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
}

bool TcpClientManager::IsValidDeviceId(const std::string& deviceId) const
{
	return !deviceId.empty() && deviceId.length() >= 3 && deviceId.length() <= 50;
}

bool TcpClientManager::IsClientTypeAllowed(TcpClientType clientType) const
{
	return clientType != TcpClientType::UNKNOWN;

}

// 单例实现
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

