#include "hard2Ser_2_0.h"
#include "CLogManager.h"

#include <string>
#include <vector>
#include <iostream>

#include <QGridLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QUuid>
#include <QMetaType>

#include "SingleOSSToken.h"
#include "CommFun.h"
#include "CameraTcpMgr.h"


//#include "hv\WebSocketClient.h"
//using namespace hv;


hard2Ser_2_0::hard2Ser_2_0(QWidget *parent)
    : QWidget(parent)
	, m_connMgr(std::make_unique<WSClientMgr>())
	, m_workMgr(std::make_unique<WorkThdMgr>())
	, m_workUI(new ScanUI())
	, m_manuWorkUI(new ManuUI())
	, m_stickWorkUI(new StickUI())
	, m_loginUI(new StoreLoginUI())
{
	qRegisterMetaType<WSMsgBase>("WSMsgBase");
	qRegisterMetaType<HttpMsgBase>("HttpMsgBase");
	qRegisterMetaType<DeviceInfo>("DeviceInfo");
	qRegisterMetaType<DevBindResp>("DevBindResp");
	qRegisterMetaType<SBaseSerResp>("SBaseSerResp");


	//qRegisterMetaType<CountListItem>("CountListItem");
	qRegisterMetaType<SkuParam>("SkuParam");
	qRegisterMetaType<SerInfo>("SerInfo");
	qRegisterMetaType<UserDetailInfo>("UserDetailInfo");
	qRegisterMetaType<DevSerItem>("DevSerItem");
	qRegisterMetaType<DevSereMapResp>("DevSereMapResp");

	qRegisterMetaType<ConnectionType>("ConnectionType");
	qRegisterMetaType<NetworkStatus>("NetworkStatus");
	qRegisterMetaType<OldTcpMessage>("OldTcpMessage");
	qRegisterMetaType<TcpConnection>("TcpConnection");

	
	qRegisterMetaType<CameraScanData>("CameraScanData");
	qRegisterMetaType<FingerScanData>("FingerScanData");
	qRegisterMetaType<SyncBusinessTask>("SyncBusinessTask");
	qRegisterMetaType<SimulateData>("SimulateData");

	Init();

	connect(CameraTcpManager::GetInstance().GetSignals(), &CameraTcpManagerSignals::SigCameraConnected, this, [this](const std::string& id)
	{
		int a = 1;
	});

}


hard2Ser_2_0::~hard2Ser_2_0()
{
	m_workMgr->StopAllWorkThds();
	m_connMgr->DisconnectFromSer();

	// 停止定时器
	if (m_devRegTimer && m_devRegTimer->isActive())
	{
		OnSendDevReg();
		m_devRegTimer->stop();
	}

}

void hard2Ser_2_0::Init()
{
	InitUI();
	InitConnect();

	m_workMgr->StartAllWorkThds();

	m_devRegTimer = new QTimer(this);
	connect(m_devRegTimer, &QTimer::timeout, this, &hard2Ser_2_0::OnSendDevReg);
	//确保是重复定时器
	m_devRegTimer->setSingleShot(false);

}

