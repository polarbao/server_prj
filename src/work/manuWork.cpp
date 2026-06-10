#include "manuWork.h"
#include "global.h"
#include "CommFun.h"
#include "SingleOSSToken.h"


#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonParseError>
#include <QMutexLocker>


//----------------------------ManuWorkThd----------------------------------------------
//----------------------------ManuWorkThd----------------------------------------------
//----------------------------ManuWorkThd----------------------------------------------



ManuWorkThd::ManuWorkThd()
	: WorkThdBase("manu")
{
	//m_pImpl 是 unique_ptr<IBusinessThread> 类型，可以直接派生Impl
	this->m_pImpl = std::make_unique<ManuWorkThdImpl>();
}

void ManuWorkThd::AddDevice(const std::shared_ptr<DeviceManu>& dev)
{
	if (auto pImpl = dynamic_cast<ManuWorkThdImpl*>(this->m_pImpl.get()))
	{
		pImpl->AddDevice(dev);
	}
	else
	{
		//dev_type_err;
		LOG_INFO("manu_work_thd::addDevice: p_impl_ is not of type manu_work_thd_dImpl. This indicates a logic error.");
	}
}


//----------------------------ManuWorkThdImpl----------------------------------------------
//----------------------------ManuWorkThdImpl----------------------------------------------
//----------------------------ManuWorkThdImpl----------------------------------------------
											 											 
ManuWorkThdImpl::ManuWorkThdImpl()
	: WorkThdBaseImpl("manu")
{

}

ManuWorkThdImpl::~ManuWorkThdImpl()
{

}

void ManuWorkThdImpl::AddDevice(const std::shared_ptr<DeviceManu>& dev)
{
	m_devicesMap[dev->GetDeviceID()] = dev;
	dev->RegStatusUpdateCallback([this](const DeviceInfo& info)
	{
		OnDeviceStatusUpdate(info);
	});
	// 更新最大并发任务数
	UpdateMaxConcurrentTasks(); 
	LOG_INFO(QString("manu_work_thd: Added device %1").arg(dev->GetDeviceID().c_str()));

}

