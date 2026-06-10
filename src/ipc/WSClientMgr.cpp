#include "WSClientMgr.h"
#include "global.h"
#include "MessageDefine.h"
#include "HttpRepParser.h"
#include "SingleOSSToken.h"

//----------------------------WSClientMgr----------------------------------------------
//----------------------------WSClientMgr----------------------------------------------
//----------------------------WSClientMgr----------------------------------------------


WSClientMgr::WSClientMgr()
	: m_pImpl(std::make_unique<WSClientMgrImpl>())
{

}

WSClientMgr::~WSClientMgr() = default;


void WSClientMgr::Connect2Ser(const std::string& wsUrl, const std::string& httpbaseUrl)
{
	m_pImpl->Connect2Ser(wsUrl, httpbaseUrl);
}

void WSClientMgr::DisconnectFromSer(bool bBtnClick /*=false*/)
{
	m_pImpl->DisconnectFromSer(bBtnClick);
}


void WSClientMgr::SendHTTPMsg(const HttpMsgBase& msg)
{
	m_pImpl->SendHTTPMsg(msg);
}

void WSClientMgr::SendWSMsg(const WSMsgBase& msg)
{
	m_pImpl->SendWSMsg(msg);
}

void WSClientMgr::SendUnifiedMsg(const UnifiedMessage& msg)
{
	m_pImpl->SendUnifiedMsg(msg);
}

bool WSClientMgr::IsConnected() const
{
	return m_pImpl->IsConnected();
}

bool WSClientMgr::IsHttpCfg() const
{
	return m_pImpl->IsConnected();
}

WSClientSig* WSClientMgr::GetSignals()
{
	return m_pImpl->GetSignals();
}


HttpMsgBase WSClientMgr::CreateHttpMessage(MessageType type, const std::string& payload, const std::vector<DeviceInfo>& devVec/* ={}*/)
{
	return m_pImpl->CreateHttpMessage(type, payload, devVec);

	//if (devVec.size())
	//{
	//}
	//else
	//{
	//	return m_pImpl->CreateHttpMessage(type, payload, std::vector<DeviceInfo>());

	//}
}

//----------------------------WSClientMgrImpl----------------------------------------------
//----------------------------WSClientMgrImpl----------------------------------------------
//----------------------------WSClientMgrImpl----------------------------------------------

WSClientMgrImpl::WSClientMgrImpl()
	: m_wsClientWrap(std::make_unique<WSClientWrapper>())
	, m_httpClientWrap(std::make_unique<HttpClientWrapper>())
	, m_bConnect(false)
	, m_bHttpCfg(false)
	, m_sendThdRunning(false)
	, m_heartbeatThdRunning(false)
	, m_wsReconnectRunning(false)
	, m_wsReconnectCount(0)
	, m_needHttpReconnect(false)
	, m_wsStatus(NetworkStatus::DISCONNECTED)
	, m_httpStatus(NetworkStatus::DISCONNECTED)
{
	//auto self = shared_from_this();

	m_wsClientWrap->SetOnOpenCallback([this]()
	{
		OnWebSocketOpen();
	});

	m_wsClientWrap->SetOnMessageCallback([this](const std::string& msg)
	{
		OnWebSocketMessage(msg);
	});

	m_wsClientWrap->SetOnCloseCallback([this]()
	{
		OnWebSocketClose();
	});

	m_wsClientWrap->SetOnErrorCallback([this](int err, const std::string& msg)
	{
		OnWebSocketError(err, msg);
	});

	//m_httpClientWrap->SetOnErrCallback([this](int err, const std::string& msg) {
	//	OnHttpError(err, msg);
	//});

	m_sendThdRunning = true;
	m_sendThd = std::thread(&WSClientMgrImpl::SendThreadFunction, this);

	//Note:启动心跳发送
	LOG_INFO(QString::fromLocal8Bit("net_mgr_moudle WSClientMgrImpl_fun 启动心跳发送报文"));
	StartHeartbeat();
}

WSClientMgrImpl::~WSClientMgrImpl()
{
	DisconnectFromSer();
	// 停止重连线程
	StopWSReconnect();
	//StopHTTPReconnect();
	// 停止发送线程
	m_sendThdRunning = false;
	m_sendCV.notify_one(); // 唤醒发送线程，使其退出
	if (m_sendThd.joinable()) 
	{
		m_sendThd.join();
	}
	// 停止心跳线程
	StopHeartbeat();
}

void WSClientMgrImpl::Connect2Ser(const std::string& wsUrl, const std::string& httpBaseUrl)
{
	if (!m_bConnect)
	{
		m_wsSerUrl = wsUrl;
		m_httpBaseUrl = httpBaseUrl;

		if (!GetIsFriLoginStatus())
		{
			// 停止任何正在进行的重连
			StopWSReconnect();
		}

		// 更新连接状态
		m_wsStatus = NetworkStatus::CONNECTING;
		emit m_wsSignals.SigNetworkStatusChanged(ConnectionType::WEBSOCKET, NetworkStatus::CONNECTING, u8"开始连接WebSocket");


		//配置HTTP
		if (!m_httpBaseUrl.empty())
		{
			m_httpClientWrap->SetBaseUrl(m_httpBaseUrl);
			m_httpClientWrap->SetTimeout(10000);
			m_httpClientWrap->AddHeader("Content-Type", "application.json");
			m_bHttpCfg = true;
			LOG_INFO(QString("ipc_moudle http_client_cfg_base_url: %1").arg(m_httpBaseUrl.c_str()));
		}
		// 连接WebSocket
		m_wsClientWrap->Connect(m_wsSerUrl);
	}
	//判断是否为第一次启动，是则取反，否则保持原样
	g_friLogin = g_friLogin ? !g_friLogin : g_friLogin;
}

