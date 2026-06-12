#pragma once

#include <QString>


#include <memory>
#include <mutex>
#include <functional>

#include <map>
#include <vector>
#include <string>

#include "iWorkMgr.h"
#include "workBase.h"
#include "workThdSig.h"
#include "comm/MessageDefine.h"
#include "IDbService.h"
#include "LocalIpcServer.h"

#include "scanWork.h"
#include "stickWork.h" 
#include "manuWork.h"

#include "deviceScan.h"
#include "deviceManu.h"
#include "deviceStick.h"

/** 
*  @author      
*  @class       任务线程接口基类 
*  @brief       brief 
*/

class WorkThdMgrImpl;

class WorkThdMgr : public IWorkThdMgr
{
public:
	WorkThdMgr();

	~WorkThdMgr() = default;

	void StartAllWorkThds() override;

	void StopAllWorkThds() override;

	void DispatchTask(const BusinessTask& task) override;

	void CancelTask(const std::string& devId, bool bStopped = false) override;

	void CancelAllTasks() override;

	std::vector<DeviceInfo> GetAllDeviceStatus() override;

	//Note：注册时，获取所有设备信息进行同步注册
	void AddRegDevInfo(std::vector<DeviceInfo>& devVec) override;	

	bool HasDevice(const std::string& devId) const override;

	bool AddDevice(const std::string& devId, int devType) override;

	bool RemoveDevice(const std::string& devId) override;
	

	WorkThdSig* GetSignals() override;

	void RegisterDeviceStatusCallBack(std::function<void(const DeviceInfo&)> callback);

	//Note：处理故障设备流程
	void HandleStopeedDev(const DeviceInfo& dev) override;

private:
	std::unique_ptr<WorkThdMgrImpl> m_pImpl;
};


class WorkThdMgrImpl : public IWorkThdMgr
{
public:

	WorkThdMgrImpl();

	~WorkThdMgrImpl() override;

	void StartAllWorkThds() override;

	void StopAllWorkThds() override;

	void DispatchTask(const BusinessTask& task) override;


	void CancelTask(const std::string& devId, bool bStopped = false) override;

	void CancelAllTasks() override;

	std::vector<DeviceInfo> GetAllDeviceStatus() override;

	//Note：注册时，获取所有设备信息进行同步注册
	void AddRegDevInfo(std::vector<DeviceInfo>& devVec) override;

	bool HasDevice(const std::string& devId) const override;

	bool AddDevice(const std::string& devId, int devType) override;

	bool RemoveDevice(const std::string& devId) override;

	//Note：处理故障设备流程
	void HandleStopeedDev(const DeviceInfo& dev);

	WorkThdSig* GetSignals() override;


private:
	void AddAllThdDevices();



private:
	void HandleTashStatusUpdate(const std::string& taskID, const std::string& status, const std::string& devID);
	void HandleTashStatusUpdate1(const SyncBusinessTask& taskID, const std::string& status);

	void HandleDevStatusUpdate(const DeviceInfo& info);

private:
	std::map<std::string, std::shared_ptr<IWorkThd>> m_workThd;
	mutable std::mutex m_deviceMtx;
	std::map<std::string, DeviceInfo> m_curDevStatus;
	WorkThdSig m_sig;

	std::shared_ptr<IDbService> m_dbService;
	std::unique_ptr<LocalIpcServer> m_ipcServer;

	std::thread m_watchdogThd;
	std::atomic<bool> m_watchdogThdRunning;

private:
	void WatchdogThreadFunction();

};
