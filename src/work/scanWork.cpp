#include "scanWork.h"
#include "global.h"


#include <chrono>
#include <QTimer>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonParseError>
#include <QMutexLocker>

#include "ipc/CameraTcpMgr.h"
#include "json.hpp"
#include "SingleOSSToken.h"
#include "CommFun.h"


ScanWorkThd::ScanWorkThd()
	: WorkThdBase("scan")
{
	//m_pImpl 是 unique_ptr<IBusinessThread> 类型，可以直接派生Impl
	this->m_pImpl = std::make_unique<ScanWorkThdImpl>();

	//connect
	//connect(dynamic_cast<ScanWorkThdImpl*>(m_pImpl.get()), &ScanWorkThdImpl::SigGetOSSTokenData, this, &ScanWorkThd::SigGetOSSTokenData2ThdMgr, Qt::QueuedConnection);
}

void ScanWorkThd::AddDevice(const std::shared_ptr<DeviceScan>& dev)
{
	if (auto pImpl = dynamic_cast<ScanWorkThdImpl*>(this->m_pImpl.get()))
	{
		pImpl->AddDevice(dev);
	}
	else
	{
		//dev_type_err;
		LOG_INFO(QString(u8"scan_work_thd::addDevice: p_impl_ is not of type scan_work_thd_dImpl. This indicates a logic error."));
	}
}


//----------------------------ScanWorkThdImpl----------------------------------------------
//----------------------------ScanWorkThdImpl----------------------------------------------
//----------------------------ScanWorkThdImpl----------------------------------------------

ScanWorkThdImpl::ScanWorkThdImpl()
	: WorkThdBaseImpl("scan")
{	
	//初始化相机设备ID集合（可根据配置文件或数据库配置）
	m_cameraDeviceIds.insert("100001"); // 相机设备ID

	////// 连接相机TCP管理器信号
	//auto cameraMgrSig = CameraTcpManager::GetInstance().GetSignals();
	//CameraTcpManager::GetInstance().StartServer(); 
	////auto sig = CameraTcpManager::GetInstance();
	////connect(&CameraTcpManager::GetInstance(), &CameraTcpManager::SigTest, this, &ScanWorkThdImpl::OnCameraConnected, Qt::QueuedConnection);

	//connect(cameraMgrSig, &CameraTcpManagerSignals::SigCameraConnected, this, &ScanWorkThdImpl::OnCameraConnected, Qt::QueuedConnection);
	//connect(cameraMgrSig, &CameraTcpManagerSignals::SigCameraDisconnected, this, &ScanWorkThdImpl::OnCameraDisconnected, Qt::QueuedConnection);
	//connect(cameraMgrSig, &CameraTcpManagerSignals::SigCameraTaskFinished, this, &ScanWorkThdImpl::OnCameraTaskFinished, Qt::QueuedConnection);
	//connect(cameraMgrSig, &CameraTcpManagerSignals::SigCameraTaskError, this, &ScanWorkThdImpl::OnCameraTaskError, Qt::QueuedConnection);
	//LOG_INFO(QString(u8"扫描业务线程已连接相机TCP管理器"));


	// ★ 使用回调函数注册相机事件处理
	auto& cameraMgr = CameraTcpManager::GetInstance();
	cameraMgr.StartServer();

	// 注册回调函数 - 使用lambda表达式捕获this指针
	cameraMgr.RegisterCameraConnectedCallback([this](const std::string& deviceId)	
	{
		this->OnCameraConnected(deviceId);
	});

	cameraMgr.RegisterCameraDisconnectedCallback([this](const std::string& deviceId) 
	{
		this->OnCameraDisconnected(deviceId);
	});

	cameraMgr.RegisterCameraTaskFinishedCallback([this](const std::string& deviceId, const std::vector<CameraScanData>& scanData) 
	{
		this->OnCameraTaskFinished(deviceId, scanData);
	});

	cameraMgr.RegisterCameraTaskErrorCallback([this](const std::string& deviceId, const std::string& errorMsg) 
	{
		this->OnCameraTaskError(deviceId, errorMsg);
	});

	// 注册相机任务取消回调
	cameraMgr.RegisterCameraTaskCancelledCallback([this](const std::string& deviceId, bool success)
	{
		this->OnCameraTaskCancelled(deviceId, success);
	});

	LOG_INFO(QString(u8"扫描业务线程注册相机TCP管理器回调函数完成"));
}

