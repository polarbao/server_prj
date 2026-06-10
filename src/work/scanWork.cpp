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
	//m_pImpl �� unique_ptr<IBusinessThread> ���ͣ�����ֱ������Impl
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
	//��ʼ������豸ID���ϣ��ɸ��������ļ������ݿ����ã�
	m_cameraDeviceIds.insert("100001"); // ����豸ID

	////// �������TCP�������ź�
	//auto cameraMgrSig = CameraTcpManager::GetInstance().GetSignals();
	//CameraTcpManager::GetInstance().StartServer(); 
	////auto sig = CameraTcpManager::GetInstance();
	////connect(&CameraTcpManager::GetInstance(), &CameraTcpManager::SigTest, this, &ScanWorkThdImpl::OnCameraConnected, Qt::QueuedConnection);

	//connect(cameraMgrSig, &CameraTcpManagerSignals::SigCameraConnected, this, &ScanWorkThdImpl::OnCameraConnected, Qt::QueuedConnection);
	//connect(cameraMgrSig, &CameraTcpManagerSignals::SigCameraDisconnected, this, &ScanWorkThdImpl::OnCameraDisconnected, Qt::QueuedConnection);
	//connect(cameraMgrSig, &CameraTcpManagerSignals::SigCameraTaskFinished, this, &ScanWorkThdImpl::OnCameraTaskFinished, Qt::QueuedConnection);
	//connect(cameraMgrSig, &CameraTcpManagerSignals::SigCameraTaskError, this, &ScanWorkThdImpl::OnCameraTaskError, Qt::QueuedConnection);
	//LOG_INFO(QString(u8"ɨ��ҵ���߳����������TCP������"));


	// �� ʹ�ûص�����ע������¼�����
	auto& cameraMgr = CameraTcpManager::GetInstance();
	cameraMgr.StartServer();

	// ע��ص����� - ʹ��lambda���ʽ����thisָ��
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

	// ע���������ȡ���ص�
	cameraMgr.RegisterCameraTaskCancelledCallback([this](const std::string& deviceId, bool success)
	{
		this->OnCameraTaskCancelled(deviceId, success);
	});

	LOG_INFO(QString(u8"ɨ��ҵ���߳�ע�����TCP�������ص��������"));
}

ScanWorkThdImpl::~ScanWorkThdImpl()
{
	// �����������״̬
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
	// ������󲢷�������
	UpdateMaxConcurrentTasks();
	LOG_INFO(QString(u8"scan_work_thd: Added device %1").arg(dev->GetDeviceID().c_str()));
}

