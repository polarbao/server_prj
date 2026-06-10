#include "TcpServerWrapper.h"
#include "hv/hlog.h"
#include "CLogManager.h"
#include <algorithm>
#include <sstream>

#include <iostream>
#include <string>
#include <regex>
#include <utility>
#include <chrono>
#include <thread>




//----------------------------TcpServerWrapper----------------------------------------------
//----------------------------TcpServerWrapper----------------------------------------------
//----------------------------TcpServerWrapper----------------------------------------------

TcpServerWrapper::TcpServerWrapper()
	: m_pImpl(std::make_unique<TcpServerWrapperImpl>())
{

}


TcpServerWrapper::~TcpServerWrapper()
{

}

bool TcpServerWrapper::Start(const std::string& ip /*= "0.0.0.0"*/, int port /*= 8080*/)
{
	return m_pImpl ? m_pImpl->Start(ip, port) : false;
}

void TcpServerWrapper::Stop()
{
	if (m_pImpl)
	{
		m_pImpl->Stop();
	}
}

bool TcpServerWrapper::IsRunning() const
{
	return m_pImpl ? m_pImpl->IsRunning() : false;
}

int TcpServerWrapper::GetPort() const
{
	return m_pImpl ? m_pImpl->GetPort() : 0;
}

bool TcpServerWrapper::SendData(int connId, const std::string& data)
{
	return m_pImpl ? m_pImpl->SendData(connId, data) : false;

}

int TcpServerWrapper::BroadcastData(const std::string& data)
{
	return m_pImpl ? m_pImpl->BroadcastData(data) : 0;
}

bool TcpServerWrapper::DisconnectClient(int connId)
{
	return m_pImpl ? m_pImpl->DisconnectClient(connId) : false;

}

int TcpServerWrapper::GetConnectionCount() const
{

	return m_pImpl ? m_pImpl->GetConnectionCount() : 0;
}

std::map<int, TcpConnection> TcpServerWrapper::GetAllConnections() const
{
	return m_pImpl ? m_pImpl->GetAllConnections() : std::map<int, TcpConnection>();
}

void TcpServerWrapper::SetOnConnectionCallback(std::function<void(const TcpConnection&)> callback)
{
	if (m_pImpl)
	{
		m_pImpl->SetOnConnectionCallback(std::move(callback));
	}

}

void TcpServerWrapper::SetOnDisconnectionCallback(std::function<void(int)> callback)
{
	if (m_pImpl)
	{
		m_pImpl->SetOnDisconnectionCallback(std::move(callback));
	}
}

void TcpServerWrapper::SetOnMessageCallback(std::function<void(const OldTcpMessage &)> callback)
{
	if (m_pImpl)
	{
		m_pImpl->SetOnMessageCallback(std::move(callback));
	}
}

void TcpServerWrapper::SetOnErrorCallback(std::function<void(int, const std::string&)> callback)
{
	if (m_pImpl)
	{
		m_pImpl->SetOnErrorCallback(std::move(callback));
	}
}

void TcpServerWrapper::SetServerConfig(int maxConnection /*= 1000*/, bool keepAlive /*= true*/, int timeOut /*= 60*/)
{
	if (m_pImpl) {
		m_pImpl->SetServerConfig(maxConnection, keepAlive, timeOut);
	}
}


//----------------------------TcpServerWrapperImpl----------------------------------------------
//----------------------------TcpServerWrapperImpl----------------------------------------------
//----------------------------TcpServerWrapperImpl----------------------------------------------

TcpServerWrapperImpl::TcpServerWrapperImpl()
	: m_server(std::make_unique<hv::TcpServer>())
	, m_bindPort(0)
	, m_isRunning(false)
	, m_connectionIdCounter(0)
	, m_maxConnections(1000)
	, m_keepAlive(true)
	, m_timeout(60)
{

}

TcpServerWrapperImpl::~TcpServerWrapperImpl()
{
	Stop();
}