void hard2Ser_2_0::InitConnect()
{

	//logic_connect
	//ws
	connect(m_connMgr->GetSignals(), &WSClientSig::SigNetworkStatusChanged, this, &hard2Ser_2_0::OnUpdateNetConnStatus);
	connect(m_connMgr->GetSignals(), &WSClientSig::SigMsgRecevied, this, &hard2Ser_2_0::OnHandleRecvMsg);
	connect(m_connMgr->GetSignals(), &WSClientSig::SigHeartBeatAckReceived, this, &hard2Ser_2_0::OnHandleHeartBeatAck);
	connect(m_connMgr->GetSignals(), &WSClientSig::SigBindRegDev, this, &hard2Ser_2_0::OnDevBind);
	connect(m_connMgr->GetSignals(), &WSClientSig::SigRegDevTaskSync, this, &hard2Ser_2_0::OnRegDevTaskHandle, Qt::QueuedConnection);

	//connect(m_connMgr.get(), &WSClientMgr::SigWSTest, this,&hard2Ser_2_0::OnDevBind, Qt::QueuedConnection);
	
	//work
	connect(m_workMgr->GetSignals(), &WorkThdSig::SigTaskStatusUpdate, this, &hard2Ser_2_0::OnHandleTaskStatusUpdate);
	connect(m_workMgr->GetSignals(), &WorkThdSig::SigTaskStatusUpdate1, this, &hard2Ser_2_0::OnHandleTaskStatusUpdate1);
	connect(m_workMgr->GetSignals(), &WorkThdSig::SigDeviceStatusUpdate, this, &hard2Ser_2_0::OnHandleDevStatusUpdate);
	connect(m_workMgr->GetSignals(), &WorkThdSig::SigUpdateDevStatus2UI, this, &hard2Ser_2_0::OnWorkDevStatusUpdate);
	connect(m_workMgr->GetSignals(), &WorkThdSig::SigDevRegFinished, this, &hard2Ser_2_0::TestTaskRun);
	connect(m_workMgr->GetSignals(), &WorkThdSig::SigGetOSSToken, this, &hard2Ser_2_0::OnHandleGetOSSToken);


	//connect(m_workMgr, &WorkThdMgr::SigTaskStatusUpdate, this, &hard2Ser_2_0::OnHandleTaskStatusUpdate);
	//connect(m_workMgr, &WorkThdMgr::SigDeviceStatusUpdate, this, &hard2Ser_2_0::OnHandleDevStatusUpdate);

	//UI 
	//store_id__WS
	connect(&m_loginUI, &StoreLoginUI::SigLoinSerOper, this, &hard2Ser_2_0::OnConnSerOper);
	connect(&m_loginUI, &StoreLoginUI::SigLogoutSerOper, this, &hard2Ser_2_0::OnDisConnSerOper);
	connect(&m_loginUI, &StoreLoginUI::SigStoreIdLogin, this, &hard2Ser_2_0::OnHandleStoreLogin);
	connect(&m_loginUI, &StoreLoginUI::SigLoginBindDev, this, &hard2Ser_2_0::OnUIGetRegDevSync);
	connect(&m_loginUI, &StoreLoginUI::SigTestOper, this, &hard2Ser_2_0::OnHandleGetOSSToken);
	connect(&m_loginUI, &StoreLoginUI::SigTestOper2, this, &hard2Ser_2_0::OnHandleUITestOper2);

	//scan_work_
	connect(&m_workUI, &ScanUI::SigDevDestory, this, &hard2Ser_2_0::OnUIDevStoppedUpdate);
	connect(&m_manuWorkUI, &ManuUI::SigDevDestory, this, &hard2Ser_2_0::OnUIDevStoppedUpdate);
	connect(&m_stickWorkUI, &StickUI::SigDevDestory, this, &hard2Ser_2_0::OnUIDevStoppedUpdate);

}


void hard2Ser_2_0::InitUI()
{
	//main_layout
	QVBoxLayout* pv = new QVBoxLayout(this);
	this->setLayout(pv);


	pv->addWidget(&m_loginUI);


	QHBoxLayout* pH = new QHBoxLayout();
	{
		QVBoxLayout* pv_scan = new QVBoxLayout(this);
		{
			pv_scan->addWidget(&m_workUI);
		}
		pH->addLayout(pv_scan);

		QVBoxLayout* pv_manu = new QVBoxLayout(this);
		{
			pv_manu->addWidget(&m_manuWorkUI);
		}
		pH->addLayout(pv_manu);

		QVBoxLayout* pv_stick = new QVBoxLayout(this);
		{
			pv_stick->addWidget(&m_stickWorkUI);
		}
		pH->addLayout(pv_stick);

		QVBoxLayout* pv_main = new QVBoxLayout(this);
		pH->addLayout(pv_main);


		//log
		QGroupBox* logGroup = new QGroupBox("Logs", this);
		{
			QVBoxLayout* pv_log = new QVBoxLayout(logGroup);
			m_logDisplayEdit = new QTextEdit(this);
			m_logDisplayEdit->setReadOnly(true);
			pv_log->addWidget(m_logDisplayEdit);
			logGroup->setLayout(pv_log);
		}
		pv_main->addWidget(logGroup);

		//dev_status_unit
		QGroupBox* devStatusGroup = new QGroupBox("dev Status", this);
		{
			QVBoxLayout* pv_dev = new QVBoxLayout(devStatusGroup);
			m_devStatusWidget = new QListWidget(this);
			pv_dev->addWidget(m_devStatusWidget);
			devStatusGroup->setLayout(pv_dev);
		}
		pv_main->addWidget(devStatusGroup);
	}
	pv->addLayout(pH);
}


