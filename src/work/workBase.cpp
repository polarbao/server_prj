#include "workBase.h"

#include <QTimer>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonParseError>
#include <QMutexLocker>

#include "global.h"
#include <stdexcept>


WorkThdBase::WorkThdBase(const std::string& work_type)
	: m_pImpl(nullptr)
{

}

void WorkThdBase::Start()
{
	if (m_pImpl)
	{
		m_pImpl->Start();
	}
}

void WorkThdBase::Stop()
{
	if (m_pImpl)
	{
		m_pImpl->Stop();
	}

}

void WorkThdBase::AddTask(const BusinessTask& task)
{
	if (m_pImpl)
	{
		m_pImpl->AddTask(task);
	}
}

bool WorkThdBase::HasDevice(const std::string& devId) const
{
	return m_pImpl ? m_pImpl->HasDevice(devId) : false;
}

bool WorkThdBase::RemoveDevice(const std::string&devId)
{
	return m_pImpl ? m_pImpl->RemoveDevice(devId) : false;
}

void WorkThdBase::CancelTaskOnDevice(const std::string& devId, bool bStopped/*=false*/)
{
	if (m_pImpl)
	{
		m_pImpl->CancelTaskOnDevice(devId, bStopped);
	}
}

void WorkThdBase::CancelAllTasks()
{
	if (m_pImpl)
	{
		m_pImpl->CancelAllTasks();
	}
}

std::string WorkThdBase::GetWorkType() const
{
	if (m_pImpl)
	{
		return m_pImpl->GetWorkType();
	}
	return "";
}

std::vector <DeviceInfo> WorkThdBase::GetDeviceStatus() const
{
	if (m_pImpl)
	{
		return m_pImpl->GetDeviceStatus();
	}
	return {};
}

//void WorkThdBase::RegisterTaskStatusCallBack(std::function<void(const std::string&, const std::string&, const std::string&)> callback)
//{
//	if (m_pImpl)
//	{
//		m_pImpl->RegisterTaskStatusCallBack(std::move(callback));
//	}
//}

void WorkThdBase::RegisterTaskStatusCallBack1(std::function<void(const SyncBusinessTask&)> callback)
{
	if (m_pImpl)
	{
		m_pImpl->RegisterTaskStatusCallBack1(std::move(callback));
	}
}

void WorkThdBase::RegisterDeviceStatusCallBack(std::function<void(const DeviceInfo&)> callback)
{
	if (m_pImpl)
	{
		m_pImpl->RegisterDeviceStatusCallBack(std::move(callback));
	}
}



//----------------------------WorkThdBaseImpl----------------------------------------------
//----------------------------WorkThdBaseImpl----------------------------------------------
//----------------------------WorkThdBaseImpl----------------------------------------------


WorkThdBaseImpl::WorkThdBaseImpl(const std::string& work_type)//, QObject *par)
	: IWorkThd()
	, m_workType(work_type)
	, m_bRunning(false)
	, m_maxConcurrentTasks(1)
{
	
}

WorkThdBaseImpl::~WorkThdBaseImpl()
{
	Stop();

	// ȷ�����������̶߳������
	std::lock_guard<std::mutex> lock(m_runningTasksMtx);
	for (auto& pair : m_runningTasks) 
	{
		if (pair.second->task_thread && pair.second->task_thread->joinable()) 
		{
			pair.second->task_thread->join();
		}
	}
	m_runningTasks.clear();
}

void WorkThdBaseImpl::Start()
{
	if (!m_bRunning)
	{
		//m_bRunning.store(true);
		m_bRunning = true;
		m_thd = std::thread(&WorkThdBaseImpl::Run, this);
		LOG_INFO(QString("Business thread %1 started.").arg(m_workType.c_str()));

	}
}

void WorkThdBaseImpl::Stop()
{
	if (m_bRunning)
	{
		m_bRunning = false;
		m_taskCV.notify_one();
		if (m_thd.joinable())
		{
			m_thd.join();
		}
		LOG_INFO(QString("Business thread %1 stopped.").arg(QString::fromStdString(m_workType)));

	}
}

void WorkThdBaseImpl::AddTask(const BusinessTask& task)
{
	std::lock_guard<std::mutex> locker(m_taskMtx);
	m_taskQueue.push(task);
	m_taskCV.notify_one();   
	LOG_INFO(QString("Added task %1 to %2 business thread.")
		.arg(QString::fromStdString(task.proId))
		.arg(QString::fromStdString(m_workType)));

}