bool TcpServerWrapperImpl::Start(const std::string& ip /*= "0.0.0.0"*/, int port /*= 8080*/)
{
	if (m_isRunning)
	{
		LOG_INFO(QString("TCP Server is already running on %1:%2").arg(ip.c_str()).arg(port));
		return true;
	}

	try
	{
		m_bindIp = ip;
		m_bindPort = port;

		// ���÷���������
		//m_server->setMaxConnectionCount(m_maxConnections);
		//m_server->setKeepalive(m_keepAlive);
		//if (m_timeout > 0) 
		//{
		//	m_server->setReadTimeout(m_timeout * 1000); // ת��Ϊ����
		//}

		// ���ûص�����
		m_server->onConnection = [this](const hv::SocketChannelPtr& channel) {
			OnConnection(channel);
		};

		m_server->onMessage = [this](const hv::SocketChannelPtr& channel, hv::Buffer* buf) {
			OnMessage(channel, buf);
		};

		m_server->onWriteComplete = [this](const hv::SocketChannelPtr& channel, hv::Buffer* buf) {
			OnWriteComplete(channel, buf);
		};

		// ���������
		int ret = m_server->createsocket(port);
		if (ret < 0) 
		{
			HandleError(ret, "Failed to create socket");
			return false;
		}

		//ret = m_server->start();
		m_server->start();

		//if (ret != 0) 
		//{
		//	HandleError(ret, "Failed to start TCP server");
		//	return false;
		//}

		m_isRunning = true;
		LOG_INFO(QString("TCP Server started successfully on %1:%2").arg(ip.c_str()).arg(port));
		return true;
	}
	catch (const std::exception& e)
	{
		HandleError(-1, std::string("Exception starting TCP server: ") + e.what());
		return false;
	}

}

void TcpServerWrapperImpl::Stop()
{
	if (!m_isRunning) 
	{
		return;
	}

	try 
	{
		// ������������
		{
			std::lock_guard<std::mutex> lock(m_connectionsMutex);
			m_connections.clear();
			m_channelToConnId.clear();
			m_connIdToChannel.clear();
		}

		// ֹͣ������
		m_server->stop();
		m_isRunning = false;

		LOG_INFO(QString("TCP Server stopped on %1:%2").arg(m_bindIp.c_str()).arg(m_bindPort));

	}
	catch (const std::exception& e) 
	{
		HandleError(-1, std::string("Exception stopping TCP server: ") + e.what());
	}
}

bool TcpServerWrapperImpl::IsRunning() const
{
	return m_isRunning;

}

int TcpServerWrapperImpl::GetPort() const
{
	return m_isRunning ? m_bindPort : 0;

}

bool TcpServerWrapperImpl::SendData(int connId, const std::string& data)
{
	hv::SocketChannelPtr channel;
	// ʹ��RAII����������С����������
	{
		std::lock_guard<std::mutex> lock(m_connectionsMutex);

		auto it = m_connIdToChannel.find(connId);
		if (it == m_connIdToChannel.end())
		{
			HandleError(-1, "Connection not found: " + std::to_string(connId));
			return false;
		}
		channel = it->second;
	}

	if (!channel || !channel->isConnected())
	{
		HandleError(-1, "Channel is disconnected: " + std::to_string(connId));
		return false;
	}
	try
	{
		int ret = channel->write(data.c_str(), data.size());
		//int ret = hio_write(channel->io_, a1.c_str(), a1.size());
		if (ret < 0)
		{
			HandleError(ret, u8"Failed to send data to connection: " + std::to_string(connId));
			return false;
		}
		return true;
	}
	catch (const std::exception& e)
	{
		HandleError(-1, std::string(u8"Exception sending data: ") + e.what());
		return false;
	}
}