void hard2Ser_2_0::PrintLogInfo(const QString& msg)
{
	LOG_INFO(QString(msg));
	std::cout << msg.toStdString().c_str() << std::endl;
	m_logDisplayEdit->append(msg);
	//todo: log_ouptu
	//PrintLogInfo(QString("Connection status: %1").arg(status_text));
}

void hard2Ser_2_0::SendAllDeviceReg()
{
	if (m_connMgr->IsConnected())
	{
		try
		{
			std::vector<DeviceInfo> allDev = m_workMgr->GetAllDeviceStatus();
			if (!allDev.empty())
			{
				WSMsgBase msg;
				msg.msgId = "000001";
				msg.msgType = "sync_device";
				msg.type = MessageType::CLIENT_SYNC_DEV_STATUS;
				nlohmann::json jsonStr;
				auto payload = nlohmann::json::array();

				for (const auto& it : allDev)
				{
					try
					{
						payload.push_back(nlohmann::json::parse(it.toJson()));
					}
					catch (const std::exception& e)
					{
						PrintLogInfo(QString("Error serializing device %1: %2").arg(QString::fromStdString(it.devId)).arg(e.what()));
						continue;
					}
				}

				if (!payload.empty())
				{
					msg.payload = payload;
					m_connMgr->SendWSMsg(msg);
					PrintLogInfo(QString("Sent device registration to server. Total devices: %1").arg(jsonStr["devices"].size()));
				}
				else
				{
					PrintLogInfo("No valid devices to register");
				}

				//每10秒发送一次设备注册信息
				if (!m_devRegTimer->isActive())
				{
					m_devRegTimer->start(10000);
				}
			}
			else
			{
				PrintLogInfo("No devices available for registration");
			}
		}
		catch (const std::exception& e)
		{
			PrintLogInfo(QString("Error during device registration: %1").arg(e.what()));
		}
	}
	else
	{
		PrintLogInfo(QString::fromLocal8Bit("未连接服务器，无法进行设备注册"));
	}
}

void hard2Ser_2_0::OnDisConnSerOper()
{
	PrintLogInfo(QString::fromLocal8Bit("尝试断开服务器连接"));
	m_connMgr->DisconnectFromSer(true);
}

void hard2Ser_2_0::OnUpdateNetConnStatus(ConnectionType netType, NetworkStatus status, const QString& message /*= ""*/)
{
	PrintLogInfo(QString("lrz_net_moudle cur_change_net_type: %1, net_status: %2, msg:%3")
		.arg(QString::number(int(netType)))
		.arg(QString::number(int(status)))
		.arg(message));
	
	// 1028_网络连接断开，则停止同步设备状态
	if (ConnectionType::WEBSOCKET == netType && NetworkStatus::DISCONNECTED == status)
	{
		m_devRegTimer->stop();
	}

	// 连接成功后发送设备注册信息
	if (NetworkStatus::CONNECTED == status)
	{
		//SendAllDeviceReg();
		//PrintLogInfo(QString::fromLocal8Bit("尝试服务器连接"));
		//m_connMgr->Connect2Ser(msg.wsUrl, msg.httpUrlBase);
	}
	//同步数据至登录UI模块
	// ver_1024
	//m_loginUI.ConnStatucChange(NetworkStatus::CONNECTED == status);
	// ver_1027
	m_loginUI.NetworkStatusChanged(netType, status, message);



	// 重连成功后，获取设备状态，筛选当前设备任务信息
	if (ConnectionType::HTTP == netType && 
		NetworkStatus::CONNECTED == status && 
		message.contains(u8"HTTP重连成功") && 
		g_netReconnectStatus)
	{
		// HTTP登陆操作->获取设备信息->获取任务状态
		//auto sendMsg = m_connMgr->CreateHttpMessage(MessageType::CLIENT_HTTP_CONN, "http://shop.moonbii.net", {});
		//m_connMgr->SendHTTPMsg(sendMsg);
		//调用ongoing接口，获取服务器设备状态信息
		OnUIGetRegDevSync();
	}
	
}