void WSClientMgrImpl::DisconnectFromSer(bool bBtnClick /*=false*/)
{
	//判断是否为按钮触发CloseNet
	g_closeNetBtnClick = bBtnClick;
	if (m_bConnect)
	{
		m_wsClientWrap->Disconnect();
		m_bConnect = false;
		emit m_wsSignals.SigNetworkStatusChanged(ConnectionType::WEBSOCKET, NetworkStatus::DISCONNECTED, u8"断开WebSocket连接");
	}
	m_bHttpCfg = false;
}

void WSClientMgrImpl::SendHTTPMsg(const HttpMsgBase& msg)
{
	std::lock_guard<std::mutex> lock(m_sendMtx);
	UnifiedMessage httpMsg(msg);
	m_sendQue.push(httpMsg);
	m_sendCV.notify_one();
}

void WSClientMgrImpl::SendWSMsg(const WSMsgBase& msg)
{
	std::lock_guard<std::mutex> lock(m_sendMtx);
	UnifiedMessage wsMsg(msg);
	m_sendQue.push(wsMsg);
	m_sendCV.notify_one();
}

void WSClientMgrImpl::SendUnifiedMsg(const UnifiedMessage& msg)
{
	std::lock_guard<std::mutex> lock(m_sendMtx);
	m_sendQue.push(msg);
	m_sendCV.notify_one();
}

bool WSClientMgrImpl::IsConnected() const
{
	return m_bConnect;
}

bool WSClientMgrImpl::IsHttpCfg() const
{
	return m_bHttpCfg;
}

void WSClientMgrImpl::StartHeartbeat()
{
	m_heartbeatThdRunning = true;
	m_heartbeatThd = std::thread([this]() 
	{
		while (m_heartbeatThdRunning) 
		{
			std::this_thread::sleep_for(std::chrono::seconds(5)); // 每5秒发送一次心跳
			if (m_heartbeatThdRunning && m_bConnect &&  m_wsClientWrap->IsConnected())
			{
				try 
				{
					//WebSocketMessage heartbeat_msg;
					//heartbeat_msg.type = MessageType::CLIENT_HEARTBEAT;
					//heartbeat_msg.payload = "{}"; // 心跳报文可以为空payload
					//SendData(heartbeat_msg);

					WSMsgBase msg;
					msg.msgId = "123456";
					msg.msgType = "heartbeat";
					nlohmann::json j;
					j["data"] = "ping";
					msg.payload = j;
					SendWSMsg(msg);
					LOG_INFO("Sent heartbeat.");
				}
				catch (const std::exception& e) 
				{
					LOG_INFO(QString("Failed to send heartbeat: %1").arg(e.what()));
				}
			}
		}
	});
}

void WSClientMgrImpl::StopHeartbeat()
{
	m_heartbeatThdRunning = false;
	if (m_heartbeatThd.joinable()) 
	{
		m_heartbeatThd.join();
	}
}

void WSClientMgrImpl::SendThreadFunction()
{
	while (m_sendThdRunning) 
	{
		std::unique_lock<std::mutex> lock(m_sendMtx);
		m_sendCV.wait(lock, [this] { return !m_sendQue.empty() || !m_sendThdRunning; });

		if (!m_sendThdRunning && m_sendQue.empty()) 
		{
			break;
		}

		if (!m_sendQue.empty()) 
		{
			UnifiedMessage msg_to_send = m_sendQue.front();
			m_sendQue.pop();
			lock.unlock(); // 提前解锁，避免 send 阻塞其他线程


			//ConnectionType method = GetCommunicationMethod(msg_to_send.type);
			if (msg_to_send.method == ConnectionType::WEBSOCKET)
			{
				SendViaWS(msg_to_send.wsMsg);
			}
			else if (msg_to_send.method == ConnectionType::HTTP)
			{
				SnedViaHttp(msg_to_send.httpMsg);
			}
			else
			{
				LOG_INFO(QString("ipc_moudle unknown communication method for msg type:%d").arg(static_cast<int>(msg_to_send.method)));
			}
		}
	}
	LOG_INFO("Send thread stopped.");
}

void WSClientMgrImpl::SendViaWS(const WSMsgBase& msg)
{
	if (m_bConnect && m_wsClientWrap->IsConnected())
	{
		try
		{
			std::string jsonMsg = msg.toJson();
			//--
			if (msg.msgType == "sync_process")
			{
				int a = 1;
			}
			//--
			m_wsClientWrap->Send(jsonMsg);
			LOG_INFO(QString("ipc_moudle send_via_ws msg_info= %1").arg(jsonMsg.c_str()));
		}
		catch (const std::exception& e)
		{
			LOG_INFO(QString("ipc_moudle send_via_ws failed_2_send_ws_msg = %1").arg(e.what()));

			// 发送失败时缓存消息
			UnifiedMessage unifiedMsg(msg);
			CacheMessage(unifiedMsg);

			// 连接可能已断开，更新状态
			m_bConnect = false;
			emit m_wsSignals.SigNetworkStatusChanged(ConnectionType::WEBSOCKET, NetworkStatus::DISCONNECTED, u8"主动断开WebSocket连接");
		}
	}
	else
	{
		// 网络未连接时缓存消息
		UnifiedMessage unifiedMsg(msg);
		CacheMessage(unifiedMsg);
		LOG_INFO(QString("ipc_moudle send_via_ws ws_not_connected, msg_no_send = %1").arg(msg.toJson().c_str()));
	}
}

