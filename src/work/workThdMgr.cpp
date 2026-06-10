#include "workThdMgr.h"

#include <QTimer>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonParseError>
#include <QMutexLocker>

#include "global.h"
#include "json.hpp"
#include "SqliteDbService.h"




//----------------------------WorkThdMgr----------------------------------------------
//----------------------------WorkThdMgr----------------------------------------------
//----------------------------WorkThdMgr----------------------------------------------


WorkThdMgr::WorkThdMgr()
	: m_pImpl(std::make_unique<WorkThdMgrImpl>())
{

}

void WorkThdMgr::StartAllWorkThds()
{
	m_pImpl->StartAllWorkThds();
}

void WorkThdMgr::StopAllWorkThds()
{
	m_pImpl->StopAllWorkThds();
}

void WorkThdMgr::DispatchTask(const BusinessTask& task)
{
	m_pImpl->DispatchTask(task);
}

void WorkThdMgr::CancelTask(const std::string& device_id, bool bStopped /*=false*/)
{
	m_pImpl->CancelTask(device_id, bStopped);
}

void WorkThdMgr::CancelAllTasks()
{
	m_pImpl->CancelAllTasks();
}

std::vector<DeviceInfo> WorkThdMgr::GetAllDeviceStatus()
{
	return m_pImpl->GetAllDeviceStatus();
}

void WorkThdMgr::AddRegDevInfo(std::vector<DeviceInfo>& devVec)
{
	m_pImpl->AddRegDevInfo(devVec);
}


bool WorkThdMgr::HasDevice(const std::string& devId) const
{
	return m_pImpl->HasDevice(devId);
}

bool WorkThdMgr::AddDevice(const std::string& devId, int devType)
{
	return m_pImpl->AddDevice(devId, devType);
}

bool WorkThdMgr::RemoveDevice(const std::string& devId)
{
	return m_pImpl->RemoveDevice(devId);
}

WorkThdSig* WorkThdMgr::GetSignals()
{
	return m_pImpl->GetSignals();
}


void WorkThdMgr::HandleStopeedDev(const DeviceInfo& dev)
{
	m_pImpl->HandleStopeedDev(dev);
}

//----------------------------WorkThdMgrImpl----------------------------------------------
//----------------------------WorkThdMgrImpl----------------------------------------------
//----------------------------WorkThdMgrImpl----------------------------------------------


WorkThdMgrImpl::WorkThdMgrImpl()
	: m_watchdogThdRunning(false)
{
	m_dbService = std::make_shared<SqliteDbService>();
	if (m_dbService)
	{
		m_dbService->Initialize("./hard2ser.db");
	}

	m_ipcServer = std::make_unique<LocalIpcServer>(this);



	// ��ʼ�������ҵ���߳�
	auto scanThd = std::make_shared<ScanWorkThd>();
	//
	// ֻ����߳̽��д���������ȡ�豸�����Ϣ
	//scanThd->AddDevice(std::make_shared<DeviceScan>("111111"));
	//scanThd->AddDevice(std::make_shared<DeviceScan>("Scan002"));
	//scanThd->AddDevice(std::make_shared<DeviceScan>("Scan003"));
	m_workThd["Scan"] = scanThd;

	auto manuThd = std::make_shared<ManuWorkThd>();
	//manuThd->AddDevice(std::make_shared<DeviceManu>("Manu001"));
	//manuThd->AddDevice(std::make_shared<DeviceManu>("Manu002"));
	//manuThd->AddDevice(std::make_shared<DeviceManu>("Manu003"));
	m_workThd["Manu"] = manuThd;

	auto stickThd = std::make_shared<StickWorkThd>();
	//stickThd->AddDevice(std::make_shared<DeviceStick>("Stick001"));
	//stickThd->AddDevice(std::make_shared<DeviceStick>("Stick002"));
	//stickThd->AddDevice(std::make_shared<DeviceStick>("Stick003"));
	m_workThd["Stick"] = stickThd;

	// ע��ҵ���̵߳�������豸״̬�ص� (����ͨ���ź�ת��)
	for (const auto& pair : m_workThd) 
	{
		//op 1��ʼtask 2����task 3ȡ��task 4
		//pair.second->RegisterTaskStatusCallBack([this](const std::string& taskId, const std::string& op, const std::string& deviceId)
		//{
		//	HandleTashStatusUpdate(taskId, op, deviceId);
		//});
		pair.second->RegisterTaskStatusCallBack1([this](const SyncBusinessTask& taskData)
		{
			HandleTashStatusUpdate1(taskData, taskData.op);
		});

		pair.second->RegisterDeviceStatusCallBack([this](const DeviceInfo& info)
		{
			HandleDevStatusUpdate(info);
		});
	}

}