//接收任务处理
void hard2Ser_2_0::OnHandleRecvMsg(const QString& msgTypeStr, const WSMsgBase& msgData)
{
	PrintLogInfo(QString("Received message Type: %1, Payload: %2").arg(msgTypeStr).arg(QString::fromStdString(msgData.payload.dump())));
	
	//qstring -> messageType
	//qstring -> payload
	MessageType msgType = static_cast<MessageType>(msgTypeStr.toInt());
	//针对payload数据进行处理
	std::string payloadStdStr = msgData.payload.dump();
	//服务器分发任务
	if (MessageType::SER_DISPATCH_TASK == msgType || msgData.type == MessageType::SER_DISPATCH_TASK)
	{
		try
		{
			//parse json
			//BusinessTask task = BusinessTask::fromJson(payloadStdStr);
			BusinessTask task = BusinessTask::fromJson(msgData.payload.dump());
			PrintLogInfo(QString("Server pushed task: %1 for business %2, device %3").arg(QString::fromStdString(task.proId)).arg(QString::fromStdString(task.op)).arg(QString::fromStdString(task.devId)));

			// 验证是否可以分发（检查业务线程是否存在对应设备）
			std::vector<DeviceInfo> allDevs = m_workMgr->GetAllDeviceStatus();
			bool bFindDev = false;
			for (const auto& dev : allDevs)
			{
				if (dev.devId == task.devId)
				{
					bFindDev = true;
					break;
				}
			}
			if (bFindDev)
			{
				if (task.op == "2")
				{
					m_workMgr->CancelTask(task.devId);
				}
				else if (task.op == "1")
				{
					//分发业务到thd
					m_workMgr->DispatchTask(task);
				}
			}
			else
			{
				PrintLogInfo(QString("Error: Device %1 not found for task %2").arg(QString::fromStdString(task.devId)).arg(QString::fromStdString(task.proId)));
			}

		}
		catch (const nlohmann::json::exception& e)
		{
			PrintLogInfo(QString("Error parsing BusinessTask from server: %1").arg(e.what()));
		}
	}
}




void hard2Ser_2_0::OnHandleHeartBeatAck()
{
	PrintLogInfo(QString("Received server heartbeat acknowledgment."));
	//todo send_ack
	// dismiss ack num
}