ScanWorkThdImpl::~ScanWorkThdImpl()
{
	// 清理相机任务状态
	std::lock_guard<std::mutex> lock(m_cameraTasksMutex);
	m_cameraRunningTasks.clear();
}

void ScanWorkThdImpl::AddDevice(const std::shared_ptr<DeviceScan>& dev)
{
	m_devicesMap[dev->GetDeviceID()] = dev;
	dev->RegStatusUpdateCallback([this](const DeviceInfo& info)
	{
		OnDeviceStatusUpdate(info);
	});
	auto a = dev->GetDeviceID();
	auto size = m_devicesMap.size();
	// 更新最大并发任务数
	UpdateMaxConcurrentTasks();
	LOG_INFO(QString(u8"scan_work_thd: Added device %1").arg(dev->GetDeviceID().c_str()));
}

//op(任务操作类型 1开始 3结束 4取消任务，进程重新排队 5终止任务
void ScanWorkThdImpl::Run()
{
	while (m_bRunning)
	{
		// 清理已完成的任务
		CleanupCompletedTasks();

		std::unique_lock<std::mutex> lock(m_taskMtx);
		m_taskCV.wait_for(lock, std::chrono::milliseconds(100),	[this]
		{
			return !m_taskQueue.empty() || !m_bRunning;
		});

		if (!m_bRunning)
		{
			break;
		}

		// 处理队列中的任务，且不超过最大并发数（TODO：后续根据设备数进行修改
		while (!m_taskQueue.empty() && GetRunningTaskCount() < GetMaxConcurrentTasks()) 
		{
			BusinessTask curTask = m_taskQueue.front();
			SyncBusinessTask task;
			task.devId = curTask.devId;
			task.proId = curTask.proId;
			task.op = curTask.op;
			m_taskQueue.pop();
			lock.unlock();

			// 检查设备是否存在且可用
			auto device_it = m_devicesMap.find(curTask.devId);
			if (device_it != m_devicesMap.end()) 
			{

				// 检查设备是否空闲
				if (device_it->second->GetStatus() == DeviceStatus::IDLE) 
				{
					//LOG_INFO(QString("scan_work_thd processing task: %1 for device %2").arg(QString::fromStdString(curTask.proId)).arg(QString::fromStdString(curTask.devId)));
					//准备开始执行任务，同步发生开始任务报文
					auto startOp = "1";
					task.op = startOp;
					m_taskStatusCallBack1(task);
					// 执行任务，在具体任务重根据标志位数据，判断是真实数据还是模拟数据
					ExecuteTaskAsync(curTask);
				}
				else 
				{
					// 设备忙碌，重新放入队列
					lock.lock();
					m_taskQueue.push(curTask);
					lock.unlock();
					break; // 等待下次循环
				}
			}
			else
			{
				LOG_INFO(QString(u8"scan_work_thd: Device %1 not found for task %2").arg(QString::fromStdString(curTask.devId)).arg(QString::fromStdString(curTask.proId)));
				if (m_taskStatusCallBack1)
				{
					m_taskStatusCallBack1(task);
				}
			}
			lock.lock();
		}
	}

	// 停止时等待所有任务完成
	while (GetRunningTaskCount() > 0) 
	{
		CleanupCompletedTasks();
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
}

void ScanWorkThdImpl::ExecuteTaskAsync(const BusinessTask& task)
{
	std::lock_guard<std::mutex> lock(m_runningTasksMtx);
	// 创建任务执行信息
	auto task_info = std::make_shared<RunningTaskInfo>(task);
	// 创建并启动任务执行线程
	// 针对任务进行处理，判断是模拟数据处理还是真实数据处理
	if (!g_simulateReturnData)
	{	//真实数据
		task_info->task_thread = std::make_shared<std::thread>(&ScanWorkThdImpl::PerformTaskInThread, this, task);
	}
	else
	{	//模拟数据
		task_info->task_thread = std::make_shared<std::thread>(&ScanWorkThdImpl::PerformSimulateTaskInThread, this, task);
	}
	// 添加到正在运行的任务列表
	m_runningTasks[task.proId] = task_info;

	LOG_INFO(QString("Started async task %1 for device %2 in %3 thread")
		.arg(QString::fromStdString(task.proId))
		.arg(QString::fromStdString(task.devId))
		.arg(QString::fromStdString(m_workType)));
}


void ScanWorkThdImpl::PerformTaskInThread(const BusinessTask& task)
{
	try 
	{
		LOG_INFO(QString(u8"扫描业务线程处理任务: %1 for device %2")
			.arg(QString::fromStdString(task.proId))
			.arg(QString::fromStdString(task.devId)));
		auto a = m_devicesMap;

		// 判断当前任务是否为取消任务，如果是取消任务则通知相应服务进行进行取消操作

		// 检查是否为相机设备
		if (IsCameraDevice(task.devId)) 
		{
			LOG_INFO(QString(u8"检测到相机设备任务: Device=%1, Task=%2")
				.arg(QString::fromStdString(task.devId))
				.arg(QString::fromStdString(task.proId)));

			// 发送任务到相机设备
			if (SendTaskToCamera(task)) 
			{
				LOG_INFO(QString(u8"任务已发送到相机设备: %1").arg(QString::fromStdString(task.devId)));
			}
			else 
			{
				LOG_INFO(QString(u8"发送任务到相机设备失败: %1").arg(QString::fromStdString(task.devId)));
				if (m_taskStatusCallBack1)
				{
					SyncBusinessTask taskStatus;
					taskStatus.proId = task.proId;
					taskStatus.op = "4";
					taskStatus.devId = task.devId;
					m_taskStatusCallBack1(taskStatus); // 4表示终止服务
				}
			}
			return;
		}
		// 调用基类的实现处理本地设备
		//WorkThdBaseImpl::PerformTaskInThread(task);
	}
	catch (const std::exception& e) 
	{
		LOG_INFO(QString(u8"扫描任务执行异常: %1").arg(e.what()));
	}
}

void ScanWorkThdImpl::PerformSimulateTaskInThread(const BusinessTask& inTask)
{
	// 其中包含开始任务处理报文，将任务信息发送至相机软件进行后续处理
	try
	{
		auto task = inTask;
		LOG_INFO(QString(u8"扫描业务线程处理任务_模拟数据: %1 for device %2")
			.arg(QString::fromStdString(task.proId))
			.arg(QString::fromStdString(task.devId)));

		// 模拟数据处理逻辑 
		SetCameraTaskRunning(task.devId, task.proId, true);
		
		// 更新设备状态为忙碌同步至服务端(在设备中进行更新为忙碌状态
		//1120_更新设备状态到工控软件中设备map
		//if (m_deviceStatusCallBack)
		//{
		//	DeviceInfo deviceInfo;
		//	deviceInfo.devId = task.devId;
		//	deviceInfo.devType = 1; // 扫描设备类型
		//	deviceInfo.devStatus = DeviceStatus::BUSY; // 忙碌状态
		//	m_deviceStatusCallBack(deviceInfo);
		//}

		std::vector<CameraScanData> fingerData;
		auto it = m_devicesMap.find(task.devId);
		auto bCancel = false;
		if (it != m_devicesMap.end())
		{
			// 模拟设备执行任务
			auto ret = it->second->PerformTask(task); 
			bCancel = it->second->GetIsTaskCancel();
			if (ret)
			//if (it->second->PerformTask(task))
			{
				LOG_INFO(QString(u8"扫描业务线程处理任务完成_模拟数据: %1 for device %2")
					.arg(QString::fromStdString(task.proId))
					.arg(QString::fromStdString(task.devId)));
				fingerData = it->second->GetHandleWorkData().scanData;
			}
			else if (!ret && it->second->GetIsTaskCancel())
			{
				LOG_INFO(QString(u8"扫描业务线程处理任务取消_模拟数据，服务单取消返回op5: %1 for device %2")
					.arg(QString::fromStdString(task.proId))
					.arg(QString::fromStdString(task.devId)));
				// 判断当前设备是否为故障状态，是则返回排队态'4', 若为取消任务则返回'5'
				task.op = DeviceStatus::ERR == it->second->GetStatus() ? "4" : "5";
			}
		}
		OnCameraTaskFinished(task.devId, fingerData, task);
	}
	catch (const std::exception& e) 
	{
		LOG_INFO(QString(u8"扫描任务执行异常: %1").arg(e.what()));
	}
}

// 相机TCP管理器事件处理
void ScanWorkThdImpl::OnCameraConnected(const std::string& deviceId)
{
	LOG_INFO(QString(u8"相机设备连接: %1").arg(QString::fromStdString(deviceId)));

	// 更新设备状态为在线
	if (m_deviceStatusCallBack) 
	{
		DeviceInfo deviceInfo;
		deviceInfo.devId = deviceId;
		deviceInfo.devType = 1; // 扫描设备类型
		deviceInfo.devStatus = DeviceStatus::IDLE; // 空闲状态
		m_deviceStatusCallBack(deviceInfo);
		//1120_更新设备状态到工控软件中设备map
		UpdateDeviceMapStatus(deviceId, DeviceStatus::IDLE);
	}
}


void ScanWorkThdImpl::OnCameraDisconnected(const std::string& deviceId)
{
	LOG_INFO(QString(u8"相机设备断开: %1").arg(QString::fromStdString(deviceId)));

	// 清理该设备的运行任务
	{
		std::lock_guard<std::mutex> lock(m_cameraTasksMutex);
		auto it = m_cameraRunningTasks.find(deviceId);
		if (it != m_cameraRunningTasks.end()) {
			std::string taskId = it->second;
			m_cameraRunningTasks.erase(it);

			//// 回调任务失败状态
			if (m_taskStatusCallBack1)
			{
				SyncBusinessTask taskStatus;
				taskStatus.proId = taskId;
				taskStatus.op = "4";
				taskStatus.devId = deviceId;
				m_taskStatusCallBack1(taskStatus); // 4表示终止服务
			}
		}
	}

	// 更新设备状态为离线
	if (m_deviceStatusCallBack) 
	{
		DeviceInfo deviceInfo;
		deviceInfo.devId = deviceId;
		deviceInfo.devType = 1; // 扫描设备类型
		deviceInfo.devStatus = DeviceStatus::OFFLINE; // 离线状态
		m_deviceStatusCallBack(deviceInfo);
		//1120_更新设备状态到工控软件中设备map
		UpdateDeviceMapStatus(deviceId, DeviceStatus::OFFLINE);
	}
}


void ScanWorkThdImpl::OnCameraTaskFinished(const std::string& deviceId,
										   const std::vector<CameraScanData>& scanData, 
										   const BusinessTask& task)
{
	LOG_INFO(QString(u8"相机任务完成: Device=%1, 数据条数=%2")
		.arg(QString::fromStdString(deviceId)).arg(scanData.size()));

	// 获取正在运行的任务ID
	std::string taskId;
	{
		std::lock_guard<std::mutex> lock(m_cameraTasksMutex);
		auto it = m_cameraRunningTasks.find(deviceId);
		if (it != m_cameraRunningTasks.end()) 
		{
			taskId = it->second;
			m_cameraRunningTasks.erase(it);
		}
	}
	//正确逻辑
	//if (!taskId.empty())
	if (taskId.empty() || !taskId.empty())
	{
		// 处理扫描结果数据
		std::vector<FingerScanData> processData;
		ProcessCameraScanData1(scanData, processData);
		// 回调任务完成状态
		if (m_taskStatusCallBack1)
		{
			SyncBusinessTask taskStatus;
			taskStatus.proId = taskId;
			//通过m_cameraTaskInfo[deviceId]查询当前设备
			if (g_simulateReturnData)
			{
				//m_cameraTaskInfo[deviceId].op;
				if (task.op == "5")
				{
					int a = 1;
				}
				taskStatus.op = task.op == "1" ? "3" : task.op;
			}
			else
			{
				if (m_cameraTaskInfo[deviceId].op.empty())
				{
					taskStatus.op = "3";
				}
				else
				{
					taskStatus.op = m_cameraTaskInfo[deviceId].op;
				}
			}
			taskStatus.devId = deviceId;
			taskStatus.scanData = processData;
			//针对数据进行上传处理，并且构造数据		
			m_taskStatusCallBack1(taskStatus); // 3表示完成
		}
		//-------------------------------------------

		// 更新设备状态为空闲
		// 设备状态再Dev中已经设置
		if (m_deviceStatusCallBack) 
		{
			//DeviceInfo deviceInfo;
			//deviceInfo.devId = deviceId;
			//deviceInfo.devType = 1; // 扫描设备类型
			//deviceInfo.devStatus = DeviceStatus::IDLE; // 空闲状态
			//m_deviceStatusCallBack(deviceInfo);
			//1120_更新设备状态到工控软件中设备map
			//UpdateDeviceMapStatus(deviceId, DeviceStatus::IDLE);
		}
	}
}

void ScanWorkThdImpl::OnCameraTaskError(const std::string& deviceId, const std::string& errorMsg)
{
	LOG_INFO(QString(u8"相机任务错误: Device=%1, Error=%2")
		.arg(QString::fromStdString(deviceId)).arg(QString::fromStdString(errorMsg)));

	// 获取正在运行的任务ID
	std::string taskId;
	{
		std::lock_guard<std::mutex> lock(m_cameraTasksMutex);
		auto it = m_cameraRunningTasks.find(deviceId);
		if (it != m_cameraRunningTasks.end()) {
			taskId = it->second;
			m_cameraRunningTasks.erase(it);
		}
	}

	if (!taskId.empty()) {
		// 回调任务失败状态
		//if (m_taskStatusCallBack) 
		//{
		//	m_taskStatusCallBack(taskId, "4", deviceId); // 4表示终止服务
		//}
		if (m_taskStatusCallBack1)
		{
			SyncBusinessTask taskStatus;
			taskStatus.proId = taskId;
			taskStatus.op = "4";
			taskStatus.devId = deviceId;
			m_taskStatusCallBack1(taskStatus); // 4表示终止服务
		}

		// 更新设备状态为错误
		if (m_deviceStatusCallBack) 
		{
			DeviceInfo deviceInfo;
			deviceInfo.devId = deviceId;
			deviceInfo.devType = 1; // 扫描设备类型
			deviceInfo.devStatus = DeviceStatus::ERR; // 错误状态
			m_deviceStatusCallBack(deviceInfo);

			// 1120未同步到本地所有设备的状态中
			//1120_更新设备状态到工控软件中设备map
			//UpdateDeviceMapStatus(deviceId, DeviceStatus::ERR);
		}
	}
}

// 相机设备处理方法
bool ScanWorkThdImpl::IsCameraDevice(const std::string& deviceId) const
{
	// 1125_判断是否在初始状态添加相关设备ID
	return m_cameraDeviceIds.find(deviceId) != m_cameraDeviceIds.end();
}

bool ScanWorkThdImpl::SendTaskToCamera(const BusinessTask& task)
{
	CameraTcpManager& cameraMgr = CameraTcpManager::GetInstance();

	// 检查相机是否在线
	if (!cameraMgr.IsCameraOnline(task.devId)) 
	{
		LOG_INFO(QString(u8"相机设备不在线: %1").arg(QString::fromStdString(task.devId)));
		return false;
	}

	// 检查是否已有任务在运行
	if (IsCameraTaskRunning(task.devId)) 
	{
		LOG_INFO(QString(u8"相机设备正在执行其他任务: %1").arg(QString::fromStdString(task.devId)));
		return false;
	}

	// 发送任务开始报文
	bool success = cameraMgr.SendTaskStartToCamera(task.devId);

	if (success) 
	{
		// 记录任务运行状态
		SetCameraTaskRunning(task.devId, task.proId, true);
		// 更新设备状态为忙碌
		if (m_deviceStatusCallBack) 
		{
			DeviceInfo deviceInfo;
			deviceInfo.devId = task.devId;
			deviceInfo.devType = 1; // 扫描设备类型
			deviceInfo.devStatus = DeviceStatus::BUSY; // 忙碌状态
			m_deviceStatusCallBack(deviceInfo);
			m_cameraTaskInfo.insert({ task.devId , task });
			//1120_更新设备状态到工控软件中设备map
			//m_devicesMap[task.devId]->SetStatus(DeviceStatus::BUSY);
		}
	}
	return success;
}

void ScanWorkThdImpl::ProcessCameraScanData(const BusinessTask& task, const std::vector<CameraScanData>& scanData)
{
	// 序列化扫描结果数据
	std::string serializedData = SerializeCameraScanResults(scanData);

	LOG_INFO(QString(u8"相机扫描数据序列化完成，数据大小: %1 bytes").arg(serializedData.size()));

	// 这里可以将序列化后的数据发送到服务器
	// 例如通过HTTP或WebSocket发送到服务器
	// 加入OSS上传数据信息逻辑
}

void ScanWorkThdImpl::ProcessCameraScanData1(const std::vector<CameraScanData>& scanData,
											 std::vector<FingerScanData>& fingerData)
{
	if (fingerData.size())
	{
		fingerData.clear();
	}

	std::vector<std::string> vFilePath, vURLData;
	std::string retInfo;

	for (const auto& it : scanData)
	{
		FingerScanData data;
		data.fingerId = it.data_id;
		data.modelPath = it.data_path;
		fingerData.push_back(data);
		vFilePath.push_back(it.data_path);
	}

	//处理上传数据
	//auto curTimerStr = QString::fromStdString(CommFun::GetInstance().GetCurrentTimeStr());
	//auto dstFolderPath = QCoreApplication::applicationDirPath() + QDir::separator() + QString("simulate_scan_data") + QDir::separator() + curTimerStr;
	//auto srcFolderPath = QCoreApplication::applicationDirPath() + QDir::separator() + QString("simulate_scan_data") + QDir::separator() + QString("src_data");
	// 获取源文件夹中所有条目（文件和子文件夹）
	//CommFun::GetInstance().FolderCopy(srcFolderPath, dstFolderPath);
	//CommFun::GetInstance().GetFolderFile(dstFolderPath, vFilePath);
	SingleOSSToken::GetInstance().UploadMulti(vFilePath, retInfo, vURLData);

	std::vector<CameraScanData> simulateFingerData;
	for (int i = 0; i < vURLData.size(); ++i)
	{
		fingerData[i].modelPath = vURLData.at(i);
	}
}

std::string ScanWorkThdImpl::SerializeCameraScanResults(const std::vector<CameraScanData>& scanData)
{
	nlohmann::json resultJson;
	resultJson["scan_count"] = scanData.size();
	resultJson["scan_data"] = nlohmann::json::array();

	for (const auto& data : scanData) {
		nlohmann::json dataItem;
		dataItem["data_id"] = data.data_id;
		dataItem["data_path"] = data.data_path;
		resultJson["scan_data"].push_back(dataItem);
	}

	resultJson["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::system_clock::now().time_since_epoch()).count();

	return resultJson.dump();
}

void ScanWorkThdImpl::SetCameraTaskRunning(const std::string& deviceId, const std::string& taskId, bool running)
{
	std::lock_guard<std::mutex> lock(m_cameraTasksMutex);

	if (running) 
	{
		m_cameraRunningTasks[deviceId] = taskId;
	}
	else 
	{
		m_cameraRunningTasks.erase(deviceId);
	}
}

bool ScanWorkThdImpl::IsCameraTaskRunning(const std::string& deviceId) const
{
	std::lock_guard<std::mutex> lock(m_cameraTasksMutex);
	return m_cameraRunningTasks.find(deviceId) != m_cameraRunningTasks.end();
}

void ScanWorkThdImpl::HandleSimulateData(const BusinessTask& task)
{
	/*
	1. 复制当前文件夹
	2. 将当前文件夹重命名
	3. 提取当前文件夹中数据
	4. 调用文件夹
	*/
	auto curTimerStr = QString::fromStdString(CommFun::GetInstance().GetCurrentTimeStr());
	auto dstFolderPath = QCoreApplication::applicationDirPath() + QDir::separator() + QString("simulate_scan_data") + QDir::separator() + curTimerStr;
	auto srcFolderPath = QCoreApplication::applicationDirPath() + QDir::separator() + QString("simulate_scan_data") + QDir::separator() + QString("src_data");
	std::vector<std::string> vFilePath, vURLData;
	std::string retInfo;
	// 获取源文件夹中所有条目（文件和子文件夹）
	CommFun::GetInstance().FolderCopy(srcFolderPath, dstFolderPath);
	CommFun::GetInstance().GetFolderFile(dstFolderPath, vFilePath);
	SingleOSSToken::GetInstance().UploadMulti(vFilePath, retInfo, vURLData);

	std::vector<CameraScanData> fingerData;
	for (int i = 0; i < vURLData.size(); ++i)
	{
		int fingerId = i < 5 ? 101 + i : 201 - 5 + i;
		CameraScanData tmpData;
		tmpData.data_id = fingerId;
		tmpData.data_path = vURLData.at(i);
		fingerData.push_back(tmpData);
	}

	OnCameraTaskFinished(task.devId, fingerData);
}

void ScanWorkThdImpl::HandleCancelTask()
{
	LOG_INFO(u8"执行取消任务处理逻辑");

	// 检查是否有超时的取消请求
	std::lock_guard<std::mutex> lock(m_cancelTasksMutex);
	auto currentTime = std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::system_clock::now().time_since_epoch()).count();

	for (auto it = m_pendingCancelTasks.begin(); it != m_pendingCancelTasks.end();) 
	{
		const std::string& deviceId = it->first;
		const std::string& taskId = it->second.first;

		// 这里可以添加超时检查逻辑
		// 例如：如果超过30秒没有收到回复，强制取消
		// 目前暂时保留，可以后续扩展
		LOG_INFO(QString(u8"待取消任务: Device=%1, TaskId=%2")
			.arg(QString::fromStdString(deviceId)).arg(QString::fromStdString(taskId)));
		++it;
	}
}

void ScanWorkThdImpl::OnCameraTaskCancelled(const std::string& deviceId, bool success)
{
	LOG_INFO(QString(u8"相机任务取消结果: Device=%1, Success=%2")
		.arg(QString::fromStdString(deviceId)).arg(success ? u8"成功" : u8"失败"));

	std::string taskId;
	bool bStopped = false;
	DeviceInfo deviceInfo;
	SyncBusinessTask taskStatus;
	bool shouldCallStatusCallback = false;
	bool shouldCallTaskCallback = false;

	// 获取并移除待取消的任务信息
	// 修复：使用统一锁顺序并减少锁的持有时间，避免死锁和重入问题
	{
		std::lock_guard<std::mutex> cancelLock(m_cancelTasksMutex);
		std::lock_guard<std::mutex> cameraLock(m_cameraTasksMutex);

		// 获取并移除待取消的任务信息
		auto cancelIt = m_pendingCancelTasks.find(deviceId);
		if (cancelIt != m_pendingCancelTasks.end()) 
		{
			taskId = cancelIt->second.first;
			bStopped = cancelIt->second.second;
			m_pendingCancelTasks.erase(cancelIt);
		}

		if (success) 
		{
			// 清理任务状态
			m_cameraRunningTasks.erase(deviceId);
			m_cameraTaskInfo.erase(deviceId);

			// 准备回调数据
			deviceInfo.devId = deviceId;
			deviceInfo.devType = 1;
			deviceInfo.devStatus = bStopped ? DeviceStatus::ERR : DeviceStatus::IDLE;
			shouldCallStatusCallback = true;

			if (!taskId.empty()) 
			{
				taskStatus.proId = taskId;
				taskStatus.op = "4";  // "4" 表示取消状态
				taskStatus.devId = deviceId;
				shouldCallTaskCallback = true;
			}
		}
		else 
		{
			// 取消失败，准备保持运行状态的回调数据
			if (!taskId.empty()) 
			{
				taskStatus.proId = taskId;
				taskStatus.op = "1";  // 保持运行状态
				taskStatus.devId = deviceId;
				shouldCallTaskCallback = true;
			}
		}
	}

	//修复：在锁外执行回调，避免重入风险和死锁
	if (success) 
	{
		if (shouldCallStatusCallback && m_deviceStatusCallBack) 
		{
			m_deviceStatusCallBack(deviceInfo);

			// 安全地更新设备映射状态
			UpdateDeviceMapStatus(deviceId, deviceInfo.devStatus);
		}

		if (shouldCallTaskCallback && m_taskStatusCallBack1) 
		{
			m_taskStatusCallBack1(taskStatus);
		}

		LOG_INFO(QString(u8"相机任务取消成功: Device=%1, TaskId=%2")
			.arg(QString::fromStdString(deviceId)).arg(QString::fromStdString(taskId)));
	}
	else 
	{
		LOG_ERROR(QString(u8"相机任务取消失败: Device=%1, TaskId=%2")
			.arg(QString::fromStdString(deviceId)).arg(QString::fromStdString(taskId)));

		if (shouldCallTaskCallback && m_taskStatusCallBack1) 
		{
			m_taskStatusCallBack1(taskStatus);
		}
	}

	if (success) 
	{
		// 取消成功，清理任务状态
		{
			std::lock_guard<std::mutex> lock(m_cameraTasksMutex);
			m_cameraRunningTasks.erase(deviceId);
			m_cameraTaskInfo.erase(deviceId);
		}

		// 更新设备状态
		if (m_deviceStatusCallBack) 
		{
			DeviceInfo deviceInfo;
			deviceInfo.devId = deviceId;
			deviceInfo.devType = 1; // 扫描设备类型
			deviceInfo.devStatus = bStopped ? DeviceStatus::ERR : DeviceStatus::IDLE;
			m_deviceStatusCallBack(deviceInfo);

			// 更新设备映射中的状态
			if (m_devicesMap.find(deviceId) != m_devicesMap.end()) 
			{
				if (bStopped)
				{
					UpdateDeviceMapStatus(deviceId, DeviceStatus::ERR);

				}
				else
				{
					UpdateDeviceMapStatus(deviceId, DeviceStatus::IDLE);
				}
				//m_devicesMap[deviceId]->SetStatus(bStopped ? DeviceStatus::ERR : DeviceStatus::IDLE);
			}
		}

		// 调用任务状态回调，通知取消成功
		if (m_taskStatusCallBack1 && !taskId.empty()) 
		{
			SyncBusinessTask taskStatus;
			taskStatus.proId = taskId;
			taskStatus.op = "4";  // "4" 表示取消状态
			taskStatus.devId = deviceId;
			m_taskStatusCallBack1(taskStatus);
		}

		LOG_INFO(QString(u8"相机任务取消成功: Device=%1, TaskId=%2")
			.arg(QString::fromStdString(deviceId)).arg(QString::fromStdString(taskId)));
	}
	else 
	{
		// 取消失败，保持原状态
		LOG_ERROR(QString(u8"相机任务取消失败: Device=%1, TaskId=%2")
			.arg(QString::fromStdString(deviceId)).arg(QString::fromStdString(taskId)));

		// 可以考虑重试或者强制取消
		if (m_taskStatusCallBack1 && !taskId.empty()) 
		{
			SyncBusinessTask taskStatus;
			taskStatus.proId = taskId;
			taskStatus.op = "1";  // 保持运行状态
			taskStatus.devId = deviceId;
			m_taskStatusCallBack1(taskStatus);
		}
	}
}

void ScanWorkThdImpl::CancelTaskOnDevice(const std::string& devId, bool bStopped)
{
	LOG_INFO(QString(u8"收到设备取消任务请求: Device=%1, Stopped=%2")
		.arg(QString::fromStdString(devId))
		.arg(bStopped ? u8"是" : u8"否"));

	// 检查是否是相机设备（真实设备
	if (IsCameraDevice(devId) && !g_simulateReturnData) 
	{
		// 获取当前运行的任务ID
		std::string taskId;
		{
			std::lock_guard<std::mutex> lock(m_cameraTasksMutex);
			auto it = m_cameraRunningTasks.find(devId);
			if (it != m_cameraRunningTasks.end()) 
			{
				taskId = it->second;
			}
		}

		if (!taskId.empty()) 
		{
			// 记录待取消的任务
			{
				std::lock_guard<std::mutex> lock(m_cancelTasksMutex);
				m_pendingCancelTasks[devId] = std::make_pair(taskId, bStopped);
			}

			// 发送取消命令到相机
			// 如果当前处于模拟状态，则将取消定时器任务推送至设备
			// 
			auto& cameraMgr = CameraTcpManager::GetInstance();
			bool sent = cameraMgr.SendTaskCancelToCamera(devId, taskId);

			if (!sent)
			{
				LOG_ERROR(QString(u8"发送任务取消命令失败: Device=%1, TaskId=%2")
					.arg(QString::fromStdString(devId))
					.arg(QString::fromStdString(taskId)));

				// 发送失败，直接处理为取消失败
				OnCameraTaskCancelled(devId, false);
			}
			else 
			{
				LOG_INFO(QString(u8"已发送任务取消命令: Device=%1, TaskId=%2")
					.arg(QString::fromStdString(devId))
					.arg(QString::fromStdString(taskId)));
			}
		}
		else 
		{
			LOG_INFO(QString(u8"设备当前没有运行的任务: %1").arg(QString::fromStdString(devId)));
			// 没有运行的任务，直接返回成功
			OnCameraTaskCancelled(devId, true);
		}
	}
	else if(g_simulateReturnData)
	{
		// 模拟设备，使用deviceType进行模拟取消
		// 非相机设备，使用原来的取消逻辑
		WorkThdBaseImpl::CancelTaskOnDevice(devId, bStopped);
	}
}

void ScanWorkThdImpl::UpdateDeviceMapStatus(const std::string& deviceId, DeviceStatus status)
{
	//使用基类的m_taskMtx保护m_devicesMap的访问，避免并发访问问题
	std::lock_guard<std::mutex> lock(m_taskMtx);
	auto it = m_devicesMap.find(deviceId);
	if (it != m_devicesMap.end()) 
	{
		it->second->SetStatus(status);
	}
}
