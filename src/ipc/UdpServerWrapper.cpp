#include "UdpServerWrapper.h"
#include "hv/hlog.h"
#include "CLogManager.h"

#include "UdpServerWrapper.h"
#include "global.h"
#include <algorithm>
#include <sstream>
#include <chrono>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif


//----------------------------WSClientWrapper----------------------------------------------
//----------------------------WSClientWrapper----------------------------------------------
//----------------------------WSClientWrapper----------------------------------------------

// UdpServerWrapper Implementation
UdpServerWrapper::UdpServerWrapper()
	: m_pImpl(std::make_unique<UdpServerWrapperImpl>())
{
}

bool UdpServerWrapper::Start(const std::string& ip, int port)
{
	return m_pImpl ? m_pImpl->Start(ip, port) : false;
}

void UdpServerWrapper::Stop()
{
	if (m_pImpl) {
		m_pImpl->Stop();
	}
}

bool UdpServerWrapper::IsRunning() const
{
	return m_pImpl ? m_pImpl->IsRunning() : false;
}

int UdpServerWrapper::GetPort() const
{
	return m_pImpl ? m_pImpl->GetPort() : 0;
}

bool UdpServerWrapper::SendData(const std::string& clientIp, int clientPort, const std::string& data)
{
	return m_pImpl ? m_pImpl->SendData(clientIp, clientPort, data) : false;
}

bool UdpServerWrapper::ReplyData(const UdpMessage& message, const std::string& data)
{
	return m_pImpl ? m_pImpl->ReplyData(message, data) : false;
}

int UdpServerWrapper::BroadcastData(const std::string& data)
{
	return m_pImpl ? m_pImpl->BroadcastData(data) : 0;
}

int UdpServerWrapper::GetActiveClientCount() const
{
	return m_pImpl ? m_pImpl->GetActiveClientCount() : 0;
}

std::map<std::string, UdpClient> UdpServerWrapper::GetAllClients() const
{
	return m_pImpl ? m_pImpl->GetAllClients() : std::map<std::string, UdpClient>();
}

int UdpServerWrapper::CleanupTimeoutClients(int timeoutSeconds)
{
	return m_pImpl ? m_pImpl->CleanupTimeoutClients(timeoutSeconds) : 0;
}

void UdpServerWrapper::SetOnMessageCallback(std::function<void(const UdpMessage&)> callback)
{
	if (m_pImpl) {
		m_pImpl->SetOnMessageCallback(std::move(callback));
	}
}

void UdpServerWrapper::SetOnNewClientCallback(std::function<void(const UdpClient&)> callback)
{
	if (m_pImpl) {
		m_pImpl->SetOnNewClientCallback(std::move(callback));
	}
}

void UdpServerWrapper::SetOnErrorCallback(std::function<void(int, const std::string&)> callback)
{
	if (m_pImpl) {
		m_pImpl->SetOnErrorCallback(std::move(callback));
	}
}

void UdpServerWrapper::SetServerConfig(int bufferSize, bool enableBroadcast, int clientTimeout)
{
	if (m_pImpl) {
		m_pImpl->SetServerConfig(bufferSize, enableBroadcast, clientTimeout);
	}
}


//----------------------------WSClientWrapperImpl----------------------------------------------
//----------------------------WSClientWrapperImpl----------------------------------------------
//----------------------------WSClientWrapperImpl----------------------------------------------

UdpServerWrapperImpl::UdpServerWrapperImpl()
	: m_server(std::make_unique<hv::UdpServer>())
	, m_bindPort(0)
	, m_isRunning(false)
	, m_bufferSize(8192)
	, m_enableBroadcast(false)
	, m_clientTimeout(300)
{
}

UdpServerWrapperImpl::~UdpServerWrapperImpl()
{
	Stop();
}

bool UdpServerWrapperImpl::Start(const std::string& ip, int port)
{
	if (m_isRunning) {
		LOG_INFO(QString("UDP Server is already running on %1:%2").arg(ip.c_str()).arg(port));
		return true;
	}

	try {
		m_bindIp = ip;
		m_bindPort = port;

		// ���÷���������
		if (m_enableBroadcast) 
		{
			// claude_qa
			//m_server->setBroadcast(true);
		}

		// ���ûص�����
		m_server->onMessage = [this](const hv::SocketChannelPtr& channel, hv::Buffer* buf) {
			OnMessage(channel, buf);
		};

		// ���������
		int ret = m_server->createsocket(port, ip.c_str());
		if (ret != 0) {
			HandleError(ret, "Failed to create UDP socket");
			return false;
		}
		// claude_qa
		//ret = m_server->start();
		m_server->start();
		if (ret != 0) {
			HandleError(ret, "Failed to start UDP server");
			return false;
		}

		m_isRunning = true;
		LOG_INFO(QString("UDP Server started successfully on %1:%2").arg(ip.c_str()).arg(port));
		return true;

	}
	catch (const std::exception& e) {
		HandleError(-1, std::string("Exception starting UDP server: ") + e.what());
		return false;
	}
}