//同步_进程任务
void hard2Ser_2_0::OnHandleTaskStatusUpdate(const QString& taskId, int op, const QString& devId)
{
	PrintLogInfo(QString("Task %1 on Device %2 op: %3").arg(taskId).arg(devId).arg(op));

	// 当任务完成时，向服务器同步任务状态和设备状态
	// 包含任务完成、任务取消
	//if (static_cast<TaskStatus>(op) == TaskStatus::COMPLETED || static_cast<TaskStatus>(op) == TaskStatus::CANCELLED)
	{
		WSMsgBase msg;
		msg.type = MessageType::CLIENT_SYNC_TASK_STATUS;
		msg.msgType = WSMsgBase::convertEnum2Str(msg.type);
		msg.msgId = QUuid::createUuid().toString().toStdString();
		SyncBusinessTask syncTask;
		syncTask.devId = devId.toStdString();
		syncTask.proId = taskId.toStdString();
		syncTask.op = std::to_string(op);
		msg.payload = syncTask.toJson();


		// 1028_add_断网期间若存在完成的服务进程，则需
		bool bNewTask = true;
		for (const auto it : m_unnetFinishTaskQue)
		{
			if (it.proId == syncTask.proId)
			{
				bNewTask = false;
				break;
			}
		}
		if (bNewTask)
		{
			m_unnetFinishTaskQue.push_back(syncTask);
		}
		//获取最新设备状态
		//std::vector<DeviceInfo> curDevStatus = m_workMgr->GetAllDeviceStatus();
		//for (const auto& devIt : curDevStatus)
		//{
		//	if (devIt.deviceId == devId.toStdString())
		//	{
		//		jPayload["deviceStatus"] = nlohmann::json::parse(devIt.toJson());
		//		break;
		//	}
		//}
		m_connMgr->SendWSMsg(msg);
		PrintLogInfo(QString("Sent task op update to server for task %1 (device %2): %3").arg(taskId).arg(devId).arg(op));
	}
}
//new_logic_update
void hard2Ser_2_0::OnHandleTaskStatusUpdate1(const SyncBusinessTask& taskData)
{
	PrintLogInfo(QString("Task %1 on Device %2 op: %3")
		.arg(taskData.proId.c_str())
		.arg(taskData.devId.c_str())
		.arg(taskData.op.c_str()));

	// 当任务完成时，向服务器同步任务状态和设备状态
	// 包含任务完成、任务取消
	//if (static_cast<TaskStatus>(op) == TaskStatus::COMPLETED || static_cast<TaskStatus>(op) == TaskStatus::CANCELLED)
	{
		WSMsgBase msg;
		msg.type = MessageType::CLIENT_SYNC_TASK_STATUS;
		msg.msgType = WSMsgBase::convertEnum2Str(msg.type);
		msg.msgId = QUuid::createUuid().toString().toStdString();
		SyncBusinessTask syncTask = taskData;
		msg.payload = syncTask.toJson();
		auto tmp = syncTask.toJson().dump();
		LOG_INFO(QString(u8"lrz_show_sync_task_info sync_task_size = %1, data = %2").arg(tmp.size()).arg(tmp.c_str()));
		
		if (tmp.size() == 0)
		{
			LOG_INFO(QString(u8"lrz_show_sync_task_info sync_task_size < 100").arg(tmp.size()).arg(tmp.c_str()));
		}
	
		// 1028_add_断网期间若存在完成的服务进程，则需
		bool bNewTask = true;
		for (const auto it : m_unnetFinishTaskQue)
		{
			if (it.proId == syncTask.proId)
			{
				bNewTask = false;
				break;
			}
		}
		if (bNewTask)
		{
			m_unnetFinishTaskQue.push_back(syncTask);
		}
		//获取最新设备状态
		//std::vector<DeviceInfo> curDevStatus = m_workMgr->GetAllDeviceStatus();
		//for (const auto& devIt : curDevStatus)
		//{
		//	if (devIt.deviceId == devId.toStdString())
		//	{
		//		jPayload["deviceStatus"] = nlohmann::json::parse(devIt.toJson());
		//		break;
		//	}
		//}
		m_connMgr->SendWSMsg(msg);
		PrintLogInfo(QString("Sent task op update to server for task %1 (device %2): %3")
			.arg(taskData.proId.c_str())
			.arg(taskData.devId.c_str())
			.arg(taskData.op.c_str()));
	}
}

void hard2Ser_2_0::OnHandleDevStatusUpdate(const QString& devId, int devType, int status)
{
	//todo: 针对设备状态进行同步处理，同步至UI，底层，等待后续同步发送


	DeviceInfo devInfo;
	devInfo.devId = devId.toStdString();
	devInfo.devType = devType;
	devInfo.devStatus = static_cast<DeviceStatus>(status);
	if (devInfo.devStatus == DeviceStatus::OFFLINE)
	{
		int a = 1;
	}
	OnUpdateDevStatusUI(devInfo);

	auto a = m_workMgr->GetAllDeviceStatus();
	////dev更新状态时，像ser发送状态信息
	//// todo 长链直接同步状态
	//if (m_connMgr->IsConnected())
	//{
	//	WSMsgBase msg;
	//	msg.type = MessageType::CLIENT_REGISTER_DEVICE_STATUS;
	//	nlohmann::json jPayload;
	//	jPayload["devices"] = nlohmann::json::array();
	//	jPayload["devices"].push_back(nlohmann::json::parse(devInfo.deviceId));
	//	msg.payload = jPayload.dump();
	//	m_connMgr->SendData(msg);
	//	PrintLogInfo(QString("Sent single device status update to server for device %1: %2").arg(devId).arg(status));
	//}
}