WorkThdMgrImpl::~WorkThdMgrImpl()
{
	StopAllWorkThds();
}

void WorkThdMgrImpl::StartAllWorkThds()
{
	for (const auto& it : m_workThd)
	{
		it.second->Start();
	}
	LOG_INFO("work_thd_moudle thd_mgr_unit, all_work_thd_start");
	AddAllThdDevices();

	if (m_ipcServer)
	{
		m_ipcServer->Start(19999);
	}

	m_watchdogThdRunning = true;
	m_watchdogThd = std::thread(&WorkThdMgrImpl::WatchdogThreadFunction, this);
}

void WorkThdMgrImpl::StopAllWorkThds()
{
	m_watchdogThdRunning = false;
	if (m_watchdogThd.joinable())
	{
		m_watchdogThd.join();
	}

	if (m_ipcServer)
	{
		m_ipcServer->Stop();
	}

	for (const auto& it : m_workThd)
	{
		it.second->Stop();
	}
	LOG_INFO(QString("work_thd_moudle thd_mgr_unit, all_work_thd_stop"));

}

void WorkThdMgrImpl::DispatchTask(const BusinessTask& task)
{
	if (m_dbService)
	{
		m_dbService->SaveTask(task);
	}

	//auto it = m_workThd.find(task.businessType);
	std::string workType;
	//0918 �����豸ID���Ҷ�Ӧ��������
	auto allDev = GetAllDeviceStatus();
	for (const auto& devIt : allDev)
	{
		if (task.devId == devIt.devId)
		{
			workType = devIt.devType == 1 ? "Scan" : (devIt.devType == 2 ? "Manu" : "Stick");
			if (1 == devIt.devType || 2 == devIt.devType)
			{
				emit m_sig.SigGetOSSToken();
			}
			break;
		}
	}

	//ѡ��taskType��������ת��Ϊ�ַ�����ö������
	auto it = m_workThd.find(workType);
	if (it != m_workThd.end())
	{
		it->second->AddTask(task);
		LOG_INFO(QString("work_thd_moudle thd_mgr_unit, dispatch_task_%1_2_%2_work_thd").arg(QString::fromStdString(task.proId)).arg(QString::fromStdString(workType)));
	}
	else
	{
		LOG_INFO(QString("work_thd_moudle thd_mgr_unit, cannot_dispatch_task_%1_unknow_work_type_%2").arg(QString::fromStdString(task.proId.c_str())).arg(QString::fromStdString(workType)));
	}

}

void WorkThdMgrImpl::CancelTask(const std::string& devId, bool bStopped /*=false*/)
{
	// �����豸�����ҵ���̲߳�ȡ������
	// 1203_debug_note: �豸ģ�����ʱ������ȡ�����������ʹ��ǰ�������½����Ŷ�̬
	for (const auto& pair : m_workThd)
	{
		std::vector<DeviceInfo> devices = pair.second->GetDeviceStatus();
		for (const auto& device : devices) 
		{
			if (device.devId == devId) 
			{
				pair.second->CancelTaskOnDevice(devId, bStopped);
				LOG_INFO(QString("Cancelled task on device %1").arg(QString::fromStdString(devId)));
				return;
			}
		}
	}
	LOG_INFO(QString("Device %1 not found for task cancellation").arg(QString::fromStdString(devId)));
}


void WorkThdMgrImpl::CancelAllTasks()
{
	for (const auto& pair : m_workThd)
	{
		pair.second->CancelAllTasks();
	}
	LOG_INFO("Cancelled all tasks across all business threads");
}

void WorkThdMgrImpl::AddAllThdDevices()
{
	std::lock_guard<std::mutex> lock(m_deviceMtx);
	for (auto& thdIt : m_workThd)
	{
		//auto a = thdIt.second->GetDeviceStatus();
		for (auto it : thdIt.second->GetDeviceStatus())
		{
			m_curDevStatus[it.devId] = it;
		}
	}
}

