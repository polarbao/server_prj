#include "CameraTcpMgr.h"
#include "global.h"

#include <algorithm>



// 静态成员定义
std::unique_ptr<CameraTcpManager> CameraTcpManager::s_instance = nullptr;
std::mutex CameraTcpManager::s_instanceMutex;


CameraTcpManager::CameraTcpManager(QObject* parent)
	: QObject(parent)
	, m_tcpServer(std::make_unique<TcpServerWrapper>())
	, m_cacheEnabled(true)
	, m_heartbeatTimeout(600)          // 60秒心跳超时
	, m_maxRetryCount(3)              // 最大重试3次
	, m_cleanupTimer(std::make_unique<StdTimer>())
	, m_cacheRetryTimer(std::make_unique<StdTimer>())
{
	//qRegisterMetaType<TcpConnection>("TcpConnection");
	//qRegisterMetaType<CameraTcpClientInfo>("CameraTcpClientInfo");
	//qRegisterMetaType<CachedMessage>("CachedMessage");
	qRegisterMetaType<CameraScanData>("CameraScanData");

	

	// 设置TCP服务器回调
	m_tcpServer->SetOnConnectionCallback([this](const TcpConnection& conn) 
	{
		OnTcpConnection(conn);
	});

	m_tcpServer->SetOnDisconnectionCallback([this](int socketId) 
	{
		OnTcpDisconnection(socketId);
	});

	m_tcpServer->SetOnMessageCallback([this](const OldTcpMessage& tcpMsg)
	{
		// 转换为项目中定义的TcpMessage结构
		OnTcpMessage(tcpMsg);
	});

	m_tcpServer->SetOnErrorCallback([this](int errorCode, const std::string& errorMsg) 
	{
		OnTcpError(errorCode, errorMsg);
	});

	// 设置清理定时器 - 每60秒检查一次
	m_cleanupTimer->setCallback([this]() { OnCleanupTimer(); });
	m_cleanupTimer->start(60000);

	// 设置缓存重试定时器 - 每10秒重试一次
	m_cacheRetryTimer->setCallback([this]() { OnCacheRetryTimer(); });
	m_cacheRetryTimer->start(10000);

	LOG_INFO(u8"相机软件TCP客户端管理器初始化完成");
}


CameraTcpManager::~CameraTcpManager()
{
	StopServer();
	LOG_INFO(u8"相机软件TCP客户端管理器已销毁");
}

CameraTcpManager& CameraTcpManager::GetInstance()
{
	std::lock_guard<std::mutex> lock(s_instanceMutex);

	if (!s_instance)
	{
		s_instance = std::unique_ptr<CameraTcpManager>(new CameraTcpManager());
	}

	return *s_instance;
}

void CameraTcpManager::DestroyInstance()
{
	std::lock_guard<std::mutex> lock(s_instanceMutex);
	s_instance.reset();
}

bool CameraTcpManager::StartServer(int port /*= 6001*/)
{
	if (!m_tcpServer) 
	{
		LOG_INFO(u8"TCP服务器实例无效");
		return false;
	}

	// 配置服务器参数
	m_tcpServer->SetServerConfig(100, true, m_heartbeatTimeout); // 最大100个相机连接

	if (m_tcpServer->Start("127.0.0.1", port)) 
	{
		LOG_INFO(QString(u8"相机软件TCP服务器启动成功，端口: %1").arg(port));
		return true;
	}
	else 
	{
		LOG_INFO(QString(u8"相机软件TCP服务器启动失败，端口: %1").arg(port));
		return false;
	}
}

void CameraTcpManager::StopServer()
{
	if (m_tcpServer && m_tcpServer->IsRunning()) 
	{
		// 停止定时器
		m_cleanupTimer->stop();
		m_cacheRetryTimer->stop();

		// 清理所有客户端
		{
			std::lock_guard<std::mutex> lock(m_camerasMutex);
			m_socketToCamera.clear();
			m_deviceToSocket.clear();
		}

		// 清理缓存
		{
			std::lock_guard<std::mutex> lock(m_cacheMutex);
			std::queue<CachedMessage> empty;
			m_messageCache.swap(empty);
		}

		// 停止服务器
		m_tcpServer->Stop();
		LOG_INFO(u8"相机软件TCP服务器已停止");
	}
}

bool CameraTcpManager::IsServerRunning() const
{
	return m_tcpServer && m_tcpServer->IsRunning();

}

