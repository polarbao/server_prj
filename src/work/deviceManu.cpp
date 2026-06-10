#include "deviceManu.h"
#include "global.h"

#include "CommFun.h"
#include "SingleOSSToken.h"

//-------------------------DeviceManu--------------------------------
//-------------------------DeviceManu--------------------------------
//-------------------------DeviceManu--------------------------------

DeviceManu::DeviceManu(const std::string& id)
	: IODeviceBase(id, 2)
{
	this->m_pImpl = std::make_unique<DeviceManuImpl>(id);
}



//-------------------------DeviceManuImpl--------------------------------
//-------------------------DeviceManuImpl--------------------------------
//-------------------------DeviceManuImpl--------------------------------


DeviceManuImpl::DeviceManuImpl(const std::string& id)
	: IODeviceBaseImpl(id, 2)
{
	LOG_INFO(QString("manu_device_id_is_%1, work_for_scan_event").arg(QString::fromStdString(id)));


}

bool DeviceManuImpl::PerformTask(const BusinessTask& task)
{
	LOG_INFO(QString("manu_device_id_is_%1, work_for_manu_event").arg(QString::fromStdString(m_devID)).arg(QString::fromStdString(task.op)));
	// 模拟打印任务的特定逻辑
	// 判断任务处理类型，1. 正常执行； 2. 取消认为
	bool ret = false;
	if (task.op != "2" && m_devStatus == DeviceStatus::BUSY)
	{
		ret = CancelTask(task);
	}
	else if (task.op == "1" && m_devStatus == DeviceStatus::IDLE)
	{
		m_handleData.workType = SimulateWorkType::WORK_MANU;
		//设置设备状态为忙碌态
		SetStatus(DeviceStatus::BUSY);
		ret = PerformSimulateTask(task);
		//下发任务开始指令
	}
	return ret;
}

bool DeviceManuImpl::PerformSimulateTask(const BusinessTask& task)
{
	//LOG_INFO("Device %s (Type %d) performing task %s: %s", m_devID.c_str(), m_devType, task.proId.c_str(), task.op.c_str());
	LOG_INFO(QString("stick_work_thd_simulate_device_%1, dev_type_%2, performing_simulate_task_proid_%3_op_type_%4")
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
		auto curTimerStr = QString::fromStdString(CommFun::GetInstance().GetCurrentTimeStr()) + "_" + QString::fromStdString(task.proId);
		auto downFolderPath = QCoreApplication::applicationDirPath() + QDir::separator() + QString("simulate_scan_data") + QDir::separator() + \
			QString("down_file") + QDir::separator() + curTimerStr;
		std::vector<std::string> vURLData;
		std::string retInfo;

		//--------------------------
		//--------------------------
		//--------------------------
		// 使用可中断的睡眠来模拟工作，实际应用中应该是真实的设备操作
		auto start_time = std::chrono::steady_clock::now();
		auto target_duration = std::chrono::seconds(30);
		while (std::chrono::steady_clock::now() - start_time < target_duration)
		{
			while (SingleOSSToken::GetInstance().JudgeClientIsEmpty())
			{

			}
			// 检查取消任务信号
			if (m_taskCancelled.load())
			{
				LOG_INFO(QString("manu_work_thd_device_%1_task_%2_was_cancelled")
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
				LOG_INFO(QString("manu_work_thd_device_%1_task_%2_was_cancelled")
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
		LOG_INFO(QString(u8"制作业务线程模拟设备耗时操作定时器到期，模拟完成下载操作"));
		std::string tmpURL = "http://macbrush-shop-image.oss-cn-shanghai.aliyuncs.com/simulate_scan_data/20250811145822_101.ply";
		std::string downURL = 0 == task.data.size() ? tmpURL : task.data;
		if (task.data.size() == 0)
		{
			LOG_INFO(QString(u8"下发业务数据为空字符串，启用本地备份下载目录： %1").arg(QString::fromStdString(tmpURL)));
		}
		auto ret = SingleOSSToken::GetInstance().DownloadSingleFile(downURL, retInfo, downFolderPath.toStdString());
		//--------------------------
		//--------------------------
		//--------------------------
		// 任务完成
		SetStatus(DeviceStatus::IDLE);
		{
			std::lock_guard<std::mutex> lock(m_cancelMtx);
			m_currentTaskId.clear();
			m_curRunTask.Clear();
		}
		LOG_INFO(QString("manu_work_thd_device_%1, finished_task_%2.")
			.arg(QString::fromStdString(m_devID))
			.arg(QString::fromStdString(task.proId)));
		return true;
	}
	catch (const std::exception& e)
	{
		LOG_INFO(QString("manu_work_thd_device_%1, failed_2_perform_simulate_task_%2,err_info_%3")
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

bool DeviceManuImpl::CancelTask(const BusinessTask& task)
{
	return IODeviceBaseImpl::CancelTask(task);

}