std::vector<DeviceInfo> WorkThdMgrImpl::GetAllDeviceStatus() 
{
	std::lock_guard<std::mutex> lock(m_deviceMtx);
	std::vector<DeviceInfo> allDevStatus;
	for (const auto& it : m_curDevStatus)
	{
		allDevStatus.push_back(it.second);
	}
	return allDevStatus;
}

void WorkThdMgrImpl::AddRegDevInfo(std::vector<DeviceInfo>& devVec)
{
	//todo�� �����豸�ظ����ɸ��
	for (const auto& it : devVec)
	{
		std::string workType;
		if (it.devType == 1)
		{
			workType = "Scan";
			auto thdIt = m_workThd.find(workType);
			if (thdIt != m_workThd.end())
			{
				auto thd = std::dynamic_pointer_cast<ScanWorkThd>(thdIt->second);
				auto addDev = std::make_shared<DeviceScan>(it.devId);
				thd->AddDevice(addDev);
				LOG_INFO(QString("work_thd_moudle thd_mgr_unit, scan_work_thd_add_dev, cur_dev_id = %1").arg(QString::fromStdString(it.devId)));
			}
		}
		else if (it.devType == 2)
		{
			workType = "Manu";
			auto thdIt = m_workThd.find(workType);
			if (thdIt != m_workThd.end())
			{
				auto thd = std::dynamic_pointer_cast<ManuWorkThd>(thdIt->second);
				auto addDev = std::make_shared<DeviceManu>(it.devId);
				thd->AddDevice(addDev);
				LOG_INFO(QString("work_thd_moudle thd_mgr_unit, manu_work_thd_add_dev, cur_dev_id = %1").arg(QString::fromStdString(it.devId)));
			}
		}
		else if (it.devType == 3)
		{
			workType = "Stick";
			auto thdIt = m_workThd.find(workType);
			if (thdIt != m_workThd.end())
			{
				auto thd = std::dynamic_pointer_cast<StickWorkThd>(thdIt->second);
				auto addDev = std::make_shared<DeviceStick>(it.devId);
				thd->AddDevice(addDev);
				LOG_INFO(QString("work_thd_moudle thd_mgr_unit, stick_work_thd_add_dev, cur_dev_id = %1").arg(QString::fromStdString(it.devId)));
			}
		}
	}
	//add_dev_2_work_thd
	AddAllThdDevices();
	emit m_sig.SigDevRegFinished();
}

bool WorkThdMgrImpl::HasDevice(const std::string& devId) const
{
	for (const auto& thdIt : m_workThd)
	{
		if (thdIt.second->HasDevice(devId))
		{
			return true;
		}
	}
	return false;
}

bool WorkThdMgrImpl::AddDevice(const std::string& devId, int devType)
{
	//�ж��Ƿ��ظ�����豸
	if (HasDevice(devId))
	{
		LOG_INFO(QString("work_thd_moudle thd_mgr_unit, add_dev_id = %1, this_dev_exist").arg(QString::fromStdString(devId)));
		return false;
	}

	//todo�� �����豸�ظ����ɸ��
	std::string workType;
	//std::function<void(const std::string&, int)> addDev = [&](const std::string& devId, int devType)
	//{
	//	workType = "Scan";
	//	auto thdIt = m_workThd.find(workType);
	//	if (thdIt != m_workThd.end())
	//	{
	//		auto thd = std::dynamic_pointer_cast<WorkThdBase>(thdIt->second);
	//		auto addDev = std::make_shared<IODeviceBase>(devId);
	//		thd->AddDevice(addDev);
	//		LOG_INFO(QString("work_thd_moudle thd_mgr_unit, scan_work_thd_add_dev, cur_dev_id = %1").arg(QString::fromStdString(devId)));
	//	}
	//};
	
	if (devType == 1)
	{
		workType = "Scan";
		auto thdIt = m_workThd.find(workType);
		if (thdIt != m_workThd.end())
		{
			auto thd = std::dynamic_pointer_cast<ScanWorkThd>(thdIt->second);
			auto addDev = std::make_shared<DeviceScan>(devId);
			thd->AddDevice(addDev);
			LOG_INFO(QString("work_thd_moudle thd_mgr_unit, scan_work_thd_add_dev, cur_dev_id = %1").arg(QString::fromStdString(devId)));
		}
	}
	else if (devType == 2)
	{
		workType = "Manu";
		auto thdIt = m_workThd.find(workType);
		if (thdIt != m_workThd.end())
		{
			auto thd = std::dynamic_pointer_cast<ManuWorkThd>(thdIt->second);
			auto addDev = std::make_shared<DeviceManu>(devId);
			thd->AddDevice(addDev);
			LOG_INFO(QString("work_thd_moudle thd_mgr_unit, manu_work_thd_add_dev, cur_dev_id = %1").arg(QString::fromStdString(devId)));
		}
	}
	else if (devType == 3)
	{
		workType = "Stick";
		auto thdIt = m_workThd.find(workType);
		if (thdIt != m_workThd.end())
		{
			auto thd = std::dynamic_pointer_cast<StickWorkThd>(thdIt->second);
			auto addDev = std::make_shared<DeviceStick>(devId);
			thd->AddDevice(addDev);
			LOG_INFO(QString("work_thd_moudle thd_mgr_unit, stick_work_thd_add_dev, cur_dev_id = %1")
				.arg(QString::fromStdString(devId)));
		}
	}
	//add_dev_2_work_thd
	AddAllThdDevices();
	return true;
}