int CameraTcpManager::GetServerPort() const
{
	return m_tcpServer ? m_tcpServer->GetPort() : 0;

}

bool CameraTcpManager::SendMessageToCamera(const std::string& deviceId, const CameraJsonMessage& message)
{
	std::lock_guard<std::mutex> lock(m_camerasMutex);

	auto it = m_deviceToSocket.find(deviceId);
	if (it == m_deviceToSocket.end()) 
	{
		LOG_INFO(QString(u8"相机设备未连接: %1").arg(deviceId.c_str()));

		// 如果启用缓存，将消息加入缓存队列
		if (m_cacheEnabled) 
		{
			CacheMessage(deviceId, message);
		}
		return false;
	}

	int socketId = it->second;
	std::string jsonData = message.toJson();
	bool result = m_tcpServer->SendData(socketId, jsonData);

	if (result) 
	{
		LOG_INFO(QString(u8"消息已发送到相机设备 %1 (Socket: %2), 类型: %3")
			.arg(deviceId.c_str()).arg(socketId).arg(message.msg_type.c_str()));
	}
	else 
	{
		LOG_INFO(QString(u8"消息发送失败到相机设备 %1 (Socket: %2)")
			.arg(deviceId.c_str()).arg(socketId));

		// 发送失败时缓存消息
		if (m_cacheEnabled) 
		{
			CacheMessage(deviceId, message);
		}
	}

	return result;
}

bool CameraTcpManager::SendTaskStartToCamera(const std::string& deviceId)
{
	CameraJsonMessage message = CameraJsonMessage::createTaskStartRequest(deviceId);
	return SendMessageToCamera(deviceId, message);
}

bool CameraTcpManager::SendTaskCancelToCamera(const std::string& deviceId, const std::string& taskId)
{
	LOG_INFO(QString(u8"发送任务取消命令到相机设备: Device=%1, TaskId=%2")
		.arg(deviceId.c_str()).arg(taskId.c_str()));

	CameraJsonMessage cancelMessage = CameraJsonMessage::createTaskCancelRequest(deviceId, taskId);
	return SendMessageToCamera(deviceId, cancelMessage);
}

bool CameraTcpManager::SendDeviceStatusResponse(const std::string& deviceId, bool success /*= true*/, const std::string& msg /*= "success"*/)
{
	CameraJsonMessage response("device_status", deviceId);
	if (success) 
	{
		response.setSuccess(msg);
	}
	else 
	{
		response.setError(msg);
	}

	return SendMessageToCamera(deviceId, response);
}

bool CameraTcpManager::BroadcastMessage(const CameraJsonMessage& message)
{
	if (!m_tcpServer) 
	{
		return false;
	}

	std::string jsonData = message.toJson();
	int count = m_tcpServer->BroadcastData(jsonData);

	LOG_INFO(QString(u8"广播消息到 %1 个相机客户端, 类型: %2")
		.arg(count).arg(message.msg_type.c_str()));

	return count > 0;
}

bool CameraTcpManager::DisconnectCamera(const std::string& deviceId)
{
	std::lock_guard<std::mutex> lock(m_camerasMutex);

	auto it = m_deviceToSocket.find(deviceId);
	if (it == m_deviceToSocket.end()) 
	{
		return false;
	}

	int socketId = it->second;
	return m_tcpServer->DisconnectClient(socketId);
}

int CameraTcpManager::GetConnectedCameraCount() const
{
	std::lock_guard<std::mutex> lock(m_camerasMutex);
	return static_cast<int>(m_socketToCamera.size());
}

std::vector<CameraTcpClientInfo> CameraTcpManager::GetAllCameras() const
{
	std::lock_guard<std::mutex> lock(m_camerasMutex);

	std::vector<CameraTcpClientInfo> cameras;
	cameras.reserve(m_socketToCamera.size());

	for (const auto& pair : m_socketToCamera) 
	{
		cameras.push_back(pair.second);
	}

	return cameras;
}

CameraTcpClientInfo CameraTcpManager::GetCameraByDeviceId(const std::string& deviceId) const
{
	std::lock_guard<std::mutex> lock(m_camerasMutex);

	auto it = m_deviceToSocket.find(deviceId);
	if (it != m_deviceToSocket.end()) {
		auto clientIt = m_socketToCamera.find(it->second);
		if (clientIt != m_socketToCamera.end()) {
			return clientIt->second;
		}
	}

	return CameraTcpClientInfo(); // 返回默认构造的无效客户端信息
}

