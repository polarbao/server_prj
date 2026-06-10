#include "deviceScan.h"


#include <QObject>
#include <QTimer>

#include "global.h"
#include "CommFun.h"
#include "SingleOSSToken.h"
#include "CLogManager.h"


DeviceScan::DeviceScan(const std::string& device_id)
	// 调用基类构造函数
	: IODeviceBase(device_id, 1)
{
		this->m_pImpl = std::make_unique<DeviceScanImpl>(device_id); // 重新赋值为派生Impl
}


DeviceScanImpl::DeviceScanImpl(const std::string& id)
	: IODeviceBaseImpl(id, 1)
{
	LOG_INFO(QString("scan_device_id_is_%1, work_for_scan_event").arg(QString::fromStdString(id)));

}

bool DeviceScanImpl::PerformTask(const BusinessTask& task)
{
	LOG_INFO(QString("Printer Device %1 is printing: %2").arg(QString::fromStdString(m_devID)).arg(QString::fromStdString(task.op)));
	// 模拟打印任务的特定逻辑
	// 判断任务处理类型，1. 正常执行； 2. 取消认为
	bool ret = false;
	if(task.op != "2" && m_devStatus == DeviceStatus::BUSY)
	{
		ret = CancelTask(task);
	}
	else if(task.op == "1" && m_devStatus == DeviceStatus::IDLE)
	{
		m_handleData.workType = SimulateWorkType::WORK_SCAN;
		//设置设备状态为忙碌态
		SetStatus(DeviceStatus::BUSY);
		ret = PerformSimulateTask(task);
		//下发任务开始指令
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
		// 重置取消标志并记录当前任务ID
		{
			std::lock_guard<std::mutex> lock(m_cancelMtx);
			m_taskCancelled = false;
			m_currentTaskId = task.proId;
			m_curRunTask = task;
		}

		// 模拟任务执行，实际中可能涉及复杂IO操作
		// 获取中断数据处理
		//SetStatus(DeviceStatus::BUSY);

		auto curTimerStr = QString::fromStdString(CommFun::GetInstance().GetCurrentTimeStr());
		auto dstFolderPath = QCoreApplication::applicationDirPath() + QDir::separator() + QString("simulate_scan_data") + QDir::separator() + curTimerStr;
		auto srcFolderPath = QCoreApplication::applicationDirPath() + QDir::separator() + QString("simulate_scan_data") + QDir::separator() + QString("src_data");
		std::vector<std::string> vFilePath, vURLData;
		std::string retInfo;
		// 获取源文件夹中所有条目（文件和子文件夹）
		CommFun::GetInstance().FolderCopy(srcFolderPath, dstFolderPath);
		CommFun::GetInstance().GetFolderFile(dstFolderPath, vFilePath);


		// 使用可中断的睡眠来模拟工作，实际应用中应该是真实的设备操作
		//------------任务开始--------------
		auto start_time = std::chrono::steady_clock::now();
		auto target_duration = std::chrono::seconds(30);
		while (std::chrono::steady_clock::now() - start_time < target_duration)
		{
			while (SingleOSSToken::GetInstance().JudgeClientIsEmpty())
			{
				int a = 1;
			}
			// 检查取消任务信号
			if (m_taskCancelled.load())
			{
				LOG_INFO(QString("Device %1 task %2 was cancelled.")
					.arg(QString::fromStdString(m_devID))
					.arg(QString::fromStdString(task.proId)));
			
				//非故障状态下取消任务，设置状态设置为空闲
				if (m_devStatus != DeviceStatus::ERR)
				{
					//同步设备状态
					SetStatus(DeviceStatus::IDLE);
				}
				{
					std::lock_guard<std::mutex> lock(m_cancelMtx);
					m_currentTaskId.clear();
					m_curRunTask.Clear();
				}
				return false; // 任务被取消
			}
			//同步取消的设备状态
			// 使用条件变量实现可中断的等待
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
		// 实现OSS上传操作
		LOG_INFO(QString(u8"扫描业务线程模拟设备耗时操作定时器到期，模拟完成扫描操作"));
		SingleOSSToken::GetInstance().UploadMulti(vFilePath, retInfo, vURLData);
		for (int i = 0; i < vURLData.size(); ++i)
		{
			int fingerId = i < 5 ? 101 + i : 201 - 5 + i;
			CameraScanData tmpData;
			tmpData.data_id = fingerId;
			tmpData.data_path = vURLData.at(i);
			m_handleData.scanData.push_back(tmpData);
		}
		//------------任务完成--------------

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
	//取消任务的处理
	return IODeviceBaseImpl::CancelTask(task);
}