//op(����������� 1��ʼ 3���� 4ȡ�����񣬽��������Ŷ� 5��ֹ����
void ScanWorkThdImpl::Run()
{
	while (m_bRunning)
	{
		// ��������ɵ�����
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

				// ����豸�Ƿ����
				if (device_it->second->GetStatus() == DeviceStatus::IDLE) 
				{
					//LOG_INFO(QString("scan_work_thd processing task: %1 for device %2").arg(QString::fromStdString(curTask.proId)).arg(QString::fromStdString(curTask.devId)));
					//׼����ʼִ������ͬ��������ʼ������
					auto startOp = "1";
					task.op = startOp;
					m_taskStatusCallBack1(task);
					// ִ�������ھ��������ظ��ݱ�־λ���ݣ��ж�����ʵ���ݻ���ģ������
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
				LOG_INFO(QString(u8"scan_work_thd: Device %1 not found for task %2").arg(QString::fromStdString(curTask.devId)).arg(QString::fromStdString(curTask.proId)));
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

void ScanWorkThdImpl::ExecuteTaskAsync(const BusinessTask& task)
{
	std::lock_guard<std::mutex> lock(m_runningTasksMtx);
	// ��������ִ����Ϣ
	auto task_info = std::make_shared<RunningTaskInfo>(task);
	// �������������ִ���߳�
	// ���������д�����ж���ģ�����ݴ��������ʵ���ݴ���
	if (!g_simulateReturnData)
	{	//��ʵ����
		task_info->task_thread = std::make_shared<std::thread>(&ScanWorkThdImpl::PerformTaskInThread, this, task);
	}
	else
	{	//ģ������
		task_info->task_thread = std::make_shared<std::thread>(&ScanWorkThdImpl::PerformSimulateTaskInThread, this, task);
	}
	// ��ӵ��������е������б�
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
		LOG_INFO(QString(u8"ɨ��ҵ���̴߳�������: %1 for device %2")
			.arg(QString::fromStdString(task.proId))
			.arg(QString::fromStdString(task.devId)));
		auto a = m_devicesMap;

		// �жϵ�ǰ�����Ƿ�Ϊȡ�����������ȡ��������֪ͨ��Ӧ������н���ȡ������

		// ����Ƿ�Ϊ����豸
		if (IsCameraDevice(task.devId)) 
		{
			LOG_INFO(QString(u8"��⵽����豸����: Device=%1, Task=%2")
				.arg(QString::fromStdString(task.devId))
				.arg(QString::fromStdString(task.proId)));

			// ������������豸
			if (SendTaskToCamera(task)) 
			{
				LOG_INFO(QString(u8"�����ѷ��͵�����豸: %1").arg(QString::fromStdString(task.devId)));
			}
			else 
			{
				LOG_INFO(QString(u8"������������豸ʧ��: %1").arg(QString::fromStdString(task.devId)));
				if (m_taskStatusCallBack1)
				{
					SyncBusinessTask taskStatus;
					taskStatus.proId = task.proId;
					taskStatus.op = "4";
					taskStatus.devId = task.devId;
					m_taskStatusCallBack1(taskStatus); // 4��ʾ��ֹ����
				}
			}
			return;
		}
		// ���û����ʵ�ִ�������豸
		//WorkThdBaseImpl::PerformTaskInThread(task);
	}
	catch (const std::exception& e) 
	{
		LOG_INFO(QString(u8"ɨ������ִ���쳣: %1").arg(e.what()));
	}
}

void ScanWorkThdImpl::PerformSimulateTaskInThread(const BusinessTask& inTask)
{
	// ���а�����ʼ��������ģ���������Ϣ���������������к�������
	try
	{
		auto task = inTask;
		LOG_INFO(QString(u8"ɨ��ҵ���̴߳�������_ģ������: %1 for device %2")
			.arg(QString::fromStdString(task.proId))
			.arg(QString::fromStdString(task.devId)));

		// ģ�����ݴ����߼� 
		SetCameraTaskRunning(task.devId, task.proId, true);
		
		// �����豸״̬Ϊæµͬ���������(���豸�н��и���Ϊæµ״̬
		//1120_�����豸״̬������������豸map
		//if (m_deviceStatusCallBack)
		//{
		//	DeviceInfo deviceInfo;
		//	deviceInfo.devId = task.devId;
		//	deviceInfo.devType = 1; // ɨ���豸����
		//	deviceInfo.devStatus = DeviceStatus::BUSY; // æµ״̬
		//	m_deviceStatusCallBack(deviceInfo);
		//}

		std::vector<CameraScanData> fingerData;
		auto it = m_devicesMap.find(task.devId);
		auto bCancel = false;
		if (it != m_devicesMap.end())
		{
			// ģ���豸ִ������
			auto ret = it->second->PerformTask(task); 
			bCancel = it->second->GetIsTaskCancel();
			if (ret)
			//if (it->second->PerformTask(task))
			{
				LOG_INFO(QString(u8"ɨ��ҵ���̴߳����������_ģ������: %1 for device %2")
					.arg(QString::fromStdString(task.proId))
					.arg(QString::fromStdString(task.devId)));
				fingerData = it->second->GetHandleWorkData().scanData;
			}
			else if (!ret && it->second->GetIsTaskCancel())
			{
				LOG_INFO(QString(u8"ɨ��ҵ���̴߳�������ȡ��_ģ�����ݣ�����ȡ������op5: %1 for device %2")
					.arg(QString::fromStdString(task.proId))
					.arg(QString::fromStdString(task.devId)));
				// �жϵ�ǰ�豸�Ƿ�Ϊ����״̬�����򷵻��Ŷ�̬'4', ��Ϊȡ�������򷵻�'5'
				task.op = DeviceStatus::ERR == it->second->GetStatus() ? "4" : "5";
			}
		}
		OnCameraTaskFinished(task.devId, fingerData, task);
	}
	catch (const std::exception& e) 
	{
		LOG_INFO(QString(u8"ɨ������ִ���쳣: %1").arg(e.what()));
	}
}

// ���TCP�������¼�����
void ScanWorkThdImpl::OnCameraConnected(const std::string& deviceId)
{
	LOG_INFO(QString(u8"����豸����: %1").arg(QString::fromStdString(deviceId)));

	// �����豸״̬Ϊ����
	if (m_deviceStatusCallBack) 
	{
		DeviceInfo deviceInfo;
		deviceInfo.devId = deviceId;
		deviceInfo.devType = 1; // ɨ���豸����
		deviceInfo.devStatus = DeviceStatus::IDLE; // ����״̬
		m_deviceStatusCallBack(deviceInfo);
		//1120_�����豸״̬������������豸map
		UpdateDeviceMapStatus(deviceId, DeviceStatus::IDLE);
	}
}


void ScanWorkThdImpl::OnCameraDisconnected(const std::string& deviceId)
{
	LOG_INFO(QString(u8"����豸�Ͽ�: %1").arg(QString::fromStdString(deviceId)));

	// ������豸����������
	{
		std::lock_guard<std::mutex> lock(m_cameraTasksMutex);
		auto it = m_cameraRunningTasks.find(deviceId);
		if (it != m_cameraRunningTasks.end()) {
			std::string taskId = it->second;
			m_cameraRunningTasks.erase(it);

			//// �ص�����ʧ��״̬
			if (m_taskStatusCallBack1)
			{
				SyncBusinessTask taskStatus;
				taskStatus.proId = taskId;
				taskStatus.op = "4";
				taskStatus.devId = deviceId;
				m_taskStatusCallBack1(taskStatus); // 4��ʾ��ֹ����
			}
		}
	}

	// �����豸״̬Ϊ����
	if (m_deviceStatusCallBack) 
	{
		DeviceInfo deviceInfo;
		deviceInfo.devId = deviceId;
		deviceInfo.devType = 1; // ɨ���豸����
		deviceInfo.devStatus = DeviceStatus::OFFLINE; // ����״̬
		m_deviceStatusCallBack(deviceInfo);
		//1120_�����豸״̬������������豸map
		UpdateDeviceMapStatus(deviceId, DeviceStatus::OFFLINE);
	}
}


void ScanWorkThdImpl::OnCameraTaskFinished(const std::string& deviceId,
										   const std::vector<CameraScanData>& scanData, 
										   const BusinessTask& task)
{
	LOG_INFO(QString(u8"����������: Device=%1, ��������=%2")
		.arg(QString::fromStdString(deviceId)).arg(scanData.size()));

	// ��ȡ�������е�����ID
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
	//��ȷ�߼�
	//if (!taskId.empty())
	if (taskId.empty() || !taskId.empty())
	{
		// ����ɨ��������
		std::vector<FingerScanData> processData;
		ProcessCameraScanData1(scanData, processData);
		// �ص��������״̬
		if (m_taskStatusCallBack1)
		{
			SyncBusinessTask taskStatus;
			taskStatus.proId = taskId;
			//ͨ��m_cameraTaskInfo[deviceId]��ѯ��ǰ�豸
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
			//������ݽ����ϴ���������ҹ�������		
			m_taskStatusCallBack1(taskStatus); // 3��ʾ���
		}
		//-------------------------------------------

		// �����豸״̬Ϊ����
		// �豸״̬��Dev���Ѿ�����
		if (m_deviceStatusCallBack) 
		{
			//DeviceInfo deviceInfo;
			//deviceInfo.devId = deviceId;
			//deviceInfo.devType = 1; // ɨ���豸����
			//deviceInfo.devStatus = DeviceStatus::IDLE; // ����״̬
			//m_deviceStatusCallBack(deviceInfo);
			//1120_�����豸״̬������������豸map
			//UpdateDeviceMapStatus(deviceId, DeviceStatus::IDLE);
		}
	}
}

void ScanWorkThdImpl::OnCameraTaskError(const std::string& deviceId, const std::string& errorMsg)
{
	LOG_INFO(QString(u8"����������: Device=%1, Error=%2")
		.arg(QString::fromStdString(deviceId)).arg(QString::fromStdString(errorMsg)));

	// ��ȡ�������е�����ID
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
		// �ص�����ʧ��״̬
		//if (m_taskStatusCallBack) 
		//{
		//	m_taskStatusCallBack(taskId, "4", deviceId); // 4��ʾ��ֹ����
		//}
		if (m_taskStatusCallBack1)
		{
			SyncBusinessTask taskStatus;
			taskStatus.proId = taskId;
			taskStatus.op = "4";
			taskStatus.devId = deviceId;
			m_taskStatusCallBack1(taskStatus); // 4��ʾ��ֹ����
		}

		// �����豸״̬Ϊ����
		if (m_deviceStatusCallBack) 
		{
			DeviceInfo deviceInfo;
			deviceInfo.devId = deviceId;
			deviceInfo.devType = 1; // ɨ���豸����
			deviceInfo.devStatus = DeviceStatus::ERR; // ����״̬
			m_deviceStatusCallBack(deviceInfo);

			// 1120δͬ�������������豸��״̬��
			//1120_�����豸״̬������������豸map
			//UpdateDeviceMapStatus(deviceId, DeviceStatus::ERR);
		}
	}
}

// ����豸�������
bool ScanWorkThdImpl::IsCameraDevice(const std::string& deviceId) const
{
	// 1125_�ж��Ƿ��ڳ�ʼ״̬�������豸ID
	return m_cameraDeviceIds.find(deviceId) != m_cameraDeviceIds.end();
}

bool ScanWorkThdImpl::SendTaskToCamera(const BusinessTask& task)
{
	CameraTcpManager& cameraMgr = CameraTcpManager::GetInstance();

	// �������Ƿ�����
	if (!cameraMgr.IsCameraOnline(task.devId)) 
	{
		LOG_INFO(QString(u8"����豸������: %1").arg(QString::fromStdString(task.devId)));
		return false;
	}

	// ����Ƿ���������������
	if (IsCameraTaskRunning(task.devId)) 
	{
		LOG_INFO(QString(u8"����豸����ִ����������: %1").arg(QString::fromStdString(task.devId)));
		return false;
	}

	// ��������ʼ����
	bool success = cameraMgr.SendTaskStartToCamera(task.devId);

	if (success) 
	{
		// ��¼��������״̬
		SetCameraTaskRunning(task.devId, task.proId, true);
		// �����豸״̬Ϊæµ
		if (m_deviceStatusCallBack) 
		{
			DeviceInfo deviceInfo;
			deviceInfo.devId = task.devId;
			deviceInfo.devType = 1; // ɨ���豸����
			deviceInfo.devStatus = DeviceStatus::BUSY; // æµ״̬
			m_deviceStatusCallBack(deviceInfo);
			m_cameraTaskInfo.insert({ task.devId , task });
			//1120_�����豸״̬������������豸map
			//m_devicesMap[task.devId]->SetStatus(DeviceStatus::BUSY);
		}
	}
	return success;
}

void ScanWorkThdImpl::ProcessCameraScanData(const BusinessTask& task, const std::vector<CameraScanData>& scanData)
{
	// ���л�ɨ��������
	std::string serializedData = SerializeCameraScanResults(scanData);

	LOG_INFO(QString(u8"���ɨ���������л���ɣ����ݴ�С: %1 bytes").arg(serializedData.size()));

	// ������Խ����л�������ݷ��͵�������
	// ����ͨ��HTTP��WebSocket���͵�������
	// ����OSS�ϴ�������Ϣ�߼�
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

	//�����ϴ�����
	//auto curTimerStr = QString::fromStdString(CommFun::GetInstance().GetCurrentTimeStr());
	//auto dstFolderPath = QCoreApplication::applicationDirPath() + QDir::separator() + QString("simulate_scan_data") + QDir::separator() + curTimerStr;
	//auto srcFolderPath = QCoreApplication::applicationDirPath() + QDir::separator() + QString("simulate_scan_data") + QDir::separator() + QString("src_data");
	// ��ȡԴ�ļ�����������Ŀ���ļ������ļ��У�
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
	1. ���Ƶ�ǰ�ļ���
	2. ����ǰ�ļ���������
	3. ��ȡ��ǰ�ļ���������
	4. �����ļ���
	*/
	auto curTimerStr = QString::fromStdString(CommFun::GetInstance().GetCurrentTimeStr());
	auto dstFolderPath = QCoreApplication::applicationDirPath() + QDir::separator() + QString("simulate_scan_data") + QDir::separator() + curTimerStr;
	auto srcFolderPath = QCoreApplication::applicationDirPath() + QDir::separator() + QString("simulate_scan_data") + QDir::separator() + QString("src_data");
	std::vector<std::string> vFilePath, vURLData;
	std::string retInfo;
	// ȡԴļĿļļУ
	CommFun::GetInstance().FolderCopy(srcFolderPath.toStdString(), dstFolderPath.toStdString());
	CommFun::GetInstance().GetFolderFile(dstFolderPath.toStdString(), vFilePath);
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
	LOG_INFO(u8"ִ��ȡ���������߼�");

	// ����Ƿ��г�ʱ��ȡ������
	std::lock_guard<std::mutex> lock(m_cancelTasksMutex);
	auto currentTime = std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::system_clock::now().time_since_epoch()).count();

	for (auto it = m_pendingCancelTasks.begin(); it != m_pendingCancelTasks.end();) 
	{
		const std::string& deviceId = it->first;
		const std::string& taskId = it->second.first;

		// ���������ӳ�ʱ����߼�
		// ���磺�������30��û���յ��ظ���ǿ��ȡ��
		// Ŀǰ��ʱ��������Ժ�����չ
		LOG_INFO(QString(u8"��ȡ������: Device=%1, TaskId=%2")
			.arg(QString::fromStdString(deviceId)).arg(QString::fromStdString(taskId)));
		++it;
	}
}