std::vector<std::string> CameraTcpManager::GetOnlineCameraDeviceIds() const
{
	std::lock_guard<std::mutex> lock(m_camerasMutex);

	std::vector<std::string> deviceIds;
	for (const auto& pair : m_socketToCamera) {
		if (pair.second.deviceStatus == "online") {
			deviceIds.push_back(pair.second.deviceId);
		}
	}

	return deviceIds;
}

bool CameraTcpManager::IsCameraOnline(const std::string& deviceId) const
{
	CameraTcpClientInfo camera = GetCameraByDeviceId(deviceId);
	return camera.isConnected && camera.deviceStatus == "online";
}

void CameraTcpManager::SetHeartbeatTimeout(int timeoutSeconds)
{
	m_heartbeatTimeout = timeoutSeconds;
	LOG_INFO(QString(u8"相机心跳超时时间设置为: %1秒").arg(timeoutSeconds));
}

void CameraTcpManager::SetMaxRetryCount(int maxRetry)
{
	m_maxRetryCount = maxRetry;
	LOG_INFO(QString(u8"最大重试次数设置为: %1").arg(maxRetry));
}


void CameraTcpManager::EnableMessageCache(bool enable)
{
	m_cacheEnabled = enable;
	LOG_INFO(QString(u8"消息缓存%1").arg(enable ? u8"已启用" : u8"已禁用"));
}

void CameraTcpManager::OnTcpConnection(const TcpConnection& connection)
{
	LOG_INFO(QString(u8"新相机TCP连接: ID=%1, IP=%2:%3")
		.arg(connection.connId)
		.arg(connection.clientIp.c_str())
		.arg(connection.clientPort));

	if (connection.clientIp.empty())
	{
		return;
	}

	std::lock_guard<std::mutex> lock(m_camerasMutex);

	// 创建客户端信息
	CameraTcpClientInfo cameraInfo;
	cameraInfo.socketId = connection.connId;
	cameraInfo.connection = connection;
	cameraInfo.isConnected = true;
	cameraInfo.connectTime = GetCurrentTimestamp();
	cameraInfo.lastHeartbeatTime = GetCurrentTimestamp();

	// 添加到映射表（等待设备状态同步消息来获取设备ID）
	m_socketToCamera[connection.connId] = cameraInfo;
}
void CameraTcpManager::OnTcpDisconnection(int socketId)
{
	LOG_INFO(QString(u8"相机TCP连接断开: Socket ID=%1").arg(socketId));

	UnregisterCamera(socketId);
}

void CameraTcpManager::OnTcpMessage(const OldTcpMessage& message)
{
	int socketId = message.connId;
	std::string messageData = message.data;

	LOG_INFO(QString(u8"收到相机TCP消息: Socket=%1, 大小=%2 bytes")
		.arg(socketId).arg(messageData.size()));

	HandleCameraMessage(socketId, messageData);
}

void CameraTcpManager::OnTcpError(int errorCode, const std::string& errorMessage)
{
	LOG_INFO(QString(u8"相机TCP服务器错误 [%1]: %2").arg(errorCode).arg(errorMessage.c_str()));
	// 发送Qt信号（保持兼容性）
	emit m_signals.SigManagerError(errorCode, errorMessage);

	// ★ 调用回调函数
	NotifyCameraManagerError(errorCode, errorMessage);
}

