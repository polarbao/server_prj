#include "deviceScan.h"


#include <QObject>
#include <QTimer>

#include "global.h"
#include "CommFun.h"
#include "SingleOSSToken.h"
#include "CLogManager.h"


DeviceScan::DeviceScan(const std::string& device_id)
	// ���û��๹�캯��
	: IODeviceBase(device_id, 1)
{
		this->m_pImpl = std::make_unique<DeviceScanImpl>(device_id); // ���¸�ֵΪ����Impl
}


DeviceScanImpl::DeviceScanImpl(const std::string& id)
	: IODeviceBaseImpl(id, 1)
{
	LOG_INFO(QString("scan_device_id_is_%1, work_for_scan_event").arg(QString::fromStdString(id)));

}

bool DeviceScanImpl::PerformTask(const BusinessTask& task)
{
	LOG_INFO(QString("Printer Device %1 is printing: %2").arg(QString::fromStdString(m_devID)).arg(QString::fromStdString(task.op)));
	// ģ���ӡ������ض��߼�
	// �ж����������ͣ�1. ����ִ�У� 2. ȡ����Ϊ
	bool ret = false;
	if(task.op != "2" && m_devStatus == DeviceStatus::BUSY)
	{
		ret = CancelTask(task);
	}
	else if(task.op == "1" && m_devStatus == DeviceStatus::IDLE)
	{
		m_handleData.workType = SimulateWorkType::WORK_SCAN;
		//�����豸״̬Ϊæµ̬
		SetStatus(DeviceStatus::BUSY);
		ret = PerformSimulateTask(task);
		//�·�����ʼָ��
	}
	return ret;
}

bool DeviceScanImpl::PerformSimulateTask(const BusinessTask& task)
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
		//SetStatus(DeviceStatus::BUSY);

		auto curTimerStr = QString::fromStdString(CommFun::GetInstance().GetCurrentTimeStr());
		auto dstFolderPath = QCoreApplication::applicationDirPath() + QDir::separator() + QString("simulate_scan_data") + QDir::separator() + curTimerStr;
		auto srcFolderPath = QCoreApplication::applicationDirPath() + QDir::separator() + QString("simulate_scan_data") + QDir::separator() + QString("src_data");
		std::vector<std::string> vFilePath, vURLData;
		std::string retInfo;
		// ȡԴļĿļļУ
		CommFun::GetInstance().FolderCopy(srcFolderPath.toStdString(), dstFolderPath.toStdString());
		CommFun::GetInstance().GetFolderFile(dstFolderPath.toStdString(), vFilePath);


		// ʹÿжϵ˯ģ⹤ʵӦӦʵ豸
		//------------ʼ--------------
		auto start_time = std::chrono::steady_clock::now();
		auto target_duration = std::chrono::seconds(30);
		while (std::chrono::steady_clock::now() - start_time < target_duration)
		{
			while (SingleOSSToken::GetInstance().JudgeClientIsEmpty())
			{
				int a = 1;
			}
			// ���ȡ�������ź�
			if (m_taskCancelled.load())
			{
				LOG_INFO(QString("Device %1 task %2 was cancelled.")
					.arg(QString::fromStdString(m_devID))
					.arg(QString::fromStdString(task.proId)));
			
				//�ǹ���״̬��ȡ����������״̬����Ϊ����
				if (m_devStatus != DeviceStatus::ERR)
				{
					//ͬ���豸״̬
					SetStatus(DeviceStatus::IDLE);
				}
				{
					std::lock_guard<std::mutex> lock(m_cancelMtx);
					m_currentTaskId.clear();
					m_curRunTask.Clear();
				}
				return false; // ����ȡ��
			}
			//ͬ��ȡ�����豸״̬
			// ʹ����������ʵ�ֿ��жϵĵȴ�
			std::unique_lock<std::mutex> lock(m_cancelMtx);
			m_cancelCV.wait_for(lock, std::chrono::milliseconds(100), [this]
			{
				return m_taskCancelled.load();
			});
			if (m_taskCancelled.load())
			{
				LOG_INFO(QString("scan_work_thd_device_%1_task_%2_was_cancelled.")
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
		// ʵ��OSS�ϴ�����
		LOG_INFO(QString(u8"ɨ��ҵ���߳�ģ���豸��ʱ������ʱ�����ڣ�ģ�����ɨ�����"));
		SingleOSSToken::GetInstance().UploadMulti(vFilePath, retInfo, vURLData);
		for (int i = 0; i < vURLData.size(); ++i)
		{
			int fingerId = i < 5 ? 101 + i : 201 - 5 + i;
			CameraScanData tmpData;
			tmpData.data_id = fingerId;
			tmpData.data_path = vURLData.at(i);
			m_handleData.scanData.push_back(tmpData);
		}
		//------------�������--------------

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

bool DeviceScanImpl::CancelTask(const BusinessTask& task)
{
	//ȡ������Ĵ���
	return IODeviceBaseImpl::CancelTask(task);
}