void hard2Ser_2_0::OnUpdateDevStatusUI(const DeviceInfo& info)
{
	QString devEntry = QString("dev_Id_%1, type;_%2, status_%3")
						.arg(QString::fromStdString(info.devId))
						.arg(info.devType)
						.arg(static_cast<int>(info.devStatus));
	if (info.devStatus == DeviceStatus::OFFLINE)
	{
		int a = 1;
	}
	bool bFind = false;
	for (int i = 0; i < m_devStatusWidget->count(); ++i)
	{
		QListWidgetItem* item = m_devStatusWidget->item(i);
		if (item->text().startsWith(QString("Device_ID: %1").arg(QString::fromStdString(info.devId))))
		{
			item->setText(devEntry);
			bFind = true;
			break;
		}
	}
	if (!bFind)
	{
		m_devStatusWidget->addItem(devEntry);
	}

}

void hard2Ser_2_0::OnSendDevReg()
{
	if (m_connMgr->IsConnected())
	{
		try
		{
			std::vector<DeviceInfo> allDev = m_workMgr->GetAllDeviceStatus();
			if (!allDev.empty())
			{
				WSMsgBase msg;
				msg.msgId = "000 001";
				msg.msgType = "sync_device";
				msg.type = MessageType::CLIENT_SYNC_DEV_STATUS;
				nlohmann::json jPayload = nlohmann::json::array();

				for (const auto& devInfo : allDev)
				{
					try
					{
						jPayload.push_back(nlohmann::json::parse(devInfo.toJson()));
					}
					catch (const std::exception& e)
					{
						PrintLogInfo(QString("Error serializing device %1: %2").arg(QString::fromStdString(devInfo.devId)).arg(e.what()));						
						continue; // 跳过有问题的设备，继续处理其他设备
					}
				}
				if (!jPayload.empty())
				{
					msg.payload = jPayload;
					m_connMgr->SendWSMsg(msg);
					//PrintLogInfo(QString("Sent device registration to server. Total devices: %1").arg(jPayload["devices"].size()));
				}
				else
				{
					PrintLogInfo("No valid devices to register");
				}
			}
			else
			{
				PrintLogInfo("No devices available for registration");
			}
		}
		catch (const std::exception& e)
		{
			PrintLogInfo(QString("Error during device registration: %1").arg(e.what()));

		}
	}
	else
	{
		PrintLogInfo("Cannot send device registration: not connected to server");
	}
}

void hard2Ser_2_0::OnDevBind(const DevBindResp& data)
{
	//设备注册
	auto a = data;
	//for (auto& it : a.devData.devVec)
	//{
	//	it.devStatus = DeviceStatus::IDLE;
	//}
	m_workMgr->AddRegDevInfo(a.devData.devVec);
	m_workUI.SyncRegDevStatus(a.devData.devVec);
	m_manuWorkUI.SyncRegDevStatus(a.devData.devVec);
	m_stickWorkUI.SyncRegDevStatus(a.devData.devVec);

}


void hard2Ser_2_0::OnConnSerOper(const LoginMsg& msg)
{
	PrintLogInfo(QString::fromLocal8Bit("尝试服务器连接"));
	m_connMgr->Connect2Ser(msg.wsUrl, msg.httpUrlBase);
}

void hard2Ser_2_0::OnHandleStoreLogin()
{
	auto sendMsg = m_connMgr->CreateHttpMessage(MessageType::CLIENT_HTTP_CONN, "http://shop.moonbii.net", {});
	m_connMgr->SendHTTPMsg(sendMsg);

}

void hard2Ser_2_0::OnHandleGetOSSToken()
{
	auto sendMsg = m_connMgr->CreateHttpMessage(MessageType::CLIENT_OSS_TOKEN, "http://shop.moonbii.net", {});
	m_connMgr->SendHTTPMsg(sendMsg);
}