void CameraTcpManager::OnCleanupTimer()
{
	// 清理超时的相机连接
	std::lock_guard<std::mutex> lock(m_camerasMutex);

	long long currentTime = GetCurrentTimestamp();
	long long timeoutMs = m_heartbeatTimeout * 1000;
	int cleanupCount = 0;

	auto it = m_socketToCamera.begin();
	while (it != m_socketToCamera.end()) 
	{
		const CameraTcpClientInfo& cameraInfo = it->second;

		if ((currentTime - cameraInfo.lastHeartbeatTime) > timeoutMs) 
		{
			//TODO: 1121_ban 清理超时相机客户端逻辑
			//LOG_INFO(QString(u8"清理超时相机客户端: Socket=%1, Device=%2, 超时=%3秒")
			//	.arg(it->first)
			//	.arg(cameraInfo.deviceId.c_str())
			//	.arg((currentTime - cameraInfo.lastHeartbeatTime) / 1000));

			//// 断开连接
			//m_tcpServer->DisconnectClient(it->first);

			//// 从映射表移除
			//if (!cameraInfo.deviceId.empty()) 
			//{
			//	m_deviceToSocket.erase(cameraInfo.deviceId);
			//	// 发送Qt信号（保持兼容性）
			//	emit m_signals.SigCameraDisconnected(cameraInfo.deviceId);
			//	// ★ 调用回调函数
			//	NotifyCameraDisconnected(cameraInfo.deviceId);
			//}

			//it = m_socketToCamera.erase(it);
			//cleanupCount++;
		}
		else {
			++it;
		}
	}

	if (cleanupCount > 0) 
	{
		LOG_INFO(QString(u8"清理了 %1 个超时相机客户端，剩余客户端数: %2")
			.arg(cleanupCount).arg(m_socketToCamera.size()));
	}

	// 清理过期缓存
	ClearExpiredCache();
}

void CameraTcpManager::OnCacheRetryTimer()
{
	if (m_cacheEnabled) 
	{
		ProcessCachedMessages();
	}
}


void CameraTcpManager::HandleCameraMessage(int socketId, const std::string& messageData)
{
	try 
	{
		CameraJsonMessage message = CameraJsonMessage::fromJson(messageData);

		LOG_INFO(QString(u8"处理相机消息: Socket=%1, 类型=%2")
			.arg(socketId).arg(message.msg_type.c_str()));

		if (message.msg_type == "device_status") 
		{
			HandleDeviceStatusMessage(socketId, message);
		}
		else if (message.msg_type == "task_start")
		{
			HandleTaskStartAckMessage(socketId, message);
		}
		else if (message.msg_type == "task_finish") 
		{
			HandleTaskFinishMessage(socketId, message);
		}
		else if (message.msg_type == "task_cancel_ack")
		{
			HandleTaskCancelAckMessage(socketId, message);
		}
		else if (message.msg_type == "task_error") 
		{
			HandleTaskErrorMessage(socketId, message);
		}
		else 
		{
			LOG_INFO(QString(u8"收到未知相机消息类型: %1").arg(message.msg_type.c_str()));
		}

	}
	catch (const std::exception& e) 
	{
		LOG_INFO(QString(u8"处理相机消息异常: %1").arg(e.what()));
	}
}

void CameraTcpManager::HandleDeviceStatusMessage(int socketId, const CameraJsonMessage& message)
{
	std::string deviceId = message.device.dev_id;
	std::string deviceStatus = message.device.dev_status;

	LOG_INFO(QString(u8"相机设备状态同步: Socket=%1, Device=%2, Status=%3")
		.arg(socketId).arg(deviceId.c_str()).arg(deviceStatus.c_str()));

	// 注册或更新相机设备
	RegisterCamera(socketId, deviceId);
	UpdateCameraDeviceStatus(deviceId, deviceStatus);

	// 更新心跳时间（设备状态报文相当于心跳）
	UpdateCameraHeartbeat(socketId);

	// 发送响应
	CameraJsonMessage response("device_status", deviceId);
	response.device.dev_status = deviceStatus;
	response.setSuccess("success");

	SendResponse(socketId, response);

	// 发送Qt信号（保持兼容性） 触发信号
	emit m_signals.SigCameraDeviceStatus(deviceId, deviceStatus);
	// ★ 调用回调函数
	NotifyCameraDeviceStatus(deviceId, deviceStatus);

	if (deviceStatus == "online") 
	{
		// 发送Qt信号（保持兼容性）
		emit m_signals.SigCameraConnected(deviceId);
		// ★ 调用回调函数  
		NotifyCameraConnected(deviceId);
	}
}

void CameraTcpManager::HandleTaskFinishMessage(int socketId, const CameraJsonMessage& message)
{
	std::string deviceId = message.device.dev_id;

	LOG_INFO(QString(u8"相机任务完成: Device=%1, 数据条数=%2")
		.arg(deviceId.c_str()).arg(message.data.size()));

	// 更新心跳时间
	UpdateCameraHeartbeat(socketId);

	// 更新任务状态
	{
		std::lock_guard<std::mutex> lock(m_camerasMutex);
		auto clientIt = m_socketToCamera.find(socketId);
		if (clientIt != m_socketToCamera.end()) {
			clientIt->second.isTaskRunning = false;
			clientIt->second.currentTaskId.clear();
		}
	}

	// 发送响应
	CameraJsonMessage response("task_finish", deviceId);
	response.device.dev_status = "online";
	response.setSuccess("success");

	SendResponse(socketId, response);

	// 触发信号，传递扫描数据
	emit m_signals.SigCameraTaskFinished(deviceId, message.data);
	// ★ 调用回调函数
	NotifyCameraTaskFinished(deviceId, message.data);
}