void WSClientMgrImpl::SnedViaHttp(const HttpMsgBase& msg)
{
	if (!m_bHttpCfg)
	{
		// HTTP未配置时缓存消息
		UnifiedMessage unifiedMsg(msg);
		CacheMessage(unifiedMsg);

		LOG_INFO(QString("ipc_moudle send_via_http_fun http_not_cfg, msg_cached"));
		return;
	}

	try
	{
		//ori
		//std::string path = GetHttpPath(msg.type);
		//std::string jsonMsg = msg.toJson();
		//new
		std::string path = msg.BuildFullUrl();
		std::string jsonMsg = msg.ToHttpBody();


		//设置http headers
		auto headers = msg.GetHttpHeaders();
		if (!headers.empty())
		{
			m_httpClientWrap->SetHeaders(headers);
			LOG_INFO(QString("Set HTTP headers, count: %1").arg(headers.size()));
		}


		//post的回调函数
		//Note: 0915_test_http_fun sync_post发送
		//Note: 0924 todo: this-> shared_from_this
		//auto a = m_httpClientWrap->Post(path, jsonMsg);
		//if (MessageType::CLIENT_OSS_TOKEN != ParseHttpRep(msg))
		//{

		//}
		//else
		//{
		//m_httpClientWrap->GetAsync(path, [this, msg](const HttpRep& rep)
		//{
		//	if (rep.statusCode >= 200 && rep.statusCode < 300)
		//	{
		//		LOG_INFO(QString("HTTP message sent successfully, status: %1").arg(rep.statusCode));
		//		// 如果响应包含数据，处理响应消息
		//		if (!rep.body.empty())
		//		{
		//			//TODO: 解析msg中API接口，根据相关信息获取更新数据文件，并进行后期处理
		//			auto reqType = ParseHttpRep(msg);
		//			switch (reqType)
		//			{

		//			case MessageType::CLIENT_OSS_TOKEN:
		//			{
		//				auto response = BaseSerResp::CreateResponse(rep.body);
		//				if (response->IsSuccess())
		//				{
		//					auto ret = true;
		//					//解析oss_token_info数据
		//					OssTokenResp* uResponse = dynamic_cast<OssTokenResp*>(response.get());
		//					m_ossToken = uResponse->ossParam;
		//					//获取到login_token后，进行设备数据获取工作
		//				}

		//				////json_body_parse
		//				//WSMsgBase msg = WSMsgBase::fromJson(rep.body);
		//				//// 发送信号，通知已完成oss_token数据获取
		//				//emit m_wsSignals.SigMsgRecevied(QString::number(static_cast<int>(msg.type)), msg);
		//				LOG_INFO(QString("cur_http_ipc_recv_body, http_type = CLIENT_HTTP_CONN, body  = %1").arg(QString::fromStdString(rep.body)));
		//				break;
		//			}


		//			case MessageType::UNKNOWN:
		//			default:
		//				break;
		//			}
		//		}
		//		else
		//		{
		//			LOG_INFO(QString("ipc_moudule send_via_http http_request_failed, status_%1, body_%2").arg(rep.statusCode).arg(rep.body.c_str()));
		//			// HTTP发送失败时缓存消息进行重试
		//			UnifiedMessage unifiedMsg(msg);
		//			CacheMessage(unifiedMsg);
		//		}
		//	}
		//});

		//}
		m_httpClientWrap->PostAsync(path, jsonMsg, [this, msg](const HttpRep& rep)
		{
			if (rep.statusCode >= 200 && rep.statusCode < 300)
			{
				LOG_INFO(QString("HTTP message sent successfully, status: %1").arg(rep.statusCode));
				// 如果响应包含数据，处理响应消息
				if (!rep.body.empty())
				{
					//TODO: 解析msg中API接口，根据相关信息获取更新数据文件，并进行后期处理
					auto reqType = ParseHttpRep(msg);
					switch (reqType)
					{
					case MessageType::CLIENT_HTTP_CONN:
					{
						auto response = BaseSerResp::CreateResponse(rep.body);
						if (response->IsSuccess())
						{
							auto ret = true;
							//解析login_token_info数据
							UserResp* uResponse = dynamic_cast<UserResp*>(response.get());
							//m_loginParam = uResponse->userParam;
							m_logintoken = uResponse->token;
							////获取到login_token后，进行设备数据获取工作
							auto sendDevMsg = CreateHttpMessage(MessageType::CLIENT_GET_DEV_INFO, "http://shop.moonbii.net");
							SendHTTPMsg(sendDevMsg);


							//m_bLoginSer = true;
							//emit SigSendReplyInfo(QString::fromStdString(resp));
							//printLog(QString::fromLocal8Bit("登录成功,返回：") + QString::fromStdString(resp));
						}

						////json_body_parse
						//WSMsgBase msg = WSMsgBase::fromJson(rep.body);
						////发送信号发出响应消息
						//emit m_wsSignals.SigMsgRecevied(QString::number(static_cast<int>(msg.type)), msg);
						LOG_INFO(QString("cur_http_ipc_recv_body, http_type = CLIENT_HTTP_CONN, body  = %1").arg(QString::fromStdString(rep.body)));


						//msg.payload = j.value("payload", nlohmann::json{});
						//return msg;
						break;
					}
					case MessageType::CLIENT_LOGIN_TOKEN:

						break;
					case MessageType::CLIENT_OSS_TOKEN:
					{
						auto response = BaseSerResp::CreateResponse(rep.body);
						if (response->IsSuccess())
						{
							auto ret = true;
							//解析oss_token_info数据
							OssTokenResp* uResponse = dynamic_cast<OssTokenResp*>(response.get());
							m_ossToken = uResponse->ossParam;
							//获取到login_token后，进行设备数据获取工作
							SingleOSSToken::GetInstance().SetOSSParam(m_ossToken);
							SingleOSSToken::GetInstance().InitConnect();
						}

						////json_body_parse
						//WSMsgBase msg = WSMsgBase::fromJson(rep.body);
						//// 发送信号，通知已完成oss_token数据获取
						//emit m_wsSignals.SigMsgRecevied(QString::number(static_cast<int>(msg.type)), msg);
						LOG_INFO(QString("cur_http_ipc_recv_body, http_type = CLIENT_HTTP_CONN, body  = %1").arg(QString::fromStdString(rep.body)));
						break;
					}
					case MessageType::CLIENT_UPLOAD_OSS_FILE:

						break;
					case MessageType::CLIENT_DOWNLOAD_OSS_FILE:

						break;
					case MessageType::CLIENT_GET_ORDERS_INFO:

						break;
					case MessageType::CLIENT_GET_USER_INFO:

						break;
					case MessageType::CLIENT_GET_DEV_INFO:
					{
						auto response = BaseSerResp::CreateResponse(rep.body);
						if (response->IsSuccess())
						{
							auto ret = true;
							//解析login_token_info数据
							DevBindResp* uResponse = dynamic_cast<DevBindResp*>(response.get());
							//设备注册处理
							emit m_wsSignals.SigBindRegDev(*uResponse);

							//m_bLoginSer = true;
							//emit SigSendReplyInfo(QString::fromStdString(resp));
							//printLog(QString::fromLocal8Bit("登录成功,返回：") + QString::fromStdString(resp));
						}

						////json_body_parse
						//WSMsgBase msg = WSMsgBase::fromJson(rep.body);
						////发送信号发出响应消息
						//emit m_wsSignals.SigMsgRecevied(QString::number(static_cast<int>(msg.type)), msg);
						LOG_INFO(QString("cur_http_ipc_recv_body, http_type = CLIENT_GET_DEV_INFO, body = %1").arg(QString::fromStdString(rep.body.c_str()).data()));
						break;
					}
					case MessageType::CLIENT_GET_REG_RUN_DEV_INFO:
					{
						auto response = BaseSerResp::CreateResponse(rep.body);
						if (response->IsSuccess())
						{
							//发送信号，进行数据剥离，将设备设置为空闲状态
							auto ret = true;
							//解析login_token_info数据
							DevSereMapResp* uResponse = dynamic_cast<DevSereMapResp*>(response.get());
							//设备注册处理
							emit m_wsSignals.SigRegDevTaskSync(*uResponse);
						}
						LOG_INFO(QString("cur_http_ipc_recv_body, http_type = CLIENT_GET_REG_RUN_DEV_INFO, body = %1").arg(QString::fromStdString(rep.body)));
						break;
					}
					case MessageType::UNKNOWN:
					default:
						break;
					}
					//auto a = rep.body;
					//try
					//{
					//	WSMsgBase msg = WSMsgBase::fromJson(rep.body);
					//	//发送信号发出响应消息
					//	emit m_wsSignals.SigMsgRecevied(QString::number(static_cast<int>(msg.type)), msg);
					//	LOG_INFO(QString("%1").arg(QString::fromStdString(rep.body)));
					//}
					//catch (const std::exception& e)
					//{
					//	LOG_INFO(QString("ipc_moudule send_via_http failed_2_http_response_%1").arg(e.what()));
					//}
				}
				else
				{
					LOG_INFO(QString("ipc_moudule send_via_http http_request_failed, status_%1, body_%2").arg(rep.statusCode).arg(rep.body.c_str()));
					// HTTP发送失败时缓存消息进行重试
					UnifiedMessage unifiedMsg(msg);
					CacheMessage(unifiedMsg);
				}
			}
		});

		LOG_INFO(QString("ipc_moudule send_via_http send_http_msg_%1, %2").arg(path.c_str()).arg(jsonMsg.c_str()));
	}
	catch (const std::exception& e)
	{

		// 发送异常时缓存消息
		UnifiedMessage unifiedMsg(msg);
		CacheMessage(unifiedMsg);
		LOG_INFO(QString("ipc_moudule send_via_http failed_2_send_http_msg = %1").arg(e.what()));
	}
}