bool WorkThdBaseImpl::HasDevice(const std::string& devId) const
{
	auto ret = m_devicesMap.find(devId) != m_devicesMap.end();
	return ret;
}

bool WorkThdBaseImpl::RemoveDevice(const std::string&devId)
{
	std::lock_guard<std::mutex> lock(m_taskMtx);
	auto it = m_devicesMap.find(devId);
	if (it != m_devicesMap.end())
	{
		//Note���жϵ�ǰ�豸�Ƿ����У�ȡ����ǰ����
		it->second->CancelCurrentTask(false);
		m_devicesMap.erase(it);
		LOG_INFO(QString("work_thd_moudle thd_base_unit, remoce_cur_dev, dev_id = %1").arg(QString::fromStdString(devId)));
		return true;
	}
	return false;
}

void WorkThdBaseImpl::CancelTaskOnDevice(const std::string& devId, bool bStopped/*=false*/)
{
	auto it = m_devicesMap.find(devId);
	if (it != m_devicesMap.end()) 
	{
		it->second->CancelCurrentTask(bStopped);
		LOG_INFO(QString("Cancelled task on device %1 in %2 business thread.")
			.arg(QString::fromStdString(devId))
			.arg(QString::fromStdString(m_workType)));
	}
	else 
	{
		LOG_INFO(QString("Device %1 not found in %2 business thread.")
			.arg(devId.c_str())
			.arg(m_workType.c_str()));

	}
}

void WorkThdBaseImpl::CancelAllTasks() 
{
	for (const auto& pair : m_devicesMap) 
	{
		pair.second->CancelCurrentTask(false);
	}
	LOG_INFO(QString("Cancelled all tasks in %1 business thread.").arg(QString::fromStdString(m_workType)));

}

std::string WorkThdBaseImpl::GetWorkType() const
{
	return m_workType;
}

std::vector<DeviceInfo> WorkThdBaseImpl::GetDeviceStatus() const
{
	std::vector<DeviceInfo> devVec;
	for (auto& itDev : m_devicesMap)
	{
		DeviceInfo info;
		info.devId = itDev.second->GetDeviceID();
		info.devType = itDev.second->GetDeviceType();
		info.devStatus = itDev.second->GetStatus();
		devVec.push_back(info);
	}
	return devVec;
}

//void WorkThdBaseImpl::RegisterTaskStatusCallBack(std::function<void(const std::string&, const std::string&, const std::string&)> callback)
//{
//	m_taskStatusCallBack = std::move(callback);
//}

void WorkThdBaseImpl::RegisterTaskStatusCallBack1(std::function<void(const SyncBusinessTask&)> callback)
{
	m_taskStatusCallBack1 = std::move(callback);
}

void WorkThdBaseImpl::RegisterDeviceStatusCallBack(std::function<void(const DeviceInfo &)> callback)
{
	m_deviceStatusCallBack = std::move(callback);
}

void WorkThdBaseImpl::Run()
{
	while (m_bRunning)
	{


	}
}

void WorkThdBaseImpl::OnDeviceStatusUpdate(const DeviceInfo& info)
{
	if (m_deviceStatusCallBack)
	{
		m_deviceStatusCallBack(info);
	}
}

void WorkThdBaseImpl::ExecuteTaskAsync(const BusinessTask& taskParam)
{
	auto task = taskParam;
	std::lock_guard<std::mutex> lock(m_runningTasksMtx);

	// ��������ִ����Ϣ
	auto task_info = std::make_shared<RunningTaskInfo>(task);

	// �������������ִ���߳�
	task_info->task_thread = std::make_shared<std::thread>(&WorkThdBaseImpl::PerformTaskInThread, this, task);

	// ��ӵ��������е������б�
	m_runningTasks[task.proId] = task_info;

	LOG_INFO(QString("Started async task %1 for device %2 in %3 thread")
		.arg(QString::fromStdString(task.proId))
		.arg(QString::fromStdString(task.devId))
		.arg(QString::fromStdString(m_workType)));
}