void CameraTcpManager::HandleTaskErrorMessage(int socketId, const CameraJsonMessage& message)
{
	std::string deviceId = message.device.dev_id;
	std::string errorMsg = message.result.msg;

	LOG_INFO(QString(u8"相机任务错误: Device=%1, Error=%2")
		.arg(deviceId.c_str()).arg(errorMsg.c_str()));

	// 更新心跳时间
	UpdateCameraHeartbeat(socketId);

	// 更新任务状态
	{
		std::lock_guard<std::mutex> lock(m_camerasMutex);
		auto clientIt = m_socketToCamera.find(socketId);
		if (clientIt != m_socketToCamera.end()) {
			clientIt->second.isTaskRunning = false;
			clientIt->second.currentTaskId.clear();
		}
	}

	// 发送响应
	CameraJsonMessage response("task_error", deviceId);
	response.device.dev_status = "online";
	response.setSuccess("success");

	SendResponse(socketId, response);

	// 触发信号
	emit m_signals.SigCameraTaskError(deviceId, errorMsg);
	// ★ 调用回调函数
	NotifyCameraTaskError(deviceId, errorMsg);
}

void CameraTcpManager::HandleTaskCancelAckMessage(int socketId, const CameraJsonMessage& message)
{
	std::string deviceId = message.device.dev_id;
	bool success = message.isSuccess();
	std::string msg = message.result.msg;

	LOG_INFO(QString(u8": Device=%1, Success=%2, Message=%3")
		.arg(deviceId.c_str()).arg(success ? u8"成功" : u8"失败").arg(msg.c_str()));

	// 
	UpdateCameraHeartbeat(socketId);

	// 
	if (success) 
	{
		std::lock_guard<std::mutex> lock(m_camerasMutex);
		auto clientIt = m_socketToCamera.find(socketId);
		if (clientIt != m_socketToCamera.end()) 
		{
			clientIt->second.isTaskRunning = false;
			clientIt->second.currentTaskId.clear();
		}
	}

	// 
	CameraJsonMessage response("task_cancel_ack", deviceId);
	response.device.dev_status = "online";
	response.setSuccess("ack received");
	SendResponse(socketId, response);

	//
	NotifyCameraTaskCancelled(deviceId, success);
}

void CameraTcpManager::HandleTaskStartAckMessage(int socketId, const CameraJsonMessage& message)
{

	// 任务状态在下发时，直接返回UI层状态
	std::string deviceId = message.device.dev_id;
	bool success = message.isSuccess();
	std::string msg = message.result.msg;

	LOG_INFO(QString(u8"开始扫描任务下发成功当前: Device=%1, Success=%2, Message=%3")
		.arg(deviceId.c_str()).arg(success ? u8"成功" : u8"失败").arg(msg.c_str()));

	// 更新心跳信息
	UpdateCameraHeartbeat(socketId);

	// 具体操作
	//if (success)
	//{
	//	LOG_INFO(QString(u8"开始扫描任务下发成功当前: Device=%1, Success=%2, Message=%3")
	//		.arg(deviceId.c_str()).arg(success ? u8"成功" : u8"失败").arg(msg.c_str()));
	//}
}

void CameraTcpManager::RegisterCamera(int socketId, const std::string& deviceId)
{
	if (!IsValidDeviceId(deviceId)) 
	{
		LOG_INFO(QString(u8"无效的相机设备ID: %1").arg(deviceId.c_str()));
		return;
	}

	std::lock_guard<std::mutex> lock(m_camerasMutex);

	// 检查设备ID是否已被其他连接使用
	auto deviceIt = m_deviceToSocket.find(deviceId);
	if (deviceIt != m_deviceToSocket.end() && deviceIt->second != socketId) 
	{
		LOG_INFO(QString(u8"相机设备ID已被使用: %1").arg(deviceId.c_str()));
		return;
	}

	// 更新客户端信息
	auto clientIt = m_socketToCamera.find(socketId);
	if (clientIt != m_socketToCamera.end()) 
	{
		CameraTcpClientInfo& cameraInfo = clientIt->second;
		cameraInfo.deviceId = deviceId;
		cameraInfo.deviceStatus = "online";
		cameraInfo.lastHeartbeatTime = GetCurrentTimestamp();

		// 更新映射表
		m_deviceToSocket[deviceId] = socketId;

		LOG_INFO(QString(u8"相机设备注册成功: Socket=%1, Device=%2")
			.arg(socketId).arg(deviceId.c_str()));
	}
}

