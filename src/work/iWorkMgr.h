#pragma once

/** 
*  @author      
*  @class       设备接口类 
*  @brief       brief 
*/

#include <string>
#include <memory>
#include <functional>

#include "comm/MessageDefine.h"
#include "workThdSig.h"

class IWorkThdMgr : public QObject
{
	Q_OBJECT
public:

	virtual ~IWorkThdMgr() = default;

	virtual void StartAllWorkThds() = 0;
	virtual void StopAllWorkThds() = 0;
	virtual void DispatchTask(const BusinessTask& task) = 0;
	virtual void CancelTask(const std::string& device_id, bool bStopped = false) = 0; // 取消指定设备的当前任务
	virtual void CancelAllTasks() = 0;							// 取消所有设备的当前任务
	virtual std::vector<DeviceInfo> GetAllDeviceStatus() = 0;
	virtual void AddRegDevInfo(std::vector<DeviceInfo>& devVec) = 0;	//从服务器获取设备信息

	virtual bool HasDevice(const std::string& devId) const = 0;
	virtual bool AddDevice(const std::string& devId, int devType) = 0;
	virtual bool RemoveDevice(const std::string& devId) = 0;
	virtual void HandleStopeedDev(const DeviceInfo& dev) = 0;	//处理故障设备


	//work信号类
	virtual WorkThdSig* GetSignals() = 0;

signals:
	void SigTaskStatusUpdate(const QString& taskID, int status, const QString& deviceID);
	void SigTaskStatusUpdate1(const SyncBusinessTask& task);
	void SigDeviceStatusUpdate(const QString& deviceID, int devicetType, int status);

};