//Note: 设备上报故障处理
void hard2Ser_2_0::OnUIDevStoppedUpdate(bool bStopped, const DeviceInfo& dev)
{
	auto updateDev = dev;
	m_workMgr->HandleStopeedDev(dev);
	//UI层改变设备状态
	//switch (dev.devType)
	//{
	//	//scan
	//case 1:
	//{
	//	updateDev.devStatus = DeviceStatus::ERR;
	//	m_workUI.SyncWorkDevStatus(updateDev);
	//	break;
	//}
	//case 2:
	//{
	//	updateDev.devStatus = DeviceStatus::ERR;
	//	m_manuWorkUI.SyncWorkDevStatus(updateDev);
	//	break;
	//}
	//case 3:
	//{
	//	updateDev.devStatus = DeviceStatus::ERR;
	//	m_stickWorkUI.SyncWorkDevStatus(updateDev);
	//	break;
	//}
	//default:
	//	break;
	//
	//}
	//同步设备信息至服务器
	OnSendDevReg();
}

void hard2Ser_2_0::OnUIGetRegDevSync()
{
	//auto sendMsg = m_connMgr->CreateHttpMessage(MessageType::CLIENT_GET_DEV_INFO, "http://shop.moonbii.net");
	//m_connMgr->SendHTTPMsg(sendMsg);

	//test_add
	auto allDev = m_workMgr->GetAllDeviceStatus();
	auto sendMsg = m_connMgr->CreateHttpMessage(MessageType::CLIENT_GET_REG_RUN_DEV_INFO, "http://shop.moonbii.net", allDev);
	m_connMgr->SendHTTPMsg(sendMsg);
	
	//设置所有设备状态为空闲状态


}

//未使用Fun
void hard2Ser_2_0::OnUIGetDevTaskSync()
{
	auto allDev = m_workMgr->GetAllDeviceStatus();
	auto sendMsg = m_connMgr->CreateHttpMessage(MessageType::CLIENT_GET_REG_RUN_DEV_INFO, "http://shop.moonbii.net", allDev);
	m_connMgr->SendHTTPMsg(sendMsg);
	
}

void hard2Ser_2_0::OnRegDevTaskHandle(const DevSereMapResp& data)
{
	auto mapData = data.devSerMap;
	auto allDev = m_workMgr->GetAllDeviceStatus();

	//数据处理
	//处于忙碌状态设备进行设置
	for (const auto& taskIt : data.devSerMap)
	{
		//数据同步至服务器
		for (auto& proIt : taskIt.second.service.proVec)
		{
	
			// status=3时，表示设备为处于忙碌状态 
			// 同步判断是否为第一次启动，或断网重连状态
			// 此时判断是否为服务2进行的状态为3,是则继续，否则取消
			if (proIt.status == 3)
			{
				if (g_netReconnectStatus)
				{
					// 断网重连环境下
					// 新增任务与当前忙碌状态任务进行比对，判断任务是否处于执行中 			
					// todo_1027_判断当前ongong获取任务是否为新增任务
					bool bNoAddTask = false;
					for (auto it : m_unnetFinishTaskQue)
					{
						if (it.proId == proIt.serProId)
						{
							bNoAddTask = true;
							break;
						}
					}
					if (!bNoAddTask)
					{
						//当前任务为新增任务
						BusinessTask addTask;
						addTask.ordersId = taskIt.second.service.orderId;
						addTask.proId = proIt.serProId;
						addTask.devId = taskIt.first;
						addTask.op = "1";
						m_workMgr->DispatchTask(addTask);
						break;
					}
					// todo_1027_当前ongong获取任务为原有任务（不对数据进行处理


				}
				else
				{					
					// 非断网重连环境下
	   				//当前任务为新增任务
					BusinessTask addTask;
					addTask.ordersId = taskIt.second.service.orderId;
					addTask.proId = proIt.serProId;
					addTask.devId = taskIt.first;
					addTask.op = "1";
					m_workMgr->DispatchTask(addTask);
					break;
				}
			}
		}
	}
	// 对处于非忙碌状态设备，设置为空闲状态
	// 相当于全部为空闲状态下，设置设备状态
	if (!data.devSerMap.size())
	{
		m_workUI.SyncRegDevStatus(allDev);
		m_manuWorkUI.SyncRegDevStatus(allDev);
		m_stickWorkUI.SyncRegDevStatus(allDev);
	}

	//结束重连网络相关设置
	if (g_netReconnectStatus)
	{
		g_netReconnectStatus = g_netReconnectStatus ? !g_netReconnectStatus : g_netReconnectStatus;
		// 断网重连情况下
		// 1028——重置设备状态定时器
		m_devRegTimer->start(10000);
	}
	// 更新设备状态
	SendAllDeviceReg();
	// 清空缓存任务队列数据
	m_unnetFinishTaskQue.clear();
}

