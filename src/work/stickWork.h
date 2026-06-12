#pragma once

#include <QString>

#include <thread>
#include <memory>
#include <atomic>
#include <condition_variable>
#include <map>
#include <vector>
#include <queue>
#include <string>
#include <set>
#include <mutex>


#include "workBase.h"
#include "deviceStick.h"
 

/** 
*  @author      
*  @class       任务线程接口基类 
*  @brief       brief 
*/

//----------------------------StickWorkThd----------------------------------------------
//----------------------------StickWorkThd----------------------------------------------
//----------------------------StickWorkThd----------------------------------------------

class StickWorkThd : public WorkThdBase
{
public:
	StickWorkThd();

	/** 
	*  @brief       添加对应的设备 
	*  @param[in]    
	*  @param[out]   
	*  @return                    
	*/
	void AddDevice(const std::shared_ptr<DeviceStick>& dev);

};

//----------------------------StickWorkThdImpl----------------------------------------------
//----------------------------StickWorkThdImpl----------------------------------------------
//----------------------------StickWorkThdImpl----------------------------------------------

class StickWorkThdImpl : public WorkThdBaseImpl
{
public:

	StickWorkThdImpl();

	~StickWorkThdImpl();

	/**
	*  @brief       添加对应的设备
	*  @param[in]
	*  @param[out]
	*  @return
	*/
	void AddDevice(const std::shared_ptr<DeviceStick>& dev);

protected:
	void Run() override;
	void ExecuteTaskAsync(const BusinessTask& task) override;

	void PerformTaskInThread(const BusinessTask& task) override;
	void PerformSimulateTaskInThread(const BusinessTask& task) override;

private:
	// 模拟数据处理流程
	void HandleSimulateData(const BusinessTask& task);

//private slots:
	//	相机TCP管理器事件处理
	// ★ 相机事件处理函数（普通成员函数，不再是槽函数）
	void OnStickConnected(const std::string& deviceId);
	void OnStickDisconnected(const std::string& deviceId);
	void OnStickTaskFinished(const std::string& deviceId, const BusinessTask& task);
	void OnStickTaskError(const std::string& deviceId, const std::string& errorMsg);
	// 安全的设备状态更新方法
	void UpdateDeviceMapStatus(const std::string& deviceId, DeviceStatus status);


private:
	// 相机设备管理
	std::map<std::string, std::string> m_stickRunningTasks;  // deviceId -> taskId
	//std::map<std::string, BusinessTask> m_stickTaskInfo;     // deviceId -> BusinessTask (存储完整任务信息)
	mutable std::mutex m_stickTasksMutex;                   // 相机任务状态互斥锁

	// 相机设备ID集合（可配置）
	std::set<std::string> m_stickDeviceIds;
	// 任务取消相关
	std::map<std::string, std::pair<std::string, bool>> m_pendingCancelTasks; // deviceId -> (taskId, bStopped)
	std::mutex m_cancelTasksMutex;

	/*
	 * 锁获取顺序规则（严格按此顺序获取，避免死锁）：
	 * 1. m_cancelTasksMutex (最外层)
	 * 2. m_cameraTasksMutex (中间层)
	 * 3. m_taskMtx (基类，最内层)
	 */

};

