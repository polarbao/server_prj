#include "stickWork.h"
#include "global.h"
#include "SingleOSSToken.h"
#include "CommFun.h"


#include <QTimer>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonParseError>
#include <QMutexLocker>


//----------------------------StickWorkThd----------------------------------------------
//----------------------------StickWorkThd----------------------------------------------
//----------------------------StickWorkThd----------------------------------------------


StickWorkThd::StickWorkThd()
	: WorkThdBase("scan")
{
	//m_pImpl �� unique_ptr<IBusinessThread> ���ͣ�����ֱ������Impl
	this->m_pImpl = std::make_unique<StickWorkThdImpl>();
}

void StickWorkThd::AddDevice(const std::shared_ptr<DeviceStick>& dev)
{
	if (auto pImpl = dynamic_cast<StickWorkThdImpl*>(this->m_pImpl.get()))
	{
		pImpl->AddDevice(dev);
	}
	else
	{
		//dev_type_err;
		LOG_INFO("scan_work_thd::addDevice: p_impl_ is not of type scan_work_thd_dImpl. This indicates a logic error.");
	}
}


//----------------------------StickWorkThdImpl----------------------------------------------
//----------------------------StickWorkThdImpl----------------------------------------------
//----------------------------StickWorkThdImpl----------------------------------------------


StickWorkThdImpl::StickWorkThdImpl()
	: WorkThdBaseImpl("stick")
{

}

StickWorkThdImpl::~StickWorkThdImpl()
{

}

void StickWorkThdImpl::AddDevice(const std::shared_ptr<DeviceStick>& dev)
{
	m_devicesMap[dev->GetDeviceID()] = dev;
	dev->RegStatusUpdateCallback([this](const DeviceInfo& info)
	{
		OnDeviceStatusUpdate(info);
	});
	// ������󲢷�������
	UpdateMaxConcurrentTasks();
	LOG_INFO(QString("scan_work_thd: Added device %1").arg(dev->GetDeviceID().c_str()));

}