void CameraTcpManager::UnregisterCamera(int socketId)
{

	std::lock_guard<std::mutex> lock(m_camerasMutex);

	auto it = m_socketToCamera.find(socketId);
	if (it != m_socketToCamera.end()) 
	{
		const CameraTcpClientInfo& cameraInfo = it->second;

		// 从映射表中移除
		if (!cameraInfo.deviceId.empty()) 
		{
			m_deviceToSocket.erase(cameraInfo.deviceId);
			emit m_signals.SigCameraDisconnected(cameraInfo.deviceId);
			// ★ 调用回调函数
			NotifyCameraDisconnected(cameraInfo.deviceId);
		}

		// 移除客户端信息
		m_socketToCamera.erase(it);

		LOG_INFO(QString(u8"相机设备已注销: Socket=%1, Device=%2")
			.arg(socketId).arg(cameraInfo.deviceId.c_str()));
	}
}

void CameraTcpManager::UpdateCameraHeartbeat(int socketId)
{
	std::lock_guard<std::mutex> lock(m_camerasMutex);

	auto it = m_socketToCamera.find(socketId);
	if (it != m_socketToCamera.end()) 
	{
		it->second.lastHeartbeatTime = GetCurrentTimestamp();
	}
}

void CameraTcpManager::UpdateCameraDeviceStatus(const std::string& deviceId, const std::string& status)
{
	std::lock_guard<std::mutex> lock(m_camerasMutex);

	auto deviceIt = m_deviceToSocket.find(deviceId);
	if (deviceIt != m_deviceToSocket.end()) 
	{
		auto clientIt = m_socketToCamera.find(deviceIt->second);
		if (clientIt != m_socketToCamera.end()) 
		{
			clientIt->second.deviceStatus = status;
		}
	}
}

void CameraTcpManager::CacheMessage(const std::string& deviceId, const CameraJsonMessage& message)
{
	std::lock_guard<std::mutex> lock(m_cacheMutex);

	// 检查缓存队列是否已满
	if (m_messageCache.size() >= MAX_CACHE_SIZE) 
	{
		m_messageCache.pop(); // 移除最早的消息
		LOG_INFO(u8"相机消息缓存队列已满，移除最早的消息");
	}

	// 添加新消息到缓存队列
	m_messageCache.emplace(deviceId, message);

	LOG_INFO(QString(u8"相机消息已缓存: Device=%1, 类型=%2, 队列大小=%3/%4")
		.arg(deviceId.c_str())
		.arg(message.msg_type.c_str())
		.arg(m_messageCache.size())
		.arg(MAX_CACHE_SIZE));
}

void CameraTcpManager::ProcessCachedMessages()
{
	std::lock_guard<std::mutex> lock(m_cacheMutex);

	if (m_messageCache.empty()) 
	{
		return;
	}

	std::queue<CachedMessage> retryQueue;
	int processedCount = 0;
	int successCount = 0;

	while (!m_messageCache.empty()) 
	{
		CachedMessage cachedMsg = m_messageCache.front();
		m_messageCache.pop();

		processedCount++;

		// 检查是否超过最大重试次数
		if (cachedMsg.retryCount >= m_maxRetryCount) 
		{
			LOG_INFO(QString(u8"相机缓存消息超过最大重试次数，丢弃: Device=%1, 类型=%2")
				.arg(cachedMsg.deviceId.c_str())
				.arg(cachedMsg.message.msg_type.c_str()));
			continue;
		}

		// 尝试发送消息
		if (SendMessageToCamera(cachedMsg.deviceId, cachedMsg.message)) 
		{
			successCount++;
			LOG_INFO(QString(u8"相机缓存消息发送成功: Device=%1, 类型=%2")
				.arg(cachedMsg.deviceId.c_str())
				.arg(cachedMsg.message.msg_type.c_str()));
		}
		else 
		{
			// 发送失败，增加重试次数并重新加入队列
			cachedMsg.retryCount++;
			retryQueue.push(cachedMsg);
		}
	}

	// 将需要重试的消息重新加入缓存队列
	while (!retryQueue.empty()) 
	{
		m_messageCache.push(retryQueue.front());
		retryQueue.pop();
	}

	if (processedCount > 0) 
	{
		LOG_INFO(QString(u8"处理相机缓存消息: 总数=%1, 成功=%2, 重试=%3")
			.arg(processedCount).arg(successCount).arg(m_messageCache.size()));
	}
}