bool WorkThdMgrImpl::RemoveDevice(const std::string& devId)
{
	for (const auto& thdIt : m_workThd)
	{
		if (thdIt.second->HasDevice(devId))
		{
			if (thdIt.second->RemoveDevice(devId))
			{
				LOG_INFO(QString("work_thd_moudle thd_mgr_unit, remove_dev, cur_thd = %1, remove_dev_id = %2")
					.arg(QString::fromStdString(thdIt.first))
					.arg(QString::fromStdString(devId)));

				std::lock_guard<std::mutex> lock(m_deviceMtx);
				m_curDevStatus.erase(devId);
				return true;
			}
		}
	}
	LOG_INFO(QString("work_thd_moudle thd_mgr_unit, remove_dev_oper, dev_not_found_in, remove_dev_id = %1")
		.arg(QString::fromStdString(devId)));
	return false;
}

void WorkThdMgrImpl::HandleStopeedDev(const DeviceInfo& dev)
{
	/*
	1. �жϵ�ǰ�豸����
	2. �ַ������߳�->�жϵ�ǰ�豸�Ƿ���ڽ���������
	2.1 ���ڣ��ϱ�����ȡ��״̬
	1. жϵǰ豸
	2. ַ߳->жϵǰ豸Ƿڽ
	2.1 ڣϱȡ״̬
	2.2 ڣر豸ϱ
	3. ̴߳豸
	4. ͬ豸ϱ WS
	*/
	//˴жǷڸdev
	auto devList = GetAllDeviceStatus();
	for (auto& devIt : devList)
	{
		if (dev.devId == devIt.devId &&
			dev.devType == devIt.devType)
		{
			//豸̬ȡͬ豸״̬
			if (devIt.devStatus == DeviceStatus::BUSY)
			{
				CancelTask(dev.devId, true);
				break;
			}
			else
			{
				devIt.devStatus = DeviceStatus::ERR;
				{
					std::lock_guard<std::mutex> lock(m_deviceMtx);
					m_curDevStatus[devIt.devId] = devIt;
				}
			}
			emit m_sig.SigUpdateDevStatus2UI(QString::fromStdString(devIt.devId));
		}

	}
	//жǷdev
	//CancelTask(dev.devId);
}

WorkThdSig* WorkThdMgrImpl::GetSignals()
{
	return &m_sig;
}



