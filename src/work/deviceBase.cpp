#include "deviceBase.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <hv/hlog.h>

#include "global.h"
#include "CommFun.h"
#include "SingleOSSToken.h"


IODeviceBase::IODeviceBase(const std::string& devID, int devType)
	: m_pImpl(std::make_unique<IODeviceBaseImpl>(devID, devType))
{

}


std::string IODeviceBase::GetDeviceID() const
{
	return m_pImpl->GetDeviceID();
}

int IODeviceBase::GetDeviceType() const
{
	return m_pImpl->GetDeviceType();
}

DeviceStatus IODeviceBase::GetStatus() const
{
	return m_pImpl->GetStatus();
}

void IODeviceBase::SetStatus(DeviceStatus status)
{
	m_pImpl->SetStatus(status);
}

void IODeviceBase::SetProId(const std::string& proId)
{
	m_pImpl->SetProId(proId);
}

bool IODeviceBase::PerformTask(const BusinessTask& task)
{
	return m_pImpl->PerformTask(task);
}

bool IODeviceBase::PerformSimulateTask(const BusinessTask& task)
{
	return m_pImpl->PerformSimulateTask(task);
}

bool IODeviceBase::CancelTask(const BusinessTask& task)
{
	return m_pImpl->CancelTask(task);

}

void IODeviceBase::CancelCurrentTask(bool bStopped)
{
	m_pImpl->CancelCurrentTask(bStopped);
}

bool IODeviceBase::IsTaskCancelled() const
{
	return m_pImpl->IsTaskCancelled();
}

void IODeviceBase::RegStatusUpdateCallback(std::function<void(const DeviceInfo &)> callback)
{
	m_pImpl->RegStatusUpdateCallback(std::move(callback));
}


bool IODeviceBase::GetIsTaskCancel()
{
	return m_pImpl->GetIsTaskCancel();
}

void IODeviceBase::DevStopped(const std::string& task)
{
	m_pImpl->DevStopped(task);
}

SimulateData IODeviceBase::GetHandleWorkData()
{
	return m_pImpl->GetHandleWorkData();
}

//----------------------------IODeviceBaseImpl----------------------------------------------
//----------------------------IODeviceBaseImpl----------------------------------------------
//----------------------------IODeviceBaseImpl----------------------------------------------


//dev_type -->  global��������
IODeviceBaseImpl::IODeviceBaseImpl(const std::string& id, int dev_type)
	: m_devID(id)
	, m_devType(dev_type)
	, m_devStatus(DeviceStatus::IDLE)
	, m_taskCancelled(false)
{

}

std::string IODeviceBaseImpl::GetDeviceID() const
{
	return m_devID;
}

int IODeviceBaseImpl::GetDeviceType() const
{
	return m_devType;
}

DeviceStatus IODeviceBaseImpl::GetStatus() const
{
	return m_devStatus;
}

void IODeviceBaseImpl::SetStatus(DeviceStatus status)
{
	if (m_devStatus != status)
	{
		DeviceStatus oldStatus = m_devStatus;
		m_devStatus = status;
		//LOG_INFO("Device %s status changed from %d to %d", device_id_.c_str(), static_cast<int>(old_status), static_cast<int>(status_));
		//update_log
		//LOG_INFO();
		// ����Ӵ���״̬�ָ�������״̬����¼�ָ���־
		if (oldStatus == DeviceStatus::ERR && status == DeviceStatus::IDLE)
		{
			//LOG_INFO();
			LOG_INFO(QString("Device %1 recovered from error state").arg(QString::fromStdString(m_devID)));
		}
		if (m_StatusUpdateCB)
		{
			DeviceInfo devInfo;
			devInfo.devId = m_devID;
			devInfo.devType = m_devType;
			devInfo.devStatus = m_devStatus;
			m_StatusUpdateCB(devInfo);
		}

	}
}

void IODeviceBaseImpl::SetProId(const std::string& proId)
{
	//m_devID
}

