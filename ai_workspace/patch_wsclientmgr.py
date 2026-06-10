import os

cpp_file = r"e:\__Code\__Work\hard2ser\hard2Ser_3_0\src\ipc\WSClientMgr.cpp"

replacement_content = """void WSClientMgrImpl::CacheMessage(const UnifiedMessage& msg)
{
	std::lock_guard<std::mutex> lock(m_cacheMtx);

	if (m_dbService)
	{
		nlohmann::json j;
		j["method"] = static_cast<int>(msg.method);
		if (msg.method == ConnectionType::WEBSOCKET)
		{
			j["wsMsg"] = nlohmann::json::parse(msg.wsMsg.toJson());
		}
		else if (msg.method == ConnectionType::HTTP)
		{
			nlohmann::json httpJ;
			httpJ["strUrl"] = msg.httpMsg.strUrl;
			httpJ["bAppendix"] = msg.httpMsg.bAppendix;
			httpJ["bSerialBody"] = msg.httpMsg.bSerialBody;
			
			nlohmann::json headersJ = nlohmann::json::object();
			for (const auto& pair : msg.httpMsg.headerInfo)
			{
				headersJ[pair.first] = pair.second;
			}
			httpJ["headerInfo"] = headersJ;
			
			nlohmann::json bodyJ = nlohmann::json::object();
			for (const auto& pair : msg.httpMsg.bodyInfo)
			{
				bodyJ[pair.first] = pair.second;
			}
			httpJ["bodyInfo"] = bodyJ;
			
			nlohmann::json urlDataJ = nlohmann::json::object();
			for (const auto& pair : msg.httpMsg.urlData)
			{
				urlDataJ[std::to_string(static_cast<int>(pair.first))] = pair.second;
			}
			httpJ["urlData"] = urlDataJ;
			
			j["httpMsg"] = httpJ;
		}

		m_dbService->EnqueueOfflineMessage("unified_message", j.dump());
		LOG_INFO(QString(u8"消息已缓存至本地数据库 SQLite, 消息类型: %1").arg(static_cast<int>(msg.method)));
	}
	else
	{
		if (m_cacheQue.size() >= MAX_CACHED_MESSAGES)
		{
			m_cacheQue.pop();
			LOG_INFO(QString(u8"内存缓存队列满，移除最老的消息"));
		}
		m_cacheQue.push(msg);
	}
}


void WSClientMgrImpl::ProcessCachedMessages()
{
	std::lock_guard<std::mutex> lock(m_cacheMtx);

	if (m_dbService)
	{
		auto list = m_dbService->DequeueOfflineMessages(50);
		if (list.empty())
		{
			return;
		}

		LOG_INFO(QString(u8"开始处理本地数据库离线缓存消息，条数: %1").arg(list.size()));

		for (const auto& item : list)
		{
			int queueId = std::get<0>(item);
			std::string payloadType = std::get<1>(item);
			std::string payloadData = std::get<2>(item);

			if (payloadType == "unified_message")
			{
				try
				{
					auto j = nlohmann::json::parse(payloadData);
					UnifiedMessage msg;
					msg.method = static_cast<ConnectionType>(j["method"].get<int>());

					if (msg.method == ConnectionType::WEBSOCKET)
					{
						WSMsgBase wsMsg;
						auto wsJ = j["wsMsg"];
						wsMsg.msgId = wsJ.value("msgId", "");
						wsMsg.msgType = wsJ.value("msgType", "");
						wsMsg.type = static_cast<MessageType>(wsJ.value("type", 0));
						wsMsg.payload = wsJ.value("payload", nlohmann::json::object());
						msg.wsMsg = wsMsg;
					}
					else if (msg.method == ConnectionType::HTTP)
					{
						HttpMsgBase httpMsg;
						auto httpJ = j["httpMsg"];
						httpMsg.strUrl = httpJ.value("strUrl", "");
						httpMsg.bAppendix = httpJ.value("bAppendix", false);
						httpMsg.bSerialBody = httpJ.value("bSerialBody", false);

						auto headersJ = httpJ["headerInfo"];
						for (auto it = headersJ.begin(); it != headersJ.end(); ++it)
						{
							httpMsg.headerInfo[it.key()] = it.value().get<std::string>();
						}

						auto bodyJ = httpJ["bodyInfo"];
						for (auto it = bodyJ.begin(); it != bodyJ.end(); ++it)
						{
							httpMsg.bodyInfo[it.key()] = it.value().get<std::string>();
						}

						auto urlDataJ = httpJ["urlData"];
						for (auto it = urlDataJ.begin(); it != urlDataJ.end(); ++it)
						{
							int urlTypeInt = std::stoi(it.key());
							httpMsg.urlData[static_cast<EURLType>(urlTypeInt)] = it.value().get<std::string>();
						}

						msg.httpMsg = httpMsg;
					}

					{
						std::lock_guard<std::mutex> sendLock(m_sendMtx);
						m_sendQue.push(msg);
					}
					m_sendCV.notify_one();

					m_dbService->DeleteOfflineMessage(queueId);
					LOG_INFO(QString(u8"从本地数据库读取并补发消息，ID: %1").arg(queueId));
				}
				catch (const std::exception& e)
				{
					LOG_INFO(QString(u8"解析离线缓存数据失败，删除该条记录. ID: %1, Error: %2")
						.arg(queueId)
						.arg(e.what()));
					m_dbService->DeleteOfflineMessage(queueId);
				}
			}
		}
	}
	else
	{
		while (!m_cacheQue.empty())
		{
			UnifiedMessage cacheMsg = m_cacheQue.front();
			m_cacheQue.pop();
			{
				std::lock_guard<std::mutex> sendLock(m_sendMtx);
				m_sendQue.push(cacheMsg);
			}
			m_sendCV.notify_one();
		}
	}
}

void WSClientMgrImpl::ClearCacheMessages()
{
	std::lock_guard<std::mutex> lock(m_cacheMtx);
	if (m_dbService)
	{
		auto list = m_dbService->DequeueOfflineMessages(1000);
		for (const auto& item : list)
		{
			m_dbService->DeleteOfflineMessage(std::get<0>(item));
		}
	}
	std::queue<UnifiedMessage> emptyQue;
	m_cacheQue.swap(emptyQue);
}

size_t WSClientMgrImpl::GetCachedMessagesSize()
{
	std::lock_guard<std::mutex> lock(m_cacheMtx);
	if (m_dbService)
	{
		auto list = m_dbService->DequeueOfflineMessages(1000);
		return list.size();
	}
	return m_cacheQue.size();
}

std::queue<UnifiedMessage> WSClientMgrImpl::GetCachedMessages()
{
	std::lock_guard<std::mutex> lock(m_cacheMtx);
	if (m_dbService)
	{
		std::queue<UnifiedMessage> retQue;
		auto list = m_dbService->DequeueOfflineMessages(1000);
		for (const auto& item : list)
		{
			std::string payloadData = std::get<2>(item);
			try
			{
				auto j = nlohmann::json::parse(payloadData);
				UnifiedMessage msg;
				msg.method = static_cast<ConnectionType>(j["method"].get<int>());

				if (msg.method == ConnectionType::WEBSOCKET)
				{
					WSMsgBase wsMsg;
					auto wsJ = j["wsMsg"];
					wsMsg.msgId = wsJ.value("msgId", "");
					wsMsg.msgType = wsJ.value("msgType", "");
					wsMsg.type = static_cast<MessageType>(wsJ.value("type", 0));
					wsMsg.payload = wsJ.value("payload", nlohmann::json::object());
					msg.wsMsg = wsMsg;
				}
				else if (msg.method == ConnectionType::HTTP)
				{
					HttpMsgBase httpMsg;
					auto httpJ = j["httpMsg"];
					httpMsg.strUrl = httpJ.value("strUrl", "");
					httpMsg.bAppendix = httpJ.value("bAppendix", false);
					httpMsg.bSerialBody = httpJ.value("bSerialBody", false);

					auto headersJ = httpJ["headerInfo"];
					for (auto it = headersJ.begin(); it != headersJ.end(); ++it)
					{
						httpMsg.headerInfo[it.key()] = it.value().get<std::string>();
					}

					auto bodyJ = httpJ["bodyInfo"];
					for (auto it = bodyJ.begin(); it != bodyJ.end(); ++it)
					{
						httpMsg.bodyInfo[it.key()] = it.value().get<std::string>();
					}

					auto urlDataJ = httpJ["urlData"];
					for (auto it = urlDataJ.begin(); it != urlDataJ.end(); ++it)
					{
						int urlTypeInt = std::stoi(it.key());
						httpMsg.urlData[static_cast<EURLType>(urlTypeInt)] = it.value().get<std::string>();
					}

					msg.httpMsg = httpMsg;
				}
				retQue.push(msg);
			}
			catch (...)
			{
			}
		}
		return retQue;
	}
	return m_cacheQue;
}
"""

raw_bytes = b""
with open(cpp_file, 'rb') as f:
    raw_bytes = f.read()

# 尝试 UTF-8 结构
try:
    content = raw_bytes.decode('utf-8-sig')
    print("Decoded with utf-8-sig")
except UnicodeDecodeError:
    # 失败退回 gb18030
    content = raw_bytes.decode('gb18030')
    print("Decoded with gb18030")

target_str = "void WSClientMgrImpl::CacheMessage(const UnifiedMessage& msg)"
idx = content.find(target_str)
if idx == -1:
    print("Could not find target function in WSClientMgr.cpp")
else:
    new_content = content[:idx] + replacement_content
    with open(cpp_file, 'w', encoding='utf-8-sig', newline='') as f:
        f.write(new_content)
    print("Successfully patched and saved WSClientMgr.cpp with UTF-8 BOM")