void UdpServerWrapperImpl::Stop()
{
	if (!m_isRunning) {
		return;
	}

	try {
		// �������пͻ��˼�¼
		{
			std::lock_guard<std::mutex> lock(m_clientsMutex);
			m_clients.clear();
		}

		// ֹͣ������
		m_server->stop();
		m_isRunning = false;

		LOG_INFO(QString("UDP Server stopped on %1:%2").arg(m_bindIp.c_str()).arg(m_bindPort));

	}
	catch (const std::exception& e) {
		HandleError(-1, std::string("Exception stopping UDP server: ") + e.what());
	}
}

bool UdpServerWrapperImpl::IsRunning() const
{
	return m_isRunning;
}

int UdpServerWrapperImpl::GetPort() const
{
	return m_isRunning ? m_bindPort : 0;
}

bool UdpServerWrapperImpl::SendData(const std::string& clientIp, int clientPort, const std::string& data)
{
	//if (!m_isRunning) {
	//	HandleError(-1, "UDP server is not running");
	//	return false;
	//}

	//try 
	//{
	//	// ����Ŀ���ַ
	//	sockaddr_in addr;
	//	memset(&addr, 0, sizeof(addr));
	//	addr.sin_family = AF_INET;
	//	addr.sin_port = htons(clientPort);
	//	inet_pton(AF_INET, clientIp.c_str(), &addr.sin_addr);

	//	// ��������
	//	// claude_qa
	//	//int ret = m_server->sendto(data.c_str(), data.size(), (struct sockaddr*)&addr, sizeof(addr));
	//	int ret = 0;
	//	m_server->sendto(data.c_str(), data.size(), (struct sockaddr*)&addr, sizeof(addr));

	//	if (ret < 0) {
	//		HandleError(ret, "Failed to send UDP data to " + clientIp + ":" + std::to_string(clientPort));
	//		return false;
	//	}

	//	LOG_INFO(QString("UDP data sent to %1:%2, Size=%3 bytes")
	//		.arg(clientIp.c_str()).arg(clientPort).arg(data.size()));

	//	return true;

	//}
	//catch (const std::exception& e) {
	//	HandleError(-1, std::string("Exception sending UDP data: ") + e.what());
		return false;
	//}
}

bool UdpServerWrapperImpl::ReplyData(const UdpMessage& message, const std::string& data)
{
	return SendData(message.clientIp, message.clientPort, data);
}

int UdpServerWrapperImpl::BroadcastData(const std::string& data)
{
	if (!m_enableBroadcast) {
		HandleError(-1, "Broadcast is not enabled");
		return 0;
	}

	std::lock_guard<std::mutex> lock(m_clientsMutex);

	int successCount = 0;
	for (const auto& pair : m_clients) {
		const UdpClient& client = pair.second;
		if (SendData(client.clientIp, client.clientPort, data)) {
			successCount++;
		}
	}

	LOG_INFO(QString("Broadcast UDP data to %1/%2 clients")
		.arg(successCount).arg(m_clients.size()));

	return successCount;
}

int UdpServerWrapperImpl::GetActiveClientCount() const
{
	std::lock_guard<std::mutex> lock(m_clientsMutex);
	return static_cast<int>(m_clients.size());
}

std::map<std::string, UdpClient> UdpServerWrapperImpl::GetAllClients() const
{
	std::lock_guard<std::mutex> lock(m_clientsMutex);
	return m_clients;
}

int UdpServerWrapperImpl::CleanupTimeoutClients(int timeoutSeconds)
{
	std::lock_guard<std::mutex> lock(m_clientsMutex);

	int64_t currentTime = GetCurrentTimeMs();
	int64_t timeoutMs = timeoutSeconds * 1000;
	int cleanupCount = 0;

	auto it = m_clients.begin();
	while (it != m_clients.end()) {
		if ((currentTime - it->second.lastActiveTime) > timeoutMs) {
			LOG_INFO(QString("Cleanup timeout UDP client: %1").arg(it->first.c_str()));
			it = m_clients.erase(it);
			cleanupCount++;
		}
		else {
			++it;
		}
	}

	if (cleanupCount > 0) {
		LOG_INFO(QString("Cleaned up %1 timeout UDP clients, remaining: %2")
			.arg(cleanupCount).arg(m_clients.size()));
	}

	return cleanupCount;
}

