#pragma once
#include "WSClient.h"

#include <QObject>
#include <QThread>
#include <QString>
#include <QQueue>
#include <QMutex>
#include <QTimer>

#include <thread>
#include <memory>
#include <atomic>
#include <condition_variable>
#include <map>
#include <vector>
#include <queue>
#include <string>

#include "deviceBase.h"
#include "iWorkBase.h"
 

/** 
*  @author      
*  @class       任务线程接口基类 
*  @brief       brief 
*/

class WorkThdBaseImpl;

class WorkThdBase : public IWorkThd
{
	
public:
	WorkThdBase(const std::string& work_type);
	~WorkThdBase() = default;

	void Start() override;

	void Stop() override;

	void AddTask(const BusinessTask& task) override;

	bool TestAddDev(const std::string& devId);

	bool HasDevice(const std::string& devId) const override;

	bool RemoveDevice(const std::string&devId) override;

	void CancelTaskOnDevice(const std::string& devId, bool bStopped) override;

	void CancelAllTasks() override;

	std::string GetWorkType() const override;

	std::vector <DeviceInfo> GetDeviceStatus() const override;

	//void RegisterTaskStatusCallBack(std::function<void(const std::string&, const std::string&, const std::string&)> callback);
	void RegisterTaskStatusCallBack1(std::function<void(const SyncBusinessTask&)> callback);

	void RegisterDeviceStatusCallBack(std::function<void(const DeviceInfo&)> callback);

protected:
	std::unique_ptr<IWorkThd> m_pImpl;
};


class WorkThdBaseImpl : public IWorkThd//, public QObject
{
	//Q_OBJECT
public:

	WorkThdBaseImpl(const std::string& work_type);// , QObject *parent = nullptr);

	~WorkThdBaseImpl();

	void Start() override;

	void Stop() override;

	void AddTask(const BusinessTask& task) override;

	bool HasDevice(const std::string& devId) const override;

	bool RemoveDevice(const std::string&devId) override;

	void CancelTaskOnDevice(const std::string& devId, bool bStopped) override;

	void CancelAllTasks() override;

	std::string GetWorkType() const override;

	std::vector<DeviceInfo> GetDeviceStatus() const override;

	//void RegisterTaskStatusCallBack(std::function<void(const std::string&, const std::string&, const std::string&)> callback) override;
	void RegisterTaskStatusCallBack1(std::function<void(const SyncBusinessTask&)> callback) override;

	void RegisterDeviceStatusCallBack(std::function<void(const DeviceInfo &)> callback) override;


protected:

	virtual void Run() = 0;

	void OnDeviceStatusUpdate(const DeviceInfo& info);

	// 1014_新增：任务执行和管理方法
	virtual void ExecuteTaskAsync(const BusinessTask& task);

	virtual void PerformTaskInThread(const BusinessTask& task);
	virtual void PerformSimulateTaskInThread(const BusinessTask& task);

	//模拟执行操作

	void CleanupCompletedTasks();
	size_t GetRunningTaskCount();
	size_t GetMaxConcurrentTasks() const;
	void UpdateMaxConcurrentTasks();


protected:

	std::string m_workType;
	std::atomic<bool> m_bRunning;
	std::thread m_thd;
	std::queue<BusinessTask> m_taskQueue;
	std::mutex m_taskMtx;
	std::condition_variable m_taskCV;
	std::map<std::string, std::shared_ptr<IODeviceBase>> m_devicesMap;


	//std::function<void(const std::string&, const std::string&, const std::string&)> m_taskStatusCallBack;
	std::function<void(const SyncBusinessTask&)> m_taskStatusCallBack1;

	//todo: 使用？
	std::function<void(const DeviceInfo&)> m_deviceStatusCallBack;

	// 1014_新增：并发任务管理
	std::map<std::string, std::shared_ptr<RunningTaskInfo>> m_runningTasks;
	std::mutex m_runningTasksMtx;
	size_t m_maxConcurrentTasks; // 最大并发任务数，等于绑定的设备数量
};