void hard2Ser_2_0::OnWorkDevStatusUpdate(const QString& devId)
{
	auto allDev = m_workMgr->GetAllDeviceStatus();
	for (auto& dev : allDev)
	{
		if (dev.devId == devId.toStdString())
		{
			switch (dev.devType)
			{
				//scan
			case 1:
			{
				m_workUI.SyncWorkDevStatus(dev);
				break;
			}
			case 2:
			{
				m_manuWorkUI.SyncWorkDevStatus(dev);
				break;
			}
			case 3:
			{
				m_stickWorkUI.SyncWorkDevStatus(dev);
				break;
			}
			default:
				break;
			}
		}
	}
}

void hard2Ser_2_0::OnHandleUITestOper()
{
	////默认任务下发操作指令
	////m_workUI
	//BusinessTask task;
	//task.devId = "100001";
	//task.op = "1";
	//task.ordersId = "123456789";
	//task.proId = "123456789_1";
	//m_workMgr->DispatchTask(task);
	//===============================================================
	//===============================================================

	//oss_token获取
	auto sendMsg = m_connMgr->CreateHttpMessage(MessageType::CLIENT_OSS_TOKEN, "http://shop.moonbii.net", {});
	m_connMgr->SendHTTPMsg(sendMsg);

}

void hard2Ser_2_0::OnHandleUITestOper2()
{

	LOG_INFO(QString(u8"制作业务线程模拟设备耗时操作定时器到期，模拟完成下载操作"));
	std::string tmpURL = "http://macbrush-shop-image.oss-cn-shanghai.aliyuncs.com/simulate_scan_data/20250811145822_101.ply";
	std::string retInfo = "";
	auto curTimerStr = QString::fromStdString(CommFun::GetInstance().GetCurrentTimeStr()) + "_25112711333600026482_2";
	auto downFolderPath = QCoreApplication::applicationDirPath() + QDir::separator() + QString("simulate_scan_data") + QDir::separator() + \
		QString("down_file") + QDir::separator() + curTimerStr;
	auto ret = SingleOSSToken::GetInstance().DownloadSingleFile(tmpURL, retInfo, downFolderPath.toStdString());


	//// oss_上传任务
	//auto dataPath = QCoreApplication::applicationDirPath() + QDir::separator() + QString("simulate_scan_data") + QDir::separator() + QString("src_data");
	//std::vector<std::string> vFilePath;
	//std::vector<std::string> vURLData;
	//std::string retInfo;
	//// 获取源文件夹中所有条目（文件和子文件夹）
	//CommFun::GetInstance().GetFolderFile(dataPath, vFilePath);
	//std::vector<FingerScanData> tmpVec;
	//SingleOSSToken::GetInstance().UploadMulti(vFilePath, retInfo, vURLData);
	//for (int i = 0; i < vURLData.size(); ++i)
	//{
	//	int fingerId = i < 5 ? 101 + i : 201 - 5 + i;
	//	FingerScanData tmpData;
	//	tmpData.fingerId = fingerId;
	//	tmpData.modelPath = vURLData.at(i);
	//	tmpVec.push_back(tmpData);
	//}
	//SyncBusinessTask tmpTask;
	//tmpTask.proId = "1234";
	//tmpTask.devId = "110001";
	//tmpTask.op = "3";
	//tmpTask.scanData = tmpVec;
	//OnHandleTaskStatusUpdate1(tmpTask);
	//// oss_拆分url
	//// oss_数据对应关系_json
}

//接收任务处理
void hard2Ser_2_0::TestTaskRun()
{

}
