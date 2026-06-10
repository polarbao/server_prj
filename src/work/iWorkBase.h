#pragma once

/** 
*  @author      
*  @class       任务管理接口抽象类 
*  @brief       brief 
*/

#include <string>
#include <memory>
#include <functional>
#include <thread>
#include <atomic>
#include <QObject>

#include "comm/MessageDefine.h"

class IWorkThd : public QObject
{
	Q_OBJECT
public:
	explicit IWorkThd(QObject *parent = nullptr) : QObject(parent) {}

	virtual ~IWorkThd() = default;

	virtual void Start() = 0;

	virtual void Stop() = 0;

	virtual void AddTask(const BusinessTask& task) = 0;

	virtual bool HasDevice(const std::string& devId) const = 0;

	virtual bool RemoveDevice(const std::string&devId) = 0;

	// 取消指定设备的任务
	virtual void CancelTaskOnDevice(const std::string& devId, bool bStopped = false) = 0;

	// 取消所有设备的任务
	virtual void CancelAllTasks() = 0; 

	virtual std::string GetWorkType() const = 0;

	/** 
	*  @brief       获取当前工作线程所有设备状态 
	*  @param[in]    
	*  @param[out]   
	*  @return                    
	*/
	virtual std::vector<DeviceInfo> GetDeviceStatus() const = 0;

	/** 
	*  @brief       任务状态回调函数 
	*  @param[in]    
	*  @param[out]   
	*  @return                    
	*/
	//virtual void RegisterTaskStatusCallBack(std::function<void(const std::string&, const std::string&, const std::string&)> callback) = 0;
	virtual void RegisterTaskStatusCallBack1(std::function<void(const SyncBusinessTask&)> callback) = 0;

	/** 
	*  @brief       设备状态回调函数 
	*  @param[in]    
	*  @param[out]   
	*  @return                    
	*/
	virtual void RegisterDeviceStatusCallBack(std::function<void(const DeviceInfo&)> callback) = 0;
};

// 任务执行信息结构
struct RunningTaskInfo 
{
	BusinessTask task;
	std::shared_ptr<std::thread> task_thread;
	std::atomic<bool> is_running;

	// 修复构造函数，正确初始化所有成员
	RunningTaskInfo(const BusinessTask& t)
		: task(t)
		, task_thread(nullptr)
		, is_running(true)
	{

	}

	// 添加析构函数确保线程安全清理
	~RunningTaskInfo() 
	{
		if (task_thread && task_thread->joinable()) 
		{
			try 
			{
				task_thread->join();
			}
			catch (...) 
			{
				// 异常时detach线程避免terminate
				task_thread->detach();
			}
		}
	}
};