int TcpServerWrapperImpl::BroadcastData(const std::string& data)
{
	std::vector<hv::SocketChannelPtr> channels;

	//��ȡ����channel�ĸ�����Ϣ
	{
		std::lock_guard<std::mutex> lock(m_connectionsMutex);
		channels.reserve(m_connIdToChannel.size());
		
		for (const auto& pair : m_connIdToChannel)
		{
			if (pair.second && pair.second->isConnected())
			{
				channels.push_back(pair.second);
			}
		}

	}

	//�������ʵ�ʷ�������
	int successCnt = 0;
	for (const auto& channel : channels)
	{
		try
		{
			if (channel && channel->isConnected())
			{
				int ret = channel->write(data.c_str(), data.size());
				if (ret >= 0)
				{
					successCnt++;
				}
			}
		}
		catch (const std::exception& e)
		{
			LOG_INFO(QString(u8"Exception broadcasting to connection: %1").arg(e.what()));

		}
	}
	LOG_INFO(QString(u8"Broadcast data to %1/%2 connections")
		.arg(successCnt).arg(channels.size()));

	return successCnt;
}

bool TcpServerWrapperImpl::DisconnectClient(int connId)
{
	std::lock_guard lock(m_connectionsMutex);

	auto it = m_connIdToChannel.find(connId);
	if (it == m_connIdToChannel.end())
	{
		return false;
	}

	try
	{
		hv::SocketChannelPtr channel = it->second;
		if (channel)
		{
			channel->close();
			return true;
		}
	}
	catch (const std::exception& e)
	{
		HandleError(-1, std::string(u8"Exception disconnecting client: ") + e.what());
	}
	return false;

}

int TcpServerWrapperImpl::GetConnectionCount() const
{
	std::lock_guard<std::mutex> lock(m_connectionsMutex);
	return static_cast<int>(m_connections.size());
}

std::map<int, TcpConnection> TcpServerWrapperImpl::GetAllConnections() const
{
	std::lock_guard<std::mutex> lock(m_connectionsMutex);
	return m_connections;
}

void TcpServerWrapperImpl::SetOnConnectionCallback(std::function<void(const TcpConnection&)> callback)
{
	m_onConnectionCallback = std::move(callback);
}

void TcpServerWrapperImpl::SetOnDisconnectionCallback(std::function<void(int)> callback)
{
	m_onDisconnectionCallback = std::move(callback);

}

void TcpServerWrapperImpl::SetOnMessageCallback(std::function<void(const OldTcpMessage&)> callback)
{
	m_onMessageCallback = std::move(callback);

}

void TcpServerWrapperImpl::SetOnErrorCallback(std::function<void(int, const std::string&)> callback)
{
	m_onErrorCallback = std::move(callback);

}

void TcpServerWrapperImpl::SetServerConfig(int maxConnections /*= 1000*/, bool keepAlive /*= true*/, int timeout /*= 60*/)
{
	m_maxConnections = maxConnections;
	m_keepAlive = keepAlive;
	m_timeout = timeout;

	LOG_INFO(QString(u8"TCP Server config updated: maxConn=%1, keepAlive=%2, timeout=%3")
		.arg(maxConnections).arg(keepAlive).arg(timeout));
}

bool TcpServerWrapperImpl::SendDataAsync(int connId, const std::string& data)
{
	if (!m_asyncSendEnabled)
	{
		//�첽����δ���ã�ʹ��ͬ������
		return SendData(connId, data);
	}

	{
		std::lock_guard<std::mutex> lock(m_asyncSendMutex);
		// �ж��첽�����Ƿ�����
		if (m_asyncSendQueue.size() >= static_cast<size_t>(m_maxAsyncQueueSize.load()))
		{
			LOG_INFO(QString(u8"�첽���Ͷ��������������������Ϣ"));
			m_asyncSendQueue.pop();
		}
		// ��ӵ��첽���Ͷ���
		m_asyncSendQueue.emplace(connId, data);
	}
	LOG_INFO(QString(u8"��Ϣ�Ѽ����첽���Ͷ���: ConnID=%1, ���д�С=%2")
		.arg(connId).arg(m_asyncSendQueue.size()));
	return true;
}

void TcpServerWrapperImpl::EnableAsyncSend(bool enable /*= true*/)
{
	bool wasEnable = m_asyncSendEnabled.exchange(enable);

	if (enable && !wasEnable)
	{
		m_asyncSendTimer->stop();
		LOG_INFO("�첽���ͻ����ѽ���");
		// ����첽���Ͷ�ʱ��
		OnAsyncSendTimer();
	}
	else if(!enable && wasEnable)
	{
		// ֹͣ�첽���Ͷ�ʱ��
		m_asyncSendTimer->stop();
		// ����ʣ��Ķ�����Ϣ
		OnAsyncSendTimer();
	}
}