MessageType WSClientMgrImpl::ParseHttpRep(const HttpMsgBase& msg)
{
	auto ret = MessageType::UNKNOWN;
	auto apiPath = msg.urlData.find(EURLType::API_PATH);
	ret = GetHttpPathType(apiPath->second);
	return ret;
}

WSMsgBase WSClientMgrImpl::ConvertToWSMsg(const WebSocketMessage& old_msg)
{
	return WSMsgBase();
}


HttpMsgBase WSClientMgrImpl::CreateHttpMessage(MessageType type, const std::string& baseURL, const std::vector<DeviceInfo>& devVec /*={}*/)
{
	HttpMsgBase httpMsg;
	httpMsg.strUrl = m_httpBaseUrl;
	httpMsg.bAppendix = false;
	httpMsg.bSerialBody = false;

	// 设置API路径
	httpMsg.urlData[EURLType::API_PATH] = GetHttpPath(type);

	//HTTP 报文数据设置
	switch (type)
	{

		case MessageType::CLIENT_HTTP_CONN:
		{
			httpMsg.headerInfo["Content-Type"] = "application/json";
			httpMsg.bodyInfo["phone_number"] = "15557102422";
			httpMsg.bodyInfo["phone_code"] = "123456";
			break;
		}
		case MessageType::CLIENT_LOGIN_TOKEN:
			break;
		case MessageType::CLIENT_OSS_TOKEN:
		{
			//设置bucket数据
			httpMsg.bodyInfo["biz_scene"] = "hand";
			//httpMsg.headerInfo["Content-Type"] = "application/json";
			////判断当前login是否失效，失效则更新token
			//if (!m_logintoken.empty())
			//{
			//	httpMsg.headerInfo["token"] = m_logintoken;
			//}
			//else
			//{
			//	auto sendMsg = CreateHttpMessage(MessageType::CLIENT_HTTP_CONN, "http://shop.moonbii.net", {});
			//	SendHTTPMsg(sendMsg);
			//	//设置一个定时器延时，再进行token查询数据
			//	// lam处理
			//	if (!m_logintoken.empty())
			//	{
			//		httpMsg.headerInfo["token"] = m_logintoken;
			//	}
			//}
			break;
		}
		case MessageType::CLIENT_UPLOAD_OSS_FILE:
			break;
		case MessageType::CLIENT_DOWNLOAD_OSS_FILE:
			break;
		case MessageType::CLIENT_GET_ORDERS_INFO:
			break;
		case MessageType::CLIENT_GET_USER_INFO:
			break;
		//工控启动时，获取所有在服务器配置的硬件设备
		case MessageType::CLIENT_GET_DEV_INFO:
		{
			if (!m_logintoken.empty())
			{
				httpMsg.headerInfo["Content-Type"] = "application/json";
				httpMsg.headerInfo["token"] = m_logintoken;
			}
			break;
		}
		//工控启动时，获取服务器中处于运行中的硬件设备
		case MessageType::CLIENT_GET_REG_RUN_DEV_INFO:
		{
			//// 构建请求JSON
			//nlohmann::json reqBody;
			//nlohmann::json devArr = nlohmann::json::array();
			//for (const auto& dev : devVec) 
			//{
			//	devArr.push_back(dev.devId);
			//}
			//httpMsg.headerInfo["Content-Type"] = "application/json";
			//httpMsg.bodyInfo["device_ids"] = devArr.dump();
			std::string body;
			auto devData = devVec;
			for (const auto& dev : devData)
			{
				body.append(dev.devId + ",");
			}
			body.pop_back();
			httpMsg.bodyInfo["device_ids"] = body;

			break;
		}
		default:
			break;
	}
	return httpMsg;
}