//ִ������
bool IODeviceBaseImpl::PerformTask(const BusinessTask& task)
{
	//LOG_INFO("Device %s (Type %d) performing task %s: %s", m_devID.c_str(), m_devType, task.proId.c_str(), task.op.c_str());
	LOG_INFO(QString("Device %1 (Type %2) performing task %3: %4")
		.arg(QString::fromStdString(m_devID))
		.arg(m_devType)
		.arg(QString::fromStdString(task.proId))
		.arg(QString::fromStdString(task.op)));

	try 
	{
		// ����ȡ����־����¼��ǰ����ID
		{
			std::lock_guard<std::mutex> lock(m_cancelMtx);
			m_taskCancelled = false;
			m_currentTaskId = task.proId;
			m_curRunTask = task;
		}

		// ģ������ִ�У�ʵ���п����漰����IO����
		// ��ȡ�ж����ݴ���
		SetStatus(DeviceStatus::BUSY);
		// ʹ�ÿ��жϵ�˯����ģ�⹤����ʵ��Ӧ����Ӧ������ʵ���豸����
		auto start_time = std::chrono::steady_clock::now();
		auto target_duration = std::chrono::seconds(30);

		while (std::chrono::steady_clock::now() - start_time < target_duration) 
		{

			// ���ȡ�������ź�
			if (m_taskCancelled.load()) 
			{
				LOG_INFO(QString("Device %1 task %2 was cancelled.")
						.arg(QString::fromStdString(m_devID))
						.arg(QString::fromStdString(task.proId)));
				//�ǹ���״̬��ȡ����������״̬����Ϊ����

				if (m_devStatus != DeviceStatus::ERR)
				{
					SetStatus(DeviceStatus::IDLE);
				}
				{
					std::lock_guard<std::mutex> lock(m_cancelMtx);
					m_currentTaskId.clear();
					m_curRunTask.Clear();
				}
				return false; // ����ȡ��
			}

			// ʹ����������ʵ�ֿ��жϵĵȴ�
			std::unique_lock<std::mutex> lock(m_cancelMtx);
			m_cancelCV.wait_for(lock, std::chrono::milliseconds(100), [this] 
			{
				return m_taskCancelled.load();
			});

			if (m_taskCancelled.load()) 
			{
				LOG_INFO(QString("Device %1 task %2 was cancelled.")
						.arg(QString::fromStdString(m_devID))
						.arg(QString::fromStdString(task.proId)));
				if (m_devStatus != DeviceStatus::ERR)
				{
					SetStatus(DeviceStatus::IDLE);
				}
				m_currentTaskId.clear();
				m_curRunTask.Clear();
				return false;
			}

		}
		// �������
		// ͨ��tcp�źţ���ȡ��ǰ�豸״̬Ϊ����״̬�����豸״̬�����޸�
		SetStatus(DeviceStatus::IDLE);
		{
			std::lock_guard<std::mutex> lock(m_cancelMtx);
			m_currentTaskId.clear();
			m_curRunTask.Clear();
		}
		LOG_INFO(QString("Device %1 finished task %2.")
				.arg(QString::fromStdString(m_devID))
				.arg(QString::fromStdString(task.proId)));
		return true;
	}
	catch (const std::exception& e) 
	{
		LOG_INFO(QString("Device %1 failed to perform task %2: %3")
			.arg(QString::fromStdString(m_devID))
			.arg(QString::fromStdString(task.proId))
			.arg(QString::fromStdString(e.what())));
		SetStatus(DeviceStatus::ERR);
		{
			std::lock_guard<std::mutex> lock(m_cancelMtx);
			m_currentTaskId.clear();
		}
		return false;
	}
}

bool IODeviceBaseImpl::PerformSimulateTask(const BusinessTask& task)
{
		return false;
}

bool IODeviceBaseImpl::CancelTask(const BusinessTask& task)
{
	bool ret = false;
	if (GetStatus() != DeviceStatus::IDLE)
	{
		SetStatus(DeviceStatus::IDLE);
		m_curRunTask.Clear();
		ret = true;
	}
	return ret;
}

void IODeviceBaseImpl::CancelCurrentTask(bool bStopped)
{
	std::lock_guard<std::mutex> lock(m_cancelMtx);
	if (!m_currentTaskId.empty()) 
	{
		LOG_INFO(QString("Cancelling task %1 on device %2").arg(QString::fromStdString(m_currentTaskId).QString::fromStdString(m_devID)));
		m_taskCancelled = true;
		m_devStatus = bStopped ? DeviceStatus::ERR : m_devStatus;
		m_cancelCV.notify_all(); // ���ѵȴ��е�����ִ��
	}
	else 
	{
		LOG_INFO(QString("No active task to cancel on device %1").arg(QString::fromStdString(m_devID)));

	}
}

bool IODeviceBaseImpl::IsTaskCancelled() const
{
	return m_taskCancelled.load();

}

//��ͣ����
//ֹͣ����


void IODeviceBaseImpl::RegStatusUpdateCallback(std::function<void(const DeviceInfo &)> callback)
{
	m_StatusUpdateCB = std::move(callback);
}

bool IODeviceBaseImpl::GetIsTaskCancel()
{
	return m_taskCancelled;
}

void IODeviceBaseImpl::DevStopped(const std::string& task)
{

}

SimulateData IODeviceBaseImpl::GetHandleWorkData()
{
	return m_handleData;
}

