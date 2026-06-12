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
#include "deviceScan.h"
#include "ipc/CameraTcpMgr.h"

 

/** 
*  @author      
*  @class       任务线程接口基类 
*  @brief       brief 
*/


class ScanWorkThd : public WorkThdBase
{
	//Q_OBJECT
public:
	ScanWorkThd();


	/** 
	*  @brief       添加对应的设备 
	*  @param[in]    
	*  @param[out]   
	*  @return                    
	*/
	void AddDevice(const std::shared_ptr<DeviceScan>& dev);



};


class ScanWorkThdImpl :  public WorkThdBaseImpl//, public QObject
{
	//Q_OBJECT
public:		
	ScanWorkThdImpl();// QObject *parent = nullptr);
	~ScanWorkThdImpl();

	/**
	*  @brief       添加对应的设备
	*  @param[in]
	*  @param[out]
	*  @return
	*/
	void AddDevice(const std::shared_ptr<DeviceScan>& dev); 

protected:
	void Run() override;
	void ExecuteTaskAsync(const BusinessTask& task) override;

	void PerformTaskInThread(const BusinessTask& task) override;
	void PerformSimulateTaskInThread(const BusinessTask& task) override;


private:
	// 相机设备处理
	bool IsCameraDevice(const std::string& deviceId) const;
	bool SendTaskToCamera(const BusinessTask& task);
	void HandleCameraScanResults(const std::string& deviceId, const std::vector<CameraScanData>& scanData);

	void ProcessCameraScanData(const BusinessTask& task, const std::vector<CameraScanData>& scanData);
	
	void ProcessCameraScanData1(const std::vector<CameraScanData>& scanData, 
								std::vector<FingerScanData>& fingerData);
	
	std::string SerializeCameraScanResults(const std::vector<CameraScanData>& scanData);

	// 相机任务状态管理	
	void SetCameraTaskRunning(const std::string& deviceId, const std::string& taskId, bool running);
	
	bool IsCameraTaskRunning(const std::string& deviceId) const;



//private slots:
//	// 相机TCP管理器事件处理
private:
	// ★ 相机事件处理函数（普通成员函数，不再是槽函数）
	void OnCameraConnected(const std::string& deviceId);
	void OnCameraDisconnected(const std::string& deviceId);
	void OnCameraTaskFinished(const std::string& deviceId, const std::vector<CameraScanData>& scanData, const BusinessTask& task = BusinessTask());
	void OnCameraTaskError(const std::string& deviceId, const std::string& errorMsg);

	//OSS相关处理逻辑
	void HandleSimulateData(const BusinessTask& task);

	// 处理取消任务
	void HandleCancelTask();

	// 相机任务取消相关
	void OnCameraTaskCancelled(const std::string& deviceId, bool success);

	// 重写取消任务方法以支持相机TCP通信
	void CancelTaskOnDevice(const std::string& devId, bool bStopped) override;

	// 安全的设备状态更新方法
	void UpdateDeviceMapStatus(const std::string& deviceId, DeviceStatus status);


private:
	bool m_cancelDev;

	// 相机设备管理
	std::map<std::string, std::string> m_cameraRunningTasks;  // deviceId -> taskId
	std::map<std::string, BusinessTask> m_cameraTaskInfo;     // deviceId -> BusinessTask (存储完整任务信息)
	mutable std::mutex m_cameraTasksMutex;                   // 相机任务状态互斥锁

	// 相机设备ID集合（可配置）
	std::set<std::string> m_cameraDeviceIds;
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