void WSClientMgrImpl::OnWebSocketOpen()
{
	m_bConnect = true;
	//LOG_INFO("WebSocket connected to %s", m_wsSerUrl.c_str());
	// if (connection_status_callback_) {
	//     connection_status_callback_(true);
	// }
	m_wsStatus = NetworkStatus::CONNECTED;
	m_httpStatus = NetworkStatus::CONNECTED; // HTTP也标记为已连接
	LOG_INFO(QString("WebSocket connected to %1").arg(m_wsSerUrl.c_str()));

	emit m_wsSignals.SigNetworkStatusChanged(ConnectionType::WEBSOCKET, NetworkStatus::CONNECTED, u8"WebSocket连接成功");
	emit m_wsSignals.SigNetworkStatusChanged(ConnectionType::HTTP, NetworkStatus::CONNECTED, u8"HTTP连接准备就绪");

	//WebSocket连接成功后处理缓存消息
	ProcessCachedMessages();
}

//WS_Recv
void WSClientMgrImpl::OnWebSocketMessage(const std::string& msg)
{
	//LOG_INFO("Received message: %s", msg.c_str());
	try 
	{
		WSMsgBase wsMsg = WSMsgBase::fromJson(msg);
		// auto it = message_callbacks_.find(wsMsg.type);
		// if (it != message_callbacks_.end()) {
		//     it->second(wsMsg);
		// }
		//WS_心跳报文_rep
		if ( wsMsg.msgType == "heartbeat" || wsMsg.type == MessageType::CLIENT_HEARTBEAT)
		{
			auto str = wsMsg.payload.dump();
			//Todo: 解析payload
			emit m_wsSignals.SigHeartBeatAckReceived();
		}
		//WS_进程任务下发_req
		else if(wsMsg.msgType == "device_task_op" && wsMsg.type == MessageType::SER_DISPATCH_TASK)
		{
			//Todo: 解析payload
			auto str = wsMsg.payload.dump();
			LOG_INFO(QString("lrz_dev_task_recv_data = %1").arg(QString::fromStdString(str).data()));
			emit m_wsSignals.SigMsgRecevied(QString::number(static_cast<int>(wsMsg.type)), wsMsg);
		}
		//WS_设备状态同步_req
		else if(wsMsg.msgType == "sync_device" || wsMsg.type == MessageType::CLIENT_SYNC_DEV_STATUS)
		{
			//Todo: task_dispatch
			//parse_payload
			auto str = wsMsg.payload.dump();
			emit m_wsSignals.SigMsgRecevied(QString::number(static_cast<int>(wsMsg.type)), wsMsg);
		}
		//WS_进程状态同步报文_rep
		else if(wsMsg.msgType == "sync_process" || wsMsg.type == MessageType::CLIENT_SYNC_TASK_STATUS)
		{
			//Todo: task_dispatch
			//parse_payload
			auto str = wsMsg.payload.dump();
			LOG_INFO(QString("tmp_log_lrz ser_dispatch_sync_task:  %1").arg(QString::fromStdString(str)));
			emit m_wsSignals.SigMsgRecevied(QString::number(static_cast<int>(wsMsg.type)), wsMsg);
		}
		else
		{
			//Todo: task_dispatch
			//parse_payload
			auto str = wsMsg.payload.dump();
			emit m_wsSignals.SigMsgRecevied(QString::number(static_cast<int>(wsMsg.type)), wsMsg);
		}
	}
	catch (const nlohmann::json::exception& e) 
	{
		LOG_INFO(QString("JSON parsing error: %1, message: %1").arg(e.what()).arg(msg.c_str()));
	}
	catch (const std::exception& e) 
	{
		LOG_INFO(QString("Error processing message: %1, message: %1").arg(e.what()).arg(msg.c_str()));
	}
}

