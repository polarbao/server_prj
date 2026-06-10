#include "deviceStick.h"
#include "global.h"

#include "CommFun.h"
#include "SingleOSSToken.h"


//-------------------------DeviceStick--------------------------------
//-------------------------DeviceStick--------------------------------
//-------------------------DeviceStick--------------------------------

DeviceStick::DeviceStick(const std::string& id)
	: IODeviceBase(id, 3)
{
	this->m_pImpl = std::make_unique<DeviceStickImpl>(id);
}


//-------------------------DeviceStickImpl--------------------------------
//-------------------------DeviceStickImpl--------------------------------
//-------------------------DeviceStickImpl--------------------------------



DeviceStickImpl::DeviceStickImpl(const std::string& id)
	: IODeviceBaseImpl(id, 3)
{
	LOG_INFO(QString("stick_device_id_is_%1, work_for_stick_event")
		.arg(id.c_str()));
}

bool DeviceStickImpl::PerformTask(const BusinessTask& task)
{
	LOG_INFO(QString("stick_device_id_is_%1, work_for_stick_event")
		.arg(QString::fromStdString(m_devID))
		.arg(QString::fromStdString(task.op)));
	// ģ���ӡ������ض��߼�
	// �ж����������ͣ�1. ����ִ�У� 2. ȡ����Ϊ
	bool ret = false;
	if (task.op != "2" && m_devStatus == DeviceStatus::BUSY)
	{
		ret = CancelTask(task);
	}
	else if (task.op == "1" && m_devStatus == DeviceStatus::IDLE)
	{
		m_handleData.workType = SimulateWorkType::WORK_MANU;
		//�����豸״̬Ϊæµ̬
		SetStatus(DeviceStatus::BUSY);
		ret = PerformSimulateTask(task);
		//�·�����ʼָ��
	}
	return ret;
}

bool DeviceStickImpl::PerformSimulateTask(const BusinessTask& task)
{
	//LOG_INFO("Device %s (Type %d) performing task %s: %s", m_devID.c_str(), m_devType, task.proId.c_str(), task.op.c_str());
	LOG_INFO(QString("stick_work_thd_simulate_device_%1, dev_type_%2, performing_simulate_task_proid_%3_op_type_%4")
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

		// ģ�������ǰ��ִ�У�ʵ���п����漰����IO����
		// ʹ�ÿ��жϵ�˯����ģ�⹤����ʵ��Ӧ����Ӧ������ʵ���豸����
		//------------����ʼ--------------

		auto start_time = std::chrono::steady_clock::now();
		auto target_duration = std::chrono::seconds(30);
		while (std::chrono::steady_clock::now() - start_time < target_duration)
		{
			// ���ȡ�������ź�
			if (m_taskCancelled.load())
			{
				LOG_INFO(QString("stick_work_thd_device_%1_task_%2_was_cancelled.")
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
			// ͬ��ȡ�����豸״̬
			// ʹ����������ʵ�ֿ��жϵĵȴ���100ms���һ���Ƿ����ж��¼�
			std::unique_lock<std::mutex> lock(m_cancelMtx);
			m_cancelCV.wait_for(lock, std::chrono::milliseconds(100), [this]
			{
				return m_taskCancelled.load();
			});
			if (m_taskCancelled.load())
			{
				LOG_INFO(QString("stick_work_thd_device_%1_task_%2_was_cancelled.")
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
		LOG_INFO(QString(u8"����ҵ���߳�ģ���豸��ʱ������ʱ�����ڣ�ģ��������ز���"));
		//------------�������--------------

		SetStatus(DeviceStatus::IDLE);
		{
			std::lock_guard<std::mutex> lock(m_cancelMtx);
			m_currentTaskId.clear();
			m_curRunTask.Clear();
		}
		LOG_INFO(QString("stick_work_thd_device_%1, finished_task_%2.")
			.arg(QString::fromStdString(m_devID))
			.arg(QString::fromStdString(task.proId)));
		return true;
	}
	catch (const std::exception& e)
	{
		LOG_INFO(QString("stick_work_thd_device_%1, failed_2_perform_simulate_task_%2,err_info_%3")
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

bool DeviceStickImpl::CancelTask(const BusinessTask& task)
{
	return IODeviceBaseImpl::CancelTask(task);

}