void UdpServerWrapperImpl::SetOnMessageCallback(std::function<void(const UdpMessage&)> callback)
{
	m_onMessageCallback = std::move(callback);
}

void UdpServerWrapperImpl::SetOnNewClientCallback(std::function<void(const UdpClient&)> callback)
{
	m_onNewClientCallback = std::move(callback);
}

void UdpServerWrapperImpl::SetOnErrorCallback(std::function<void(int, const std::string&)> callback)
{
	m_onErrorCallback = std::move(callback);
}

void UdpServerWrapperImpl::SetServerConfig(int bufferSize, bool enableBroadcast, int clientTimeout)
{
	m_bufferSize = bufferSize;
	m_enableBroadcast = enableBroadcast;
	m_clientTimeout = clientTimeout;

	LOG_INFO(QString("UDP Server config updated: bufferSize=%1, broadcast=%2, timeout=%3")
		.arg(bufferSize).arg(enableBroadcast).arg(clientTimeout));
}

void UdpServerWrapperImpl::OnMessage(const hv::SocketChannelPtr& channel, hv::Buffer* buf)
{
	//if (!channel || !buf) {
	//	return;
	//}

	//try 
	//{
	//	// ��ȡ�ͻ�����Ϣ
	//	std::string clientIp = channel->peeraddr();
	//	// claude_qa
	//	//int clientPort = channel->peerport();
	//	//std::string clientKey = GetAddressKey(clientIp, clientPort);

	//	// ���¿ͻ��˻�Ծʱ��
	//	UpdateClientActiveTime(clientKey);

	//	// ������Ϣ����
	//	std::string data(static_cast<const char*>(buf->data()), buf->size());
	//	UdpMessage message(clientIp, clientPort, data);

	//	//LOG_INFO(QString("UDP message received from %1:%2, Size=%3 bytes").arg(clientIp.c_str()).arg(clientPort).arg(data.size()));

	//	// ����Ƿ����¿ͻ���
	//	{
	//		std::lock_guard<std::mutex> lock(m_clientsMutex);
	//		if (m_clients.find(clientKey) == m_clients.end()) {
	//			// �¿ͻ���
	//			UdpClient newClient = CreateClientInfo(channel);
	//			m_clients[clientKey] = newClient;

	//			LOG_INFO(QString("New UDP client discovered: %1, Total=%2")
	//				.arg(clientKey.c_str()).arg(m_clients.size()));

	//			// �����¿ͻ��˻ص�
	//			if (m_onNewClientCallback) 
	//			{
	//				m_onNewClientCallback(newClient);
	//			}
	//		}
	//	}

	//	// ������Ϣ�ص�
	//	if (m_onMessageCallback) 
	//	{
	//		m_onMessageCallback(message);
	//	}

	//}
	//catch (const std::exception& e) 
	//{
	//	HandleError(-1, std::string("Exception processing UDP message: ") + e.what());
	//}
}

void UdpServerWrapperImpl::HandleError(int errorCode, const std::string& errorMessage)
{
	LOG_INFO(QString("UDP Server Error [%1]: %2").arg(errorCode).arg(errorMessage.c_str()));

	if (m_onErrorCallback) {
		m_onErrorCallback(errorCode, errorMessage);
	}
}

UdpClient UdpServerWrapperImpl::CreateClientInfo(const hv::SocketChannelPtr& channel)
{
	UdpClient client;
	if (channel) 
	{
		client.clientIp = channel->peeraddr();
		//client.clientPort = channel->peerport();
		client.lastActiveTime = GetCurrentTimeMs();
	}
	return client;
}

void UdpServerWrapperImpl::UpdateClientActiveTime(const std::string& clientKey)
{
	std::lock_guard<std::mutex> lock(m_clientsMutex);

	auto it = m_clients.find(clientKey);
	if (it != m_clients.end()) {
		it->second.lastActiveTime = GetCurrentTimeMs();
	}
}

int64_t UdpServerWrapperImpl::GetCurrentTimeMs()
{
	return std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::steady_clock::now().time_since_epoch()).count();
}

std::string UdpServerWrapperImpl::GetAddressKey(const std::string& ip, int port)
{
	return ip + ":" + std::to_string(port);
}