void WSClientMgrImpl::OnWebSocketClose()
{
	m_bConnect = false;
	m_wsStatus = NetworkStatus::DISCONNECTED;
	LOG_INFO("WebSocket disconnected.");
	// if (connection_status_callback_) 
	// {
	//     connection_status_callback_(false);
	// }
	emit m_wsSignals.SigNetworkStatusChanged(ConnectionType::WEBSOCKET, NetworkStatus::DISCONNECTED, u8"WebSocket连接断开");

	// 触发WebSocket断连处理逻辑
	if (ShouldAutoReconnect())
	{
		HandleWSDisconnection();
	}
	else
	{
		LOG_INFO(u8"WebSocket断连，但因用户手动关闭网络或其他原因，跳过自动重连");
	}

}

void WSClientMgrImpl::OnWebSocketError(int err, const std::string& msg)
{
	m_bConnect = false;
	m_wsStatus = NetworkStatus::DISCONNECTED;
	LOG_INFO(QString("WebSocket error: %1, %2").arg(err).arg(msg.c_str()));
	// if (connection_status_callback_) {
	//     connection_status_callback_(false);
	// }
	emit m_wsSignals.SigNetworkStatusChanged(ConnectionType::WEBSOCKET, NetworkStatus::FAILED, QString(u8"WebSocket错误: %1").arg(msg.c_str()));

	// 触发WebSocket断连处理逻辑
	if (ShouldAutoReconnect())
	{
		HandleWSDisconnection();
	}
	else
	{
		LOG_INFO(QString(u8"WebSocket错误(%1)，但因用户手动关闭网络或其他原因，跳过自动重连").arg(err));

	}
}

void WSClientMgrImpl::OnHttpError(int err, const std::string& msg)
{
	LOG_INFO(QString(u8"HTTP client error: %1, %2").arg(err).arg(msg.c_str()));
	// HTTP错误不影响WebSocket连接状态，但可以通过信号通知UI
	// 可以考虑添加专门的HTTP错误信号

	m_httpStatus = NetworkStatus::FAILED;
	emit m_wsSignals.SigNetworkStatusChanged(ConnectionType::HTTP, NetworkStatus::FAILED, QString(u8"HTTP错误: %1").arg(msg.c_str()));
}

void WSClientMgrImpl::StopWSReconnect()
{
	m_wsReconnectRunning = false;
	if (m_wsReconnectThd.joinable()) 
	{
		m_wsReconnectThd.join();
	}
	// 重置线程对象为默认状态
	m_wsReconnectThd = std::thread();
}


void WSClientMgrImpl::AttemptWSReconnect()
{
	m_wsReconnectCount++;
	emit m_wsSignals.SigReconnectAttempt(ConnectionType::WEBSOCKET, m_wsReconnectCount, MAX_WS_RECONNECT_ATTEMPTS);

	LOG_INFO(QString(u8"尝试WebSocket重连, 第 %1/%2 次").arg(m_wsReconnectCount).arg(MAX_WS_RECONNECT_ATTEMPTS));

	try
	{
		m_wsClientWrap->Connect(m_wsSerUrl);
		// 等待连接结果 (简单的延时检查)
		std::this_thread::sleep_for(std::chrono::milliseconds(2000));
		if (m_wsClientWrap->IsConnected())
		{
			OnWSReconnectSuccess();
		}
	}
	catch (const std::exception& e)
	{
		LOG_INFO(QString(u8"WebSocket重连失败: %1").arg(e.what()));
	}
}

void WSClientMgrImpl::OnWSReconnectSuccess()
{
	LOG_INFO(u8"WebSocket重连成功");

	m_bConnect = true;
	m_wsStatus = NetworkStatus::CONNECTED;
	emit m_wsSignals.SigNetworkStatusChanged(ConnectionType::WEBSOCKET, NetworkStatus::CONNECTED, u8"WebSocket重连成功");

	// 设置重连停止标志，让重连线程自然结束
	m_wsReconnectRunning = false;

	// WebSocket重连成功后，重新连接HTTP
	if (m_needHttpReconnect)
	{
		m_httpClientWrap->SetBaseUrl(m_httpBaseUrl);
		m_httpClientWrap->SetTimeout(5000);
		m_httpClientWrap->AddHeader("Content-Type", "application/json");

		// 通过ping接口测试HTTP连接是否正常
		if (TestHTTPConnectionWithPing())
		{
			m_bHttpCfg = true;
			m_httpStatus = NetworkStatus::CONNECTED;
			m_needHttpReconnect = false;
			emit m_wsSignals.SigNetworkStatusChanged(ConnectionType::HTTP, NetworkStatus::CONNECTED, u8"HTTP重连成功");
		}
		//清空ws重连次数
		m_wsReconnectCount = 0;
	}
	//处理ws的重连逻辑
	//StopWSReconnect();
}