void CameraTcpManager::ClearExpiredCache()
{
	std::lock_guard<std::mutex> lock(m_cacheMutex);

	if (m_messageCache.empty()) 
	{
		return;
	}

	long long currentTime = GetCurrentTimestamp();
	std::queue<CachedMessage> validMessages;
	int expiredCount = 0;

	while (!m_messageCache.empty()) 
	{
		CachedMessage cachedMsg = m_messageCache.front();
		m_messageCache.pop();

		if ((currentTime - cachedMsg.cacheTime) > MAX_CACHE_AGE_MS) 
		{
			expiredCount++;
		}
		else 
		{
			validMessages.push(cachedMsg);
		}
	}

	m_messageCache = std::move(validMessages);

	if (expiredCount > 0) 
	{
		LOG_INFO(QString(u8"清理过期相机缓存消息: %1条").arg(expiredCount));
	}
}

long long CameraTcpManager::GetCurrentTimestamp()
{
	return std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::system_clock::now().time_since_epoch()).count();
}


void CameraTcpManager::SendResponse(int socketId, const CameraJsonMessage& response)
{
	std::string jsonData = response.toJson();
	m_tcpServer->SendData(socketId, jsonData);
}

bool CameraTcpManager::IsValidDeviceId(const std::string& deviceId) const
{
	return !deviceId.empty() && deviceId.length() >= 3 && deviceId.length() <= 50;
}

//=========================================================================
// ★ 回调函数注册实现
//=========================================================================

void CameraTcpManager::RegisterCameraConnectedCallback(CameraConnectedCallback callback)
{
	if (!callback) return;

	std::lock_guard<std::mutex> lock(m_callbacksMutex);
	m_connectedCallbacks.push_back(std::move(callback));

	LOG_INFO(QString(u8"注册相机连接回调函数，总数: %1").arg(m_connectedCallbacks.size()));
}

void CameraTcpManager::RegisterCameraDisconnectedCallback(CameraDisconnectedCallback callback)
{
	if (!callback) return;

	std::lock_guard<std::mutex> lock(m_callbacksMutex);
	m_disconnectedCallbacks.push_back(std::move(callback));

	LOG_INFO(QString(u8"注册相机断开回调函数，总数: %1").arg(m_disconnectedCallbacks.size()));
}

void CameraTcpManager::RegisterCameraTaskFinishedCallback(CameraTaskFinishedCallback callback)
{
	if (!callback) return;

	std::lock_guard<std::mutex> lock(m_callbacksMutex);
	m_taskFinishedCallbacks.push_back(std::move(callback));

	LOG_INFO(QString(u8"注册任务完成回调函数，总数: %1").arg(m_taskFinishedCallbacks.size()));
}

void CameraTcpManager::RegisterCameraTaskCancelledCallback(CameraTaskCancelledCallback callback)
{
	if (!callback) return;

	std::lock_guard<std::mutex> lock(m_callbacksMutex);
	m_taskCancelledCallbacks.push_back(std::move(callback));

	LOG_INFO(QString(u8"注册任务取消回调函数，总数 %1").arg(m_taskCancelledCallbacks.size()));
}

void CameraTcpManager::RegisterCameraTaskErrorCallback(CameraTaskErrorCallback callback)
{
	if (!callback) return;

	std::lock_guard<std::mutex> lock(m_callbacksMutex);
	m_taskErrorCallbacks.push_back(std::move(callback));

	LOG_INFO(QString(u8"注册任务错误回调函数，总数: %1").arg(m_taskErrorCallbacks.size()));
}