void TcpServerWrapperImpl::SetAsyncSendConfig(int maxQueueSize /*= 5000*/, int processInterval /*= 50*/)
{
	m_maxAsyncQueueSize = maxQueueSize;
	m_asyncProcessInterval = processInterval;

	// �����ʱ���������У����¼��
	if (m_asyncSendEnabled && m_asyncSendTimer->isActive())
	{
		m_asyncSendTimer->stop();
		m_asyncSendTimer->start(processInterval);
	}

	LOG_INFO(QString(u8"�첽���������Ѹ���: �����д�С=%1, ������=%2ms")
		.arg(maxQueueSize).arg(processInterval));
}

void TcpServerWrapperImpl::HandleNewConnection(const hv::SocketChannelPtr& channel)
{
	TcpConnection conn;

	{
		// ������
		std::lock_guard<std::mutex> lock(m_connectionsMutex);
		int connId = GenerateConnectionId();
		CreateConnectionInfo(channel, conn);
		conn.connId = connId;

		m_connections[connId] = conn;
		m_channelToConnId[channel] = connId;
		m_connIdToChannel[connId] = channel;

		LOG_INFO(QString(u8"New TCP connection: ID=%1, Client=%2:%3, Total=%4")
			.arg(connId).arg(conn.clientIp.c_str()).arg(conn.clientPort).arg(m_connections.size()));
	}
	// �����ⴥ���ص�
	if (m_onConnectionCallback)
	{
		m_onConnectionCallback(conn);
	}
}

void TcpServerWrapperImpl::HandleDisconnection(const hv::SocketChannelPtr& channel)
{
	int connId = -1;
	{
		// ���ӶϿ�
		std::lock_guard<std::mutex> lock(m_connectionsMutex);

		auto it = m_channelToConnId.find(channel);
		if (it != m_channelToConnId.end())
		{
			int connId = it->second;

			m_connections.erase(connId);
			m_channelToConnId.erase(channel);
			m_connIdToChannel.erase(connId);

			LOG_INFO(QString(u8"TCP connection closed: ID=%1, Total=%2")
				.arg(connId).arg(m_connections.size()));
		}
	}
	// �����ص�
	if ( -1 != connId && m_onDisconnectionCallback)
	{
		m_onDisconnectionCallback(connId);
	}
}

void TcpServerWrapperImpl::OnConnection(const hv::SocketChannelPtr& channel)
{
	if (!channel)
	{
		return;
	}

	if (channel->isConnected())
	{
		HandleNewConnection(channel);
	}
	else
	{
		HandleDisconnection(channel);
	}
}

void TcpServerWrapperImpl::OnMessage(const hv::SocketChannelPtr& channel, hv::Buffer* buf)
{
	if (!channel || !buf) 
	{
		return;
	}

	OldTcpMessage message;
	//��С��ʹ�÷�Χ
	{
		std::lock_guard<std::mutex> lock(m_connectionsMutex);

		auto it = m_channelToConnId.find(channel);
		if (it == m_channelToConnId.end())
		{
			HandleError(-1, "Received message from unknown connection");
			return;
		}

		int connId = it->second;
		std::string data(static_cast<const char*>(buf->data()), buf->size());
		message = OldTcpMessage(connId, data);
		LOG_INFO(QString("TCP message received: ConnID=%1, Size=%2 bytes")
			.arg(connId).arg(data.size()));
	}

	// ��������ûص���������������
	if (m_onMessageCallback) 
	{
		m_onMessageCallback(message);
	}
}