void ManuWorkThdImpl::Run()
{
	while (m_bRunning)
	{
		// 清理已完成的任务
		CleanupCompletedTasks();

		std::unique_lock<std::mutex> lock(m_taskMtx);
		m_taskCV.wait_for(lock, std::chrono::milliseconds(100), [this]
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
				//LOG_INFO(QString("manu_work_thd processing task: %1 for device %2").arg(QString::fromStdString(curTask.proId)).arg(QString::fromStdString(curTask.devId)));
				// 检查设备是否空闲
				if (device_it->second->GetStatus() == DeviceStatus::IDLE)
				{
					//LOG_INFO(QString("manu_work_thd processing task: %1 for device %2").arg(QString::fromStdString(curTask.proId)).arg(QString::fromStdString(curTask.devId)));
					//准备开始执行任务，同步发生开始任务报文
					auto startOp = "1";
					SyncBusinessTask taskState;
					taskState.proId = curTask.proId;
					taskState.op = startOp;
					taskState.devId = curTask.devId;
					m_taskStatusCallBack1(taskState);
					// 异步执行任务
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
				LOG_INFO(QString("manu_work_thd: Device %1 not found for task %2").arg(QString::fromStdString(curTask.devId)).arg(QString::fromStdString(curTask.proId)));
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

void ManuWorkThdImpl::ExecuteTaskAsync(const BusinessTask& task)
{
	std::lock_guard<std::mutex> lock(m_runningTasksMtx);
	// 创建任务执行信息
	auto task_info = std::make_shared<RunningTaskInfo>(task);
	// 创建并启动任务执行线程
	// 针对任务进行处理，判断是模拟数据处理还是真实数据处理
	if (!g_simulateReturnData)
	{	//真实数据
		task_info->task_thread = std::make_shared<std::thread>(&ManuWorkThdImpl::PerformSimulateTaskInThread, this, task);
	}
	else
	{	//模拟数据
		task_info->task_thread = std::make_shared<std::thread>(&ManuWorkThdImpl::PerformSimulateTaskInThread, this, task);
	}
	// 添加到正在运行的任务列表
	m_runningTasks[task.proId] = task_info;

	LOG_INFO(QString("Started async task %1 for device %2 in %3 thread")
		.arg(QString::fromStdString(task.proId))
		.arg(QString::fromStdString(task.devId))
		.arg(QString::fromStdString(m_workType)));
}



void ManuWorkThdImpl::PerformTaskInThread(const BusinessTask& task)
{
	LOG_INFO(QString(u8"制作业务线程处理任务: %1 for device %2")
		.arg(QString::fromStdString(task.proId))
		.arg(QString::fromStdString(task.devId)));
	
	
	LOG_INFO(QString(u8"制作业务线程处理任务: %1 for device %2，完成任务操作，返回服务器信息。")
		.arg(QString::fromStdString(task.proId))
		.arg(QString::fromStdString(task.devId)));
	
}

void ManuWorkThdImpl::PerformSimulateTaskInThread(const BusinessTask& inTask)
{
	// 其中包含开始任务处理报文，将任务信息发送至相机软件进行后续处理
	try
	{
		LOG_INFO(QString(u8"制作业务线程处理任务_模拟数据: %1 for device %2")
			.arg(QString::fromStdString(inTask.proId))
			.arg(QString::fromStdString(inTask.devId)));
										
		auto task = inTask;
		std::vector<CameraScanData> fingerData;
		auto it = m_devicesMap.find(task.devId);
		auto bCancel = false;
		if (it != m_devicesMap.end())
		{
			// 模拟设备执行任务
			auto ret = it->second->PerformTask(task);
			bCancel = it->second->GetIsTaskCancel();
			if (ret)
			{
				LOG_INFO(QString(u8"制作业务线程处理任务完成_模拟数据: %1 for device %2")
					.arg(QString::fromStdString(task.proId))
					.arg(QString::fromStdString(task.devId)));
			}
			else if(!ret && it->second->GetIsTaskCancel())
			{
				LOG_INFO(QString(u8"制作业务线程处理任务取消_模拟数据，服务单取消返回op5: %1 for device %2")
					.arg(QString::fromStdString(task.proId))
					.arg(QString::fromStdString(task.devId)));
				// 判断当前设备是否为故障状态，是则返回排队态'4', 若为取消任务则返回'5'
				task.op = DeviceStatus::ERR == it->second->GetStatus() ? "4" : "5";
			}
		}
		OnManuTaskFinished(task.devId, task);

		// 1119_ban 原有操作逻辑，直接复制本地数据进行上传处理
		// 调用基类的实现处理本地设备
		// WorkThdBaseImpl::PerformTaskInThread(task);

	}
	catch (const std::exception& e)
	{
		LOG_INFO(QString(u8"制作任务执行异常: %1").arg(e.what()));
	}
}


void ManuWorkThdImpl::HandleSimulateData(const BusinessTask& task)
{
	/*
	1. 复制当前文件夹
	2. 将当前文件夹重命名
	3. 提取当前文件夹中数据
	4. 调用文件夹
	*/
	auto curTimerStr = QString::fromStdString(CommFun::GetInstance().GetCurrentTimeStr()) + "_" +  QString::fromStdString(task.proId);
	auto downFolderPath = QCoreApplication::applicationDirPath() + QDir::separator() + QString("simulate_scan_data") + QDir::separator() + \
		QString("down_file")+ QDir::separator() + curTimerStr;
	std::vector<std::string> vURLData;
	std::string retInfo;
	// 获取源文件夹中所有条目（文件和子文件夹）
	//auto ret = SingleOSSToken::GetInstance().DownloadSingleFile(task.data, retInfo, downFolderPath.toStdString());
	bool ret = false;
	// 设置一次性定时器：延迟 20000ms（20秒）后执行操作
	int delayMs = 20000;
	QTimer::singleShot(delayMs, [ret]() 
	{
		LOG_INFO(QString(u8"制作业务线程定时器到期，完成下载文件操作，操作结果%1").arg(ret));
	});
	OnManuTaskFinished(task.devId, task);
	LOG_INFO(QString(u8"制作业务线程处理任务_模拟数据:: %1 for device %2，完成任务操作，返回服务器信息。")
		.arg(QString::fromStdString(task.proId))
		.arg(QString::fromStdString(task.devId)));
}

void ManuWorkThdImpl::OnManuConnected(const std::string& deviceId)
{

}

void ManuWorkThdImpl::OnManuDisconnected(const std::string& deviceId)
{

}

void ManuWorkThdImpl::OnManuTaskFinished(const std::string& deviceId, const BusinessTask& task)
{
	LOG_INFO(QString(u8"制作任务完成: Device=%1")
		.arg(QString::fromStdString(deviceId)));

	// 获取正在运行的任务ID
	std::string taskId;
	{
		std::lock_guard<std::mutex> lock(m_manuTasksMutex);
		auto it = m_manuRunningTasks.find(deviceId);
		if (it != m_manuRunningTasks.end())
		{
			taskId = it->second;
			m_manuRunningTasks.erase(it);
		}
	}

	// 回调任务完成状态
	if (m_taskStatusCallBack1)
	{
		SyncBusinessTask taskStatus;
		taskStatus.proId = task.proId;
		// 判断是否为取消任务，如果是则保持原来op数据
		taskStatus.op = task.op == "1" ? "3" : task.op;
		taskStatus.devId = task.devId;
		//针对数据进行上传处理，并且构造数据		
		m_taskStatusCallBack1(taskStatus); // 3表示完成
	}
	//-------------------------------------------

	//// 更新设备状态为空闲
	//if (m_deviceStatusCallBack)
	//{
	//	DeviceInfo deviceInfo;
	//	deviceInfo.devId = deviceId;
	//	deviceInfo.devType = 2; // 制作设备类型
	//	deviceInfo.devStatus = DeviceStatus::IDLE; // 空闲状态
	//	m_deviceStatusCallBack(deviceInfo);

	//	//1120_更新设备状态到工控软件中设备map
	//	//UpdateDeviceMapStatus(deviceId, DeviceStatus::IDLE);
	//}
}

void ManuWorkThdImpl::OnManuTaskError(const std::string& deviceId, const std::string& errorMsg)
{
	LOG_INFO(QString(u8"打印机任务错误: Device=%1, Error=%2")
		.arg(QString::fromStdString(deviceId)).arg(QString::fromStdString(errorMsg)));

	// 获取正在运行的任务ID
	std::string taskId;
	{
		std::lock_guard<std::mutex> lock(m_manuTasksMutex);
		auto it = m_manuRunningTasks.find(deviceId);
		if (it != m_manuRunningTasks.end()) {
			taskId = it->second;
			m_manuRunningTasks.erase(it);
		}
	}

	if (!taskId.empty()) 
	{
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
			//DeviceInfo deviceInfo;
			//deviceInfo.devId = deviceId;
			//deviceInfo.devType = 2; // 制作设备类型
			//deviceInfo.devStatus = DeviceStatus::ERR; // 错误状态
			//m_deviceStatusCallBack(deviceInfo);

			// 1120_更新设备状态到工控软件中设备map
			// 错误状态时，设置设备时会同步至UI
			UpdateDeviceMapStatus(deviceId, DeviceStatus::ERR);
		}
	}
}

void ManuWorkThdImpl::UpdateDeviceMapStatus(const std::string& deviceId, DeviceStatus status)
{
	//使用基类的m_taskMtx保护m_devicesMap的访问，避免并发访问问题
	std::lock_guard<std::mutex> lock(m_taskMtx);
	auto it = m_devicesMap.find(deviceId);
	if (it != m_devicesMap.end())
	{
		it->second->SetStatus(status);
	}
}