void StickWorkThdImpl::Run()
{
	while (m_bRunning)
	{
		// ��������ɵ�����
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


		// ��������е������Ҳ�������󲢷�����TODO�����������豸�������޸�
		while (!m_taskQueue.empty() && GetRunningTaskCount() < GetMaxConcurrentTasks())
		{
			BusinessTask curTask = m_taskQueue.front();
			SyncBusinessTask task;
			task.devId = curTask.devId;
			task.proId = curTask.proId;
			task.op = curTask.op;
			m_taskQueue.pop();
			lock.unlock();

			// ����豸�Ƿ�����ҿ���
			auto device_it = m_devicesMap.find(curTask.devId);
			if (device_it != m_devicesMap.end())
			{
				//LOG_INFO(QString("scan_work_thd processing task: %1 for device %2").arg(QString::fromStdString(curTask.proId)).arg(QString::fromStdString(curTask.devId)));
				// ����豸�Ƿ����
				if (device_it->second->GetStatus() == DeviceStatus::IDLE)
				{
					//LOG_INFO(QString("stick_work_thd processing task: %1 for device %2").arg(QString::fromStdString(curTask.proId)).arg(QString::fromStdString(curTask.devId)));
					//׼����ʼִ������ͬ��������ʼ������
					auto startOp = "1";
					// �첽ִ������
					SyncBusinessTask taskState;
					taskState.proId = curTask.proId;
					taskState.op = startOp;
					taskState.devId = curTask.devId;
					m_taskStatusCallBack1(taskState);
					ExecuteTaskAsync(curTask);
				}
				else
				{
					// �豸æµ�����·������
					lock.lock();
					m_taskQueue.push(curTask);
					lock.unlock();
					break; // �ȴ��´�ѭ��
				}
			}
			else
			{
				LOG_INFO(QString("scan_work_thd: Device %1 not found for task %2").arg(QString::fromStdString(curTask.devId)).arg(QString::fromStdString(curTask.proId)));
				if (m_taskStatusCallBack1)
				{
					m_taskStatusCallBack1(task);
				}
			}
			lock.lock();
		}
	}
	// ֹͣʱ�ȴ������������
	while (GetRunningTaskCount() > 0)
	{
		CleanupCompletedTasks();
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
}

void StickWorkThdImpl::ExecuteTaskAsync(const BusinessTask& task)
{
	std::lock_guard<std::mutex> lock(m_runningTasksMtx);
	// ��������ִ����Ϣ
	auto task_info = std::make_shared<RunningTaskInfo>(task);
	// �������������ִ���߳�
	// ���������д�����ж���ģ�����ݴ��������ʵ���ݴ���
	if (!g_simulateReturnData)
	{	//��ʵ����
		task_info->task_thread = std::make_shared<std::thread>(&StickWorkThdImpl::PerformTaskInThread, this, task);
	}
	else
	{	//ģ������
		task_info->task_thread = std::make_shared<std::thread>(&StickWorkThdImpl::PerformSimulateTaskInThread, this, task);
	}
	// ��ӵ��������е������б�
	m_runningTasks[task.proId] = task_info;

	LOG_INFO(QString("Started async task %1 for device %2 in %3 thread")
		.arg(QString::fromStdString(task.proId))
		.arg(QString::fromStdString(task.devId))
		.arg(QString::fromStdString(m_workType)));
}



void StickWorkThdImpl::PerformTaskInThread(const BusinessTask& task)
{
	LOG_INFO(QString(u8"����ҵ���̴߳�������: %1 for device %2")
		.arg(QString::fromStdString(task.proId))
		.arg(QString::fromStdString(task.devId)));

	//PerformSimulateTaskInThread(task);

	LOG_INFO(QString(u8"����ҵ���̴߳�������: %1 for device %2�����������������ط�������Ϣ��")
		.arg(QString::fromStdString(task.proId))
		.arg(QString::fromStdString(task.devId)));
}

void StickWorkThdImpl::PerformSimulateTaskInThread(const BusinessTask& inTask)
{
	try
	{
		LOG_INFO(QString(u8"����ҵ���̴߳�������_ģ������: %1 for device %2")
			.arg(QString::fromStdString(inTask.proId))
			.arg(QString::fromStdString(inTask.devId)));

		auto task = inTask;
		auto it = m_devicesMap.find(task.devId);
		auto bCancel = false;
		if (it != m_devicesMap.end())
		{
			// ģ���豸ִ������
			auto ret = it->second->PerformTask(task);
			bCancel = it->second->GetIsTaskCancel();
			if (ret)
			{
				LOG_INFO(QString(u8"����ҵ���̴߳����������_ģ������: %1 for device %2")
					.arg(QString::fromStdString(task.proId))
					.arg(QString::fromStdString(task.devId)));
			}
			else if (!ret && it->second->GetIsTaskCancel())
			{
				LOG_INFO(QString(u8"����ҵ���̴߳����������_ģ�����ݣ�����ȡ������op5: %1 for device %2")
					.arg(QString::fromStdString(task.proId))
					.arg(QString::fromStdString(task.devId)));
				// �жϵ�ǰ�豸�Ƿ�Ϊ����״̬�����򷵻��Ŷ�̬'4', ��Ϊȡ�������򷵻�'5'
				task.op = DeviceStatus::ERR == it->second->GetStatus() ? "4" : "5";
			}
		}
		OnStickTaskFinished(task.devId, task);


	}
	catch (const std::exception& e)
	{
		LOG_INFO(QString(u8"��������ִ���쳣: %1").arg(e.what()));
	}
}

void StickWorkThdImpl::HandleSimulateData(const BusinessTask& task)
{
	///*
	//1. ���Ƶ�ǰ�ļ���
	//2. ����ǰ�ļ���������
	//3. ��ȡ��ǰ�ļ���������
	//4. �����ļ���
	//*/
	//auto curTimerStr = QString::fromStdString(CommFun::GetInstance().GetCurrentTimeStr()) + "_" + QString::fromStdString(task.proId);
	//auto downFolderPath = QCoreApplication::applicationDirPath() + QDir::separator() + QString("simulate_scan_data") + QDir::separator() + \
	//	QString("down_file") + QDir::separator() + curTimerStr;
	//std::vector<std::string> vURLData;
	//std::string retInfo;
	//// ��ȡԴ�ļ�����������Ŀ���ļ������ļ��У�
	//auto ret = SingleOSSToken::GetInstance().DownloadSingleFile(task.data, retInfo, downFolderPath.toStdString());

	//// ����һ���Զ�ʱ�����ӳ� 20000ms��20�룩��ִ�в���
	//int delayMs = 20000;
	//QTimer::singleShot(delayMs, [ret]()
	//{
	//	LOG_INFO(QString(u8"����ҵ���̶߳�ʱ�����ڣ�����������ļ��������������Ϊ%1").arg(ret));
	//});
	//
	//LOG_INFO(QString(u8"����ҵ���̴߳�������_ģ������:: %1 for device %2�����������������ط�������Ϣ��")
	//	.arg(QString::fromStdString(task.proId))
	//	.arg(QString::fromStdString(task.devId)));
}

void StickWorkThdImpl::OnStickConnected(const std::string& deviceId)
{

}

void StickWorkThdImpl::OnStickDisconnected(const std::string& deviceId)
{

}

void StickWorkThdImpl::OnStickTaskFinished(const std::string& deviceId, const BusinessTask& task)
{
	LOG_INFO(QString(u8"�����������: Device=%1")
		.arg(QString::fromStdString(deviceId)));

	// ��ȡ�������е�����ID
	std::string taskId;
	{
		std::lock_guard<std::mutex> lock(m_stickTasksMutex);
		auto it = m_stickRunningTasks.find(deviceId);
		if (it != m_stickRunningTasks.end())
		{
			taskId = it->second;
			m_stickRunningTasks.erase(it);
		}
	}

	// �ص��������״̬
	if (m_taskStatusCallBack1)
	{
		SyncBusinessTask taskStatus;
		taskStatus.proId = task.proId;
		// �ж��Ƿ�Ϊȡ������������򱣳�ԭ��op����
		taskStatus.op = task.op == "1" ? "3" : task.op;
		taskStatus.devId = task.devId;
		//������ݽ����ϴ���������ҹ�������		
		m_taskStatusCallBack1(taskStatus); // 3��ʾ���
	}
}

void StickWorkThdImpl::OnStickTaskError(const std::string& deviceId, const std::string& errorMsg)
{
	LOG_INFO(QString(u8"�����������: Device=%1, Error=%2")
		.arg(QString::fromStdString(deviceId)).arg(QString::fromStdString(errorMsg)));

	// ��ȡ�������е�����ID
	std::string taskId;
	{
		std::lock_guard<std::mutex> lock(m_stickTasksMutex);
		auto it = m_stickRunningTasks.find(deviceId);
		if (it != m_stickRunningTasks.end()) {
			taskId = it->second;
			m_stickRunningTasks.erase(it);
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
			m_taskStatusCallBack1(taskStatus); // 4��ʾ��ֹ�������½����Ŷ�̬
		}

		// �����豸״̬Ϊ����
		if (m_deviceStatusCallBack)
		{
			//DeviceInfo deviceInfo;
			//deviceInfo.devId = deviceId;
			//deviceInfo.devType = 2; // �����豸����
			//deviceInfo.devStatus = DeviceStatus::ERR; // ����״̬
			//m_deviceStatusCallBack(deviceInfo);

			// 1120_�����豸״̬������������豸map
			// ����״̬ʱ�������豸ʱ��ͬ����UI
			UpdateDeviceMapStatus(deviceId, DeviceStatus::ERR);
		}
	}
}

void StickWorkThdImpl::UpdateDeviceMapStatus(const std::string& deviceId, DeviceStatus status)
{
	//ʹ�û����m_taskMtx����m_devicesMap�ķ��ʣ����Ⲣ����������
	std::lock_guard<std::mutex> lock(m_taskMtx);
	auto it = m_devicesMap.find(deviceId);
	if (it != m_devicesMap.end())
	{
		it->second->SetStatus(status);
	}
}