void WSClientMgrImpl::HandleWSDisconnection()
{
	LOG_INFO(QString(u8"处理WebSocket断连"));

	// 防止重复重连
	if (m_wsReconnectRunning) 
	{
		LOG_INFO(QString(u8"WebSocket重连已在进行中，跳过重复处理"));
		return;
	}

	// 1. 判断HTTP连接状态（简化检查，避免网络请求）
	bool httpConnected = m_bHttpCfg && (m_httpStatus == NetworkStatus::CONNECTED);
	LOG_INFO(QString(u8"WebSocket断连时HTTP连接状态: %1").arg(httpConnected ? u8"连接正常" : u8"连接断开"));

	// 2. 处理HTTP连接
	if (httpConnected) 
	{
		// HTTP仍处于连接状态，先断开HTTP连接
		LOG_INFO(QString(u8"HTTP连接正常，主动断开HTTP连接"));
		m_bHttpCfg = false;
		m_httpStatus = NetworkStatus::DISCONNECTED;
		emit m_wsSignals.SigNetworkStatusChanged(ConnectionType::HTTP, NetworkStatus::DISCONNECTED, u8"因WebSocket断连而主动断开HTTP");
	}

	// 3. 暂停定时器--在断网消息断开注册设备信息，在完成ongoing_server接口后重启定时器
	// 4. 标记HTTP需要重连并启动WebSocket重连
	m_needHttpReconnect = true;
	StartWSReconnectInternal();

}


void WSClientMgrImpl::StartWSReconnectInternal()
{
	LOG_INFO(QString("启动WebSocket内部重连（跳过状态检查）"));

	// 停止任何正在进行的重连线程
	if (m_wsReconnectRunning || m_wsReconnectThd.joinable())
	{
		StopWSReconnect();
	}

	m_wsReconnectCount = 0;
	m_wsReconnectRunning = true;
	m_wsStatus = NetworkStatus::RECONNECTING;
	//设置网络重连状态为真，默认为假
	g_netReconnectStatus = true;

	emit m_wsSignals.SigNetworkStatusChanged(ConnectionType::WEBSOCKET, NetworkStatus::RECONNECTING, u8"开始WebSocket重连");

	m_wsReconnectThd = std::thread([this]()
	{
		while (m_wsReconnectRunning && m_wsReconnectCount < MAX_WS_RECONNECT_ATTEMPTS)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(WS_RECONNECT_INTERVAL_MS));
			if (!m_wsReconnectRunning)
			{
				break;
			}
			AttemptWSReconnect();
		}

		if (m_wsReconnectCount >= MAX_WS_RECONNECT_ATTEMPTS)
		{
			m_wsStatus = NetworkStatus::FAILED;
			emit m_wsSignals.SigReconnectFailed(ConnectionType::WEBSOCKET, u8"超过最大重连次数");
			emit m_wsSignals.SigNetworkStatusChanged(ConnectionType::WEBSOCKET, NetworkStatus::FAILED, u8"WebSocket重连失败");
		}

		m_wsReconnectRunning = false;
	});
}

bool WSClientMgrImpl::TestHTTPConnectionWithPing()
{
	try 
	{
		LOG_INFO(QString(u8"开始HTTP ping连接测试"));

		// 发送GET请求到ping接口
		HttpRep response = m_httpClientWrap->Get("/ping");

		LOG_INFO(QString(u8"HTTP ping响应状态码: %1").arg(response.statusCode));
		LOG_INFO(QString(u8"HTTP ping响应内容: %1").arg(response.body.c_str()));

		// 检查状态码
		if (response.statusCode < 200 || response.statusCode >= 300) 
		{
			LOG_INFO(QString(u8"HTTP ping失败，状态码: %1").arg(response.statusCode));
			return false;
		}

		// 解析响应JSON
		if (!response.body.empty() && response.statusCode == 200)
		{
			LOG_INFO(QString(u8"HTTP ping测试成功，返回正确的pong响应"));
			return true;
		}
		else
		{
			LOG_INFO(QString(u8"HTTP ping响应为空"));
			return false;
		}

		//// 解析响应JSON
		//if (!response.body.empty())
		//{
		//	try 
		//	{
		//		nlohmann::json jsonResponse = nlohmann::json::parse(response.body);
		//		// 检查是否包含message字段且值为"pong"
		//		if (jsonResponse.contains("message") && jsonResponse["message"] == "pong") 
		//		{
		//			LOG_INFO("HTTP ping测试成功，返回正确的pong响应");
		//			return true;
		//		}
		//		else 
		//		{
		//			LOG_INFO(QString("HTTP ping响应格式不正确，期望{\"message\":\"pong\"}，实际: %1").arg(response.body.c_str()));
		//			return false;
		//		}
		//	}
		//	catch (const nlohmann::json::exception& e) 
		//	{
		//		LOG_INFO(QString("HTTP ping响应JSON解析失败: %1, 响应内容: %2").arg(e.what()).arg(response.body.c_str()));
		//		return false;
		//	}
		//}
		//else 
		//{
		//	LOG_INFO("HTTP ping响应为空");
		//	return false;
		//}
	}
	catch (const std::exception& e) 
	{
		LOG_INFO(QString(u8"HTTP ping连接测试异常: %1").arg(e.what()));
		return false;
	}
}

bool WSClientMgrImpl::CheckWebSocketConnection()
{
	try 
	{
		LOG_INFO("检查WebSocket连接状态");

		// 检查基本连接状态
		if (!m_bConnect || !m_wsClientWrap->IsConnected()) 
		{
			LOG_INFO("WebSocket基本状态检查：未连接");
			return false;
		}

		// 可以发送一个测试消息来验证连接是否正常
		// 这里我们先简单检查连接状态
		LOG_INFO("WebSocket连接状态检查：正常");
		return true;
	}
	catch (const std::exception& e) 
	{
		LOG_INFO(QString("WebSocket连接状态检查异常: %1").arg(e.what()));
		return false;
	}
}