void WorkThdBaseImpl::PerformTaskInThread(const BusinessTask& task)
{
	LOG_INFO(QString("%1 thread processing task: %2 for device %3")
		.arg(QString::fromStdString(m_workType))
		.arg(QString::fromStdString(task.proId))
		.arg(QString::fromStdString(task.devId)));

	/*
		//opΪ1ʱ����������������opRes�ϱ�Ϊ3
		//opΪ1ʱ���豸���ϣ�opRes�ϱ�Ϊ4

		//opΪ2ʱ��ȡ������opRes�ϱ�Ϊ5
	*/
	if (m_taskStatusCallBack1) 
	{
		//m_taskStatusCallBack(task.proId, TaskStatus::IN_PROGRESS, task.devId);
		SyncBusinessTask tmpTask; 
		tmpTask.proId = task.proId;
		tmpTask.devId = task.devId;
		tmpTask.op = task.op;
		m_taskStatusCallBack1(tmpTask);

	}
	std::string opRes;
	auto it = m_devicesMap.find(task.devId);
	if (it != m_devicesMap.end())
	{
		bool taskSuccess = false;
		if (!g_simulateReturnData)
		{
			taskSuccess = it->second->PerformTask(task); // ִ������
		} 
		else
		{
			taskSuccess = it->second->PerformSimulateTask(task); // ִ������

		}
		if (m_taskStatusCallBack1) 
		{

			TaskStatus finalStatus = taskSuccess ? TaskStatus::COMPLETED : TaskStatus::CANCELLED;
			//�豸���״̬�ı�->����̬
			//0926: ȡ������ʱ���ж��豸�Ƿ�Ϊ����״̬����������豸״̬ͬ�������������豸����״̬
			if (it->second->GetStatus() != DeviceStatus::ERR)
			{
				it->second->SetStatus(DeviceStatus::IDLE);
			}
			else
			{
				DeviceInfo dev;
				dev.devId = it->second->GetDeviceID();
				dev.devType = it->second->GetDeviceType();
				dev.devStatus = it->second->GetStatus();
				m_deviceStatusCallBack(dev);
			}
			//0911 finalStatus -> op(����������� 1��ʼ 2���� 3ȡ����Ϊ�����������Ŷ� 4��ֹ����
			//�������ִ��
			if (task.op == "2" && it->second->GetIsTaskCancel())
			{
				opRes = "5";
			}
			else if (task.op == "1")
			{
				//opΪ1ʱ����������������opRes�ϱ�Ϊ3
				//opΪ1ʱ���豸���ϣ�opRes�ϱ�Ϊ4
				if (it->second->GetIsTaskCancel())
				{
					opRes = "4";
				}
				else
				{
					opRes = "3";
				}
				//opRes = it->second->GetIsTaskCancel() ��"4": "3";
			}
			//������ɣ����ͻظ�����
			//m_taskStatusCallBack(task.proId, TaskStatus::COMPLETED, task.devId);
			SyncBusinessTask tmpTask;
			tmpTask.proId = task.proId;
			tmpTask.devId = task.devId;
			tmpTask.op = opRes;
			m_taskStatusCallBack1(tmpTask);
		}
	}
	else 
	{
		//hloge("%s thread: Device %s not found for task %s",	business_type_.c_str(), task.deviceId.c_str(), task.taskId.c_str());
		if (m_taskStatusCallBack1) 
		{
			opRes = "4";
			//m_taskStatusCallBack(task.proId, TaskStatus::CANCELLED, task.devId);
			SyncBusinessTask tmpTask;
			tmpTask.proId = task.proId;
			tmpTask.devId = task.devId;
			tmpTask.op = opRes;
			m_taskStatusCallBack1(tmpTask);
		}
	}

	// ����������
	std::lock_guard<std::mutex> lock(m_runningTasksMtx);
	auto task_it = m_runningTasks.find(task.proId);
	if (task_it != m_runningTasks.end()) 
	{
		task_it->second->is_running = false;
	}
}