void TcpServerWrapperImpl::OnWriteComplete(const hv::SocketChannelPtr& channel, hv::Buffer* buf)
{
	// ���������ﴦ��д������¼�������ͳ�Ʒ�����������
	if (buf) 
	{
		std::lock_guard<std::mutex> lock(m_connectionsMutex);
		auto it = m_channelToConnId.find(channel);
		if (it != m_channelToConnId.end()) 
		{
			LOG_INFO(QString("TCP data sent: ConnID=%1, Size=%2 bytes")
				.arg(it->second).arg(buf->size()));
		}
	}
}

void TcpServerWrapperImpl::HandleError(int errorCode, const std::string& errorMessage)
{
	LOG_INFO(QString("TCP Server Error [%1]: %2").arg(errorCode).arg(errorMessage.c_str()));

	if (m_onErrorCallback) 
	{
		m_onErrorCallback(errorCode, errorMessage);
	}
}

void TcpServerWrapperImpl::CreateConnectionInfo(const hv::SocketChannelPtr& channel, TcpConnection& connParam)
{
	TcpConnection conn;
	if (channel) 
	{
		auto ipData = channel->peeraddr();
		std::pair<std::string, int> data;
		//�ͻ������Ӳ��ip���˿�
		{
			std::regex pattern(R"(^(\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3})(?::(\d{1,5}))?$)");
			std::smatch matches;

			if (std::regex_match(ipData, matches, pattern))
			{
				std::string ip = matches[1];
				int port = 0;
				// ����ж˿���
				if (matches.size() > 2 && matches[2].matched)
				{
					try
					{
						port = std::stoi(matches[2]);
					}
					catch (const std::exception& e)
					{
						std::cerr << "���棺��Ч�Ķ˿ںŸ�ʽ" << std::endl;
					}
				}
				data = { ip, port };
			}
		}

		//conn.clientIp = channel->peeraddr();
		conn.clientIp = data.first;
		conn.clientPort =data.second;
		conn.isConnected = channel->isConnected();
	}
	connParam = conn;
	//return connParam;
}

int TcpServerWrapperImpl::GenerateConnectionId()
{
	return ++m_connectionIdCounter;

}

void TcpServerWrapperImpl::OnAsyncSendTimer()
{
	if (!m_asyncSendEnabled)
	{
		return;
	}

	std::queue<AsyncSendItem> retryQueue;
	int processCount = 0;
	int successCount = 0;
	const int maxProcessPerCycle = 100;

	{
		std::lock_guard<std::mutex> lock(m_asyncSendMutex);

		while (!m_asyncSendQueue.empty() && processCount < maxProcessPerCycle)
		{
			AsyncSendItem item = m_asyncSendQueue.front();
			m_asyncSendQueue.pop();
			processCount++;

			// �����Ϣ�Ƿ�ʱ
			long long currentTime = std::chrono::duration_cast<std::chrono::milliseconds>(
				std::chrono::system_clock::now().time_since_epoch()).count();
			if ((currentTime - item.timestamp) > MESSAGE_TIMEOUT_MS)
			{
				LOG_INFO(QString(u8"������ʱ��Ϣ: ConnID=%1").arg(item.connId));
				continue;
			}

			// ���Է�����Ϣ (�������ڽ����������)
			bool success = false;
			// ��ʱ��������������
			m_connectionsMutex.unlock();
			success = SendData(item.connId, item.data);
			m_asyncSendMutex.lock();

			if (success)
			{
				successCount++;
			}
			else
			{
				// ����ʧ�ܣ�������Դ���
				if (item.retryCount < MAX_RETRY_COUNT)
				{
					item.retryCount++;
					retryQueue.push(item);
				}
				else
				{
					LOG_INFO(QString(u8"��Ϣ����ʧ�ܣ��Ѵﵽ������Դ���: ConnID=%1")
						.arg(item.connId));
				}
			}
		}
		// ��Ҫ���Ե���Ϣ���¼������
		while (!retryQueue.empty())
		{
			m_asyncSendQueue.push(retryQueue.front());
			retryQueue.pop();
		}
	}
	if (processCount > 0)
	{
		LOG_INFO(QString(u8"�첽���ʹ������: ����=%1��, �ɹ�=%2��, ʣ��=%3��")
			.arg(processCount).arg(successCount).arg(m_asyncSendQueue.size()));
	}
}