bool WSClientMgrImpl::CheckHTTPConnection()
{
	try {
		LOG_INFO("检查HTTP连接状态");

		// 检查基本配置状态
		if (!m_bHttpCfg) {
			LOG_INFO("HTTP基本状态检查：未配置");
			return false;
		}

		// 通过ping接口测试HTTP连接
		return TestHTTPConnectionWithPing();
	}
	catch (const std::exception& e) {
		LOG_INFO(QString("HTTP连接状态检查异常: %1").arg(e.what()));
		return false;
	}
}

void WSClientMgrImpl::DisconnectAllConnections()
{
	LOG_INFO(QString(u8"主动断开所有网络连接"));

	try 
	{
		// 断开WebSocket连接
		if (m_bConnect && m_wsClientWrap->IsConnected()) 
		{
			LOG_INFO(QString(u8"主动断开WebSocket连接"));
			m_wsClientWrap->Disconnect();
			m_bConnect = false;
			m_wsStatus = NetworkStatus::DISCONNECTED;
			emit m_wsSignals.SigNetworkStatusChanged(ConnectionType::WEBSOCKET, NetworkStatus::DISCONNECTED, u8"主动断开WebSocket连接");
		}

		//// 断开HTTP连接
		//if (m_bHttpCfg) {
		//	LOG_INFO("主动断开HTTP连接");
		//	m_bHttpCfg = false;
		//	m_httpStatus = NetworkStatus::DISCONNECTED;
		//	emit m_wsSignals.SigNetworkStatusChanged(ConnectionType::HTTP, NetworkStatus::DISCONNECTED, "主动断开HTTP连接");
		//}

		// 短暂等待，确保断开操作完成
		std::this_thread::sleep_for(std::chrono::milliseconds(1000));

		LOG_INFO(QString(u8"所有网络连接已断开"));
	}
	catch (const std::exception& e) 
	{
		LOG_INFO(QString(u8"断开网络连接异常: %1").arg(e.what()));
	}
}

bool WSClientMgrImpl::ShouldAutoReconnect()
{
	// 1. 检查用户是否手动关闭了网络按钮
	if (GetIsCloseNetBtnStatus()) 
	{
		LOG_INFO(QString(u8"用户手动关闭了网络按钮，跳过自动重连"));
		return false;
	}

	// 2. 检查当前是否已经在重连中
	if (m_wsReconnectRunning) 
	{
		LOG_INFO(QString(u8"WebSocket重连已在进行中，跳过重复重连"));
		return false;
	}

	// 3. 检查是否有业务线程在工作（这里可以根据具体业务需求扩展）
	// 例如：检查发送队列、心跳状态等
	if (m_sendThdRunning || m_heartbeatThdRunning) 
	{
		LOG_INFO(QString(u8"检测到业务线程正在工作，允许自动重连"));
		return true;
	}

	// 4. 默认情况下允许重连
	LOG_INFO(QString(u8"满足重连条件，允许自动重连"));
	return true;
}

void WSClientMgrImpl::CacheMessage(const UnifiedMessage& msg)
{
	std::lock_guard<std::mutex> lock(m_cacheMtx);

	// 检查缓存队列是否已满
	if (m_cacheQue.size() >= MAX_CACHED_MESSAGES)
	{
		// 移除最早的消息
		m_cacheQue.pop();
		LOG_INFO(QString(u8"缓存队列已满，移除最早的消息"));
	}

	// 添加新消息到缓存队列
	m_cacheQue.push(msg);
	emit m_wsSignals.SigSyncCacheFinishTask(msg);
	LOG_INFO(QString(u8"消息已缓存，队列大小: %1/%2，消息类型: %3")
		.arg(m_cacheQue.size())
		.arg(MAX_CACHED_MESSAGES)
		.arg(static_cast<int>(msg.method)));
}


void WSClientMgrImpl::ProcessCachedMessages()
{
	std::lock_guard<std::mutex> lock(m_cacheMtx);

	if (m_cacheQue.empty())
	{
		return;
	}

	LOG_INFO(QString(u8"开始处理缓存消息，队列大小: %1").arg(m_cacheQue.size()));

	while (!m_cacheQue.empty())
	{
		UnifiedMessage cacheMsg = m_cacheQue.front();
		m_cacheQue.pop();
		
		//将缓存消息重新假如到发送队列
		{
			std::lock_guard<std::mutex> sendLock(m_sendMtx);
			m_sendQue.push(cacheMsg);
		}
		//通知发送线程
		m_sendCV.notify_one();
		LOG_INFO(QString(u8"缓存消息已重新加入发送队列，类型: %1").arg(static_cast<int>(cacheMsg.method)));
	}
	LOG_INFO(QString(u8"所有缓存消息已重新发送"));
}

void WSClientMgrImpl::ClearCacheMessages()
{
	std::lock_guard<std::mutex> lock(m_cacheMtx);
	size_t cnt = m_cacheQue.size();

	// 清空缓存队列
	std::queue<UnifiedMessage> emptyQue;
	m_cacheQue.swap(emptyQue);
	LOG_INFO(QString(u8"已清空缓存消息队列，清除消息数: %1").arg(cnt));
}

size_t WSClientMgrImpl::GetCachedMessagesSize()
{
	std::lock_guard<std::mutex> lock(m_cacheMtx);
	return m_cacheQue.size();
}

std::queue<UnifiedMessage> WSClientMgrImpl::GetCachedMessages()
{
	std::lock_guard<std::mutex> lock(m_cacheMtx);
	auto data = m_cacheQue;
	return data;
}