void ScanWorkThdImpl::OnCameraTaskCancelled(const std::string& deviceId, bool success)
{
	LOG_INFO(QString(u8"�������ȡ�����: Device=%1, Success=%2")
		.arg(QString::fromStdString(deviceId)).arg(success ? u8"�ɹ�" : u8"ʧ��"));

	std::string taskId;
	bool bStopped = false;
	DeviceInfo deviceInfo;
	SyncBusinessTask taskStatus;
	bool shouldCallStatusCallback = false;
	bool shouldCallTaskCallback = false;

	// ��ȡ���Ƴ���ȡ����������Ϣ
	// �޸���ʹ��ͳһ��˳�򲢼������ĳ���ʱ�䣬������������������
	{
		std::lock_guard<std::mutex> cancelLock(m_cancelTasksMutex);
		std::lock_guard<std::mutex> cameraLock(m_cameraTasksMutex);

		// ��ȡ���Ƴ���ȡ����������Ϣ
		auto cancelIt = m_pendingCancelTasks.find(deviceId);
		if (cancelIt != m_pendingCancelTasks.end()) 
		{
			taskId = cancelIt->second.first;
			bStopped = cancelIt->second.second;
			m_pendingCancelTasks.erase(cancelIt);
		}

		if (success) 
		{
			// ��������״̬
			m_cameraRunningTasks.erase(deviceId);
			m_cameraTaskInfo.erase(deviceId);

			// ׼���ص�����
			deviceInfo.devId = deviceId;
			deviceInfo.devType = 1;
			deviceInfo.devStatus = bStopped ? DeviceStatus::ERR : DeviceStatus::IDLE;
			shouldCallStatusCallback = true;

			if (!taskId.empty()) 
			{
				taskStatus.proId = taskId;
				taskStatus.op = "4";  // "4" ��ʾȡ��״̬
				taskStatus.devId = deviceId;
				shouldCallTaskCallback = true;
			}
		}
		else 
		{
			// ȡ��ʧ�ܣ�׼����������״̬�Ļص�����
			if (!taskId.empty()) 
			{
				taskStatus.proId = taskId;
				taskStatus.op = "1";  // ��������״̬
				taskStatus.devId = deviceId;
				shouldCallTaskCallback = true;
			}
		}
	}

	//�޸���������ִ�лص�������������պ�����
	if (success) 
	{
		if (shouldCallStatusCallback && m_deviceStatusCallBack) 
		{
			m_deviceStatusCallBack(deviceInfo);

			// ��ȫ�ظ����豸ӳ��״̬
			UpdateDeviceMapStatus(deviceId, deviceInfo.devStatus);
		}

		if (shouldCallTaskCallback && m_taskStatusCallBack1) 
		{
			m_taskStatusCallBack1(taskStatus);
		}

		LOG_INFO(QString(u8"�������ȡ���ɹ�: Device=%1, TaskId=%2")
			.arg(QString::fromStdString(deviceId)).arg(QString::fromStdString(taskId)));
	}
	else 
	{
		LOG_ERROR(QString(u8"�������ȡ��ʧ��: Device=%1, TaskId=%2")
			.arg(QString::fromStdString(deviceId)).arg(QString::fromStdString(taskId)));

		if (shouldCallTaskCallback && m_taskStatusCallBack1) 
		{
			m_taskStatusCallBack1(taskStatus);
		}
	}

	if (success) 
	{
		// ȡ���ɹ�����������״̬
		{
			std::lock_guard<std::mutex> lock(m_cameraTasksMutex);
			m_cameraRunningTasks.erase(deviceId);
			m_cameraTaskInfo.erase(deviceId);
		}

		// �����豸״̬
		if (m_deviceStatusCallBack) 
		{
			DeviceInfo deviceInfo;
			deviceInfo.devId = deviceId;
			deviceInfo.devType = 1; // ɨ���豸����
			deviceInfo.devStatus = bStopped ? DeviceStatus::ERR : DeviceStatus::IDLE;
			m_deviceStatusCallBack(deviceInfo);

			// �����豸ӳ���е�״̬
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

		// ��������״̬�ص���֪ͨȡ���ɹ�
		if (m_taskStatusCallBack1 && !taskId.empty()) 
		{
			SyncBusinessTask taskStatus;
			taskStatus.proId = taskId;
			taskStatus.op = "4";  // "4" ��ʾȡ��״̬
			taskStatus.devId = deviceId;
			m_taskStatusCallBack1(taskStatus);
		}

		LOG_INFO(QString(u8"�������ȡ���ɹ�: Device=%1, TaskId=%2")
			.arg(QString::fromStdString(deviceId)).arg(QString::fromStdString(taskId)));
	}
	else 
	{
		// ȡ��ʧ�ܣ�����ԭ״̬
		LOG_ERROR(QString(u8"�������ȡ��ʧ��: Device=%1, TaskId=%2")
			.arg(QString::fromStdString(deviceId)).arg(QString::fromStdString(taskId)));

		// ���Կ������Ի���ǿ��ȡ��
		if (m_taskStatusCallBack1 && !taskId.empty()) 
		{
			SyncBusinessTask taskStatus;
			taskStatus.proId = taskId;
			taskStatus.op = "1";  // ��������״̬
			taskStatus.devId = deviceId;
			m_taskStatusCallBack1(taskStatus);
		}
	}
}

void ScanWorkThdImpl::CancelTaskOnDevice(const std::string& devId, bool bStopped)
{
	LOG_INFO(QString(u8"�յ��豸ȡ����������: Device=%1, Stopped=%2")
		.arg(QString::fromStdString(devId))
		.arg(bStopped ? u8"��" : u8"��"));

	// ����Ƿ�������豸����ʵ�豸
	if (IsCameraDevice(devId) && !g_simulateReturnData) 
	{
		// ��ȡ��ǰ���е�����ID
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
			// ��¼��ȡ��������
			{
				std::lock_guard<std::mutex> lock(m_cancelTasksMutex);
				m_pendingCancelTasks[devId] = std::make_pair(taskId, bStopped);
			}

			// ����ȡ��������
			// �����ǰ����ģ��״̬����ȡ����ʱ�������������豸
			// 
			auto& cameraMgr = CameraTcpManager::GetInstance();
			bool sent = cameraMgr.SendTaskCancelToCamera(devId, taskId);

			if (!sent)
			{
				LOG_ERROR(QString(u8"��������ȡ������ʧ��: Device=%1, TaskId=%2")
					.arg(QString::fromStdString(devId))
					.arg(QString::fromStdString(taskId)));

				// ����ʧ�ܣ�ֱ�Ӵ���Ϊȡ��ʧ��
				OnCameraTaskCancelled(devId, false);
			}
			else 
			{
				LOG_INFO(QString(u8"�ѷ�������ȡ������: Device=%1, TaskId=%2")
					.arg(QString::fromStdString(devId))
					.arg(QString::fromStdString(taskId)));
			}
		}
		else 
		{
			LOG_INFO(QString(u8"�豸��ǰû�����е�����: %1").arg(QString::fromStdString(devId)));
			// û�����е�����ֱ�ӷ��سɹ�
			OnCameraTaskCancelled(devId, true);
		}
	}
	else if(g_simulateReturnData)
	{
		// ģ���豸��ʹ��deviceType����ģ��ȡ��
		// ������豸��ʹ��ԭ����ȡ���߼�
		WorkThdBaseImpl::CancelTaskOnDevice(devId, bStopped);
	}
}

void ScanWorkThdImpl::UpdateDeviceMapStatus(const std::string& deviceId, DeviceStatus status)
{
	//ʹ�û����m_taskMtx����m_devicesMap�ķ��ʣ����Ⲣ����������
	std::lock_guard<std::mutex> lock(m_taskMtx);
	auto it = m_devicesMap.find(deviceId);
	if (it != m_devicesMap.end()) 
	{
		it->second->SetStatus(status);
	}
}