void CameraTcpManager::RegisterCameraDeviceStatusCallback(CameraDeviceStatusCallback callback)
{
	if (!callback) return;

	std::lock_guard<std::mutex> lock(m_callbacksMutex);
	m_deviceStatusCallbacks.push_back(std::move(callback));

	LOG_INFO(QString(u8"注册设备状态回调函数，总数: %1").arg(m_deviceStatusCallbacks.size()));
}

void CameraTcpManager::RegisterCameraManagerErrorCallback(CameraManagerErrorCallback callback)
{
	if (!callback) return;

	std::lock_guard<std::mutex> lock(m_callbacksMutex);
	m_managerErrorCallbacks.push_back(std::move(callback));

	LOG_INFO(QString(u8"注册管理器错误回调函数，总数: %1").arg(m_managerErrorCallbacks.size()));
}

//=========================================================================
// ★ 回调函数通知实现
//=========================================================================

void CameraTcpManager::NotifyCameraConnected(const std::string& deviceId)
{
	std::lock_guard<std::mutex> lock(m_callbacksMutex);

	LOG_INFO(QString(u8"通知 %1 个回调：相机连接 %2")
		.arg(m_connectedCallbacks.size())
		.arg(QString::fromStdString(deviceId)));

	for (const auto& callback : m_connectedCallbacks) {
		if (callback) {
			try {
				callback(deviceId);
			}
			catch (const std::exception& e) {
				LOG_INFO(QString(u8"相机连接回调函数异常: %1").arg(e.what()));
			}
		}
	}
}

void CameraTcpManager::NotifyCameraDisconnected(const std::string& deviceId)
{
	std::lock_guard<std::mutex> lock(m_callbacksMutex);

	for (const auto& callback : m_disconnectedCallbacks) {
		if (callback) {
			try {
				callback(deviceId);
			}
			catch (const std::exception& e) {
				LOG_INFO(QString(u8"相机断开回调函数异常: %1").arg(e.what()));
			}
		}
	}
}

void CameraTcpManager::NotifyCameraTaskFinished(const std::string& deviceId, const std::vector<CameraScanData>& scanData)
{
	std::lock_guard<std::mutex> lock(m_callbacksMutex);

	for (const auto& callback : m_taskFinishedCallbacks) {
		if (callback) {
			try {
				callback(deviceId, scanData);
			}
			catch (const std::exception& e) {
				LOG_INFO(QString(u8"任务完成回调函数异常: %1").arg(e.what()));
			}
		}
	}
}

void CameraTcpManager::NotifyCameraTaskError(const std::string& deviceId, const std::string& errorMsg)
{
	std::lock_guard<std::mutex> lock(m_callbacksMutex);

	for (const auto& callback : m_taskErrorCallbacks) 
	{
		if (callback) 
		{
			try 
			{
				callback(deviceId, errorMsg);
			}
			catch (const std::exception& e) 
			{
				LOG_INFO(QString(u8"任务错误回调函数异常: %1").arg(e.what()));
			}
		}
	}
}

void CameraTcpManager::NotifyCameraTaskCancelled(const std::string& deviceId, bool success)
{
	std::lock_guard<std::mutex> lock(m_callbacksMutex);

	for (const auto& callback : m_taskCancelledCallbacks) 
	{
		if (callback) 
		{
			try 
			{
				callback(deviceId, success);
			}
			catch (const std::exception& e) 
			{
				LOG_INFO(QString(u8"任务取消回调函数异常: %1").arg(e.what()));
			}
		}
	}
}

void CameraTcpManager::NotifyCameraDeviceStatus(const std::string& deviceId, const std::string& status)
{
	std::lock_guard<std::mutex> lock(m_callbacksMutex);

	for (const auto& callback : m_deviceStatusCallbacks) 
	{
		if (callback) 
		{
			try 
			{
				callback(deviceId, status);
			}
			catch (const std::exception& e) 
			{
				LOG_INFO(QString(u8"设备状态回调函数异常: %1").arg(e.what()));
			}
		}
	}
}

void CameraTcpManager::NotifyCameraManagerError(int errorCode, const std::string& errorMessage)
{
	std::lock_guard<std::mutex> lock(m_callbacksMutex);

	for (const auto& callback : m_managerErrorCallbacks) {
		if (callback) {
			try {
				callback(errorCode, errorMessage);
			}
			catch (const std::exception& e) {
				LOG_INFO(QString(u8"管理器错误回调函数异常: %1").arg(e.what()));
			}
		}
	}
}