void WorkThdBaseImpl::PerformSimulateTaskInThread(const BusinessTask& task)
{
	LOG_INFO(QString("%1 thread processing task: %2 for device %3")
		.arg(QString::fromStdString(m_workType))
		.arg(QString::fromStdString(task.proId))
		.arg(QString::fromStdString(task.devId)));

	/*
		//opΪ1ʱ����������������opRes�ϱ�Ϊ3
		//opΪ1ʱ���豸���ϣ�opRes�ϱ�Ϊ4

		//opΪ2ʱ��ȡ������opRes�ϱ�Ϊ5
	*/
	if (m_taskStatusCallBack1)
	{
		//m_taskStatusCallBack(task.proId, TaskStatus::IN_PROGRESS, task.devId);
		SyncBusinessTask tmpTask;
		tmpTask.proId = task.proId;
		tmpTask.devId = task.devId;
		tmpTask.op = task.op;
		m_taskStatusCallBack1(tmpTask);

	}
	std::string opRes;
	auto it = m_devicesMap.find(task.devId);
	if (it != m_devicesMap.end())
	{
		bool taskSuccess = false;
		if (!g_simulateReturnData)
		{
			taskSuccess = it->second->PerformTask(task); // ִ������
		}
		else
		{
			taskSuccess = it->second->PerformSimulateTask(task); // ִ������

		}
		if (m_taskStatusCallBack1)
		{

			TaskStatus finalStatus = taskSuccess ? TaskStatus::COMPLETED : TaskStatus::CANCELLED;
			//�豸���״̬�ı�->����̬
			//0926: ȡ������ʱ���ж��豸�Ƿ�Ϊ����״̬����������豸״̬ͬ�������������豸����״̬
			if (it->second->GetStatus() != DeviceStatus::ERR)
			{
				it->second->SetStatus(DeviceStatus::IDLE);
			}
			else
			{
				DeviceInfo dev;
				dev.devId = it->second->GetDeviceID();
				dev.devType = it->second->GetDeviceType();
				dev.devStatus = it->second->GetStatus();
				m_deviceStatusCallBack(dev);
			}
			//0911 finalStatus -> op(����������� 1��ʼ 2���� 3ȡ����Ϊ�����������Ŷ� 4��ֹ����
			//�������ִ��
			if (task.op == "2" && it->second->GetIsTaskCancel())
			{
				opRes = "5";
			}
			else if (task.op == "1")
			{
				//opΪ1ʱ����������������opRes�ϱ�Ϊ3
				//opΪ1ʱ���豸���ϣ�opRes�ϱ�Ϊ4
				if (it->second->GetIsTaskCancel())
				{
					opRes = "4";
				}
				else
				{
					opRes = "3";
				}
				//opRes = it->second->GetIsTaskCancel() ��"4": "3";
			}
			//������ɣ����ͻظ�����
			//m_taskStatusCallBack(task.proId, TaskStatus::COMPLETED, task.devId);
			SyncBusinessTask tmpTask;
			tmpTask.proId = task.proId;
			tmpTask.devId = task.devId;
			tmpTask.op = opRes;
			m_taskStatusCallBack1(tmpTask);
		}
	}
	else
	{
		//hloge("%s thread: Device %s not found for task %s",	business_type_.c_str(), task.deviceId.c_str(), task.taskId.c_str());
		if (m_taskStatusCallBack1)
		{
			opRes = "4";
			//m_taskStatusCallBack(task.proId, TaskStatus::CANCELLED, task.devId);
			SyncBusinessTask tmpTask;
			tmpTask.proId = task.proId;
			tmpTask.devId = task.devId;
			tmpTask.op = opRes;
			m_taskStatusCallBack1(tmpTask);
		}
	}

	// ����������
	std::lock_guard<std::mutex> lock(m_runningTasksMtx);
	auto task_it = m_runningTasks.find(task.proId);
	if (task_it != m_runningTasks.end())
	{
		task_it->second->is_running = false;
	}
}

void WorkThdBaseImpl::CleanupCompletedTasks()
{
	std::lock_guard<std::mutex> lock(m_runningTasksMtx);

	auto it = m_runningTasks.begin();
	while (it != m_runningTasks.end()) 
	{
		if (!it->second->is_running && it->second->task_thread->joinable()) 
		{
			it->second->task_thread->join();
			it = m_runningTasks.erase(it);
		}
		else 
		{
			++it;
		}
	}
}

size_t WorkThdBaseImpl::GetRunningTaskCount()
{
	std::lock_guard<std::mutex> lock(m_runningTasksMtx);
	return m_runningTasks.size();
}

size_t WorkThdBaseImpl::GetMaxConcurrentTasks() const
{
	return m_maxConcurrentTasks;
}

void WorkThdBaseImpl::UpdateMaxConcurrentTasks()
{
	m_maxConcurrentTasks = m_devicesMap.size();

	LOG_INFO(QString("%1 thread: Updated max concurrent tasks to %2 (based on %3 devices)")
		.arg(QString::fromStdString(m_workType))
		.arg(QString::number(m_maxConcurrentTasks))
		.arg(QString::number(m_devicesMap.size())));
}