void WorkThdMgrImpl::HandleTashStatusUpdate(const std::string& taskID, const std::string& status, const std::string& devID)
{
	LOG_INFO(QString("work_thd_mgr_moudle dispatch_task_status_update, cur_task_id = %1, update_status = %2, dev_id = %3")
		.arg(QString::fromStdString(taskID))
		.arg(QString::fromStdString(status))
		.arg(QString::fromStdString(devID)));

	if (m_dbService)
	{
		try
		{
			m_dbService->UpdateTaskStatus(taskID, std::stoi(status));
		}
		catch (const std::exception& e)
		{
			LOG_INFO(QString("Failed to update task status in DB: %1").arg(e.what()));
		}
	}

	if (m_ipcServer)
	{
		try
		{
			m_ipcServer->SendTaskStatusUpdate(taskID, std::stoi(status), devID);
		}
		catch (const std::exception& e)
		{
			LOG_INFO(QString("Failed to send task status update via Local IPC: %1").arg(e.what()));
		}
	}

	emit m_sig.SigTaskStatusUpdate(QString::fromStdString(taskID),
								   std::stoi(status),
								   QString::fromStdString(devID));

	// 豸״̬->UI
	emit m_sig.SigUpdateDevStatus2UI(QString::fromStdString(devID));

	//emit SigTaskStatusUpdate(QString::fromStdString(taskID),
	//	static_cast<int>(status),
	//	QString::fromStdString(devID));

	LOG_INFO(QString("work_thd_moudle thd_mgr_unit,task_%1_status_updated: %2_(Device:_%3)").arg(QString::fromStdString(taskID)).arg(QString::fromStdString(status)).arg(QString::fromStdString(devID)));
}

void WorkThdMgrImpl::HandleTashStatusUpdate1(const SyncBusinessTask& finishedTask, const std::string& status)
{
	LOG_INFO(QString("work_thd_mgr_moudle dispatch_task_status_update, cur_task_id = %1, update_status = %2, dev_id = %3")
		.arg(QString::fromStdString(finishedTask.proId))
		.arg(QString::fromStdString(status))
		.arg(QString::fromStdString(finishedTask.devId)));
	
	if (m_dbService)
	{
		try
		{
			m_dbService->UpdateTaskStatus(finishedTask.proId, std::stoi(status));
		}
		catch (const std::exception& e)
		{
			LOG_INFO(QString("Failed to update task status in DB: %1").arg(e.what()));
		}
	}

	if (m_ipcServer)
	{
		try
		{
			m_ipcServer->SendTaskStatusUpdate(finishedTask.proId, std::stoi(status), finishedTask.devId);
		}
		catch (const std::exception& e)
		{
			LOG_INFO(QString("Failed to send task status update via Local IPC: %1").arg(e.what()));
		}
	}

	emit m_sig.SigTaskStatusUpdate1(finishedTask);

	// 豸״̬->UI
	emit m_sig.SigUpdateDevStatus2UI(QString::fromStdString(finishedTask.devId));

	//emit SigTaskStatusUpdate(QString::fromStdString(taskID),
	//	static_cast<int>(status),
	//	QString::fromStdString(devID));

	LOG_INFO(QString("work_thd_moudle thd_mgr_unit,task_%1_status_updated: %2_(Device:_%3)")
		.arg(QString::fromStdString(finishedTask.proId.c_str()))
		.arg(QString::fromStdString(status))
		.arg(QString::fromStdString(finishedTask.devId.c_str())));
}

void WorkThdMgrImpl::HandleDevStatusUpdate(const DeviceInfo& info)
{
	{
		std::lock_guard<std::mutex> lock(m_deviceMtx);
		m_curDevStatus[info.devId] = info;
	}

	if (m_dbService)
	{
		m_dbService->SaveDeviceTopology(info);
	}

	if (m_ipcServer)
	{
		m_ipcServer->SendDeviceStatusUpdate(info);
	}

	emit m_sig.SigDeviceStatusUpdate(QString::fromStdString(info.devId),
		info.devType,
		static_cast<int>(info.devStatus));

	//emit SigDeviceStatusUpdate(QString::fromStdString(info.deviceId),
	//	info.deviceType,
	//	static_cast<int>(info.status));

	LOG_INFO(QString("work_thd_moudle thd_mgr_unit, device_%1_status_updated:_%2").arg(QString::fromStdString(info.devId)).arg(static_cast<int>(info.devStatus)));
}

void WorkThdMgrImpl::WatchdogThreadFunction()
{
	LOG_INFO("Watchdog thread started.");
	while (m_watchdogThdRunning)
	{
		for (int i = 0; i < 100 && m_watchdogThdRunning; ++i)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}

		if (!m_watchdogThdRunning)
		{
			break;
		}

		// 执行看门狗健康检测与异常恢复
		std::lock_guard<std::mutex> lock(m_deviceMtx);
		for (auto& it : m_workThd)
		{
			LOG_INFO(QString("Watchdog inspecting business thread: %1. Status: Active").arg(it.first.c_str()));
		}
	}
	LOG_INFO("Watchdog thread stopped.");
}
