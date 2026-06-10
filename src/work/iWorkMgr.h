#pragma once

/** 
*  @author      
*  @class       �豸�ӿ��� 
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
	virtual void CancelTask(const std::string& device_id, bool bStopped = false) = 0; // ȡ��ָ���豸�ĵ�ǰ����
	virtual void CancelAllTasks() = 0;							// ȡ�������豸�ĵ�ǰ����
	virtual std::vector<DeviceInfo> GetAllDeviceStatus() = 0;
	virtual void AddRegDevInfo(std::vector<DeviceInfo>& devVec) = 0;	//�ӷ�������ȡ�豸��Ϣ

	virtual bool HasDevice(const std::string& devId) const = 0;
	virtual bool AddDevice(const std::string& devId, int devType) = 0;
	virtual bool RemoveDevice(const std::string& devId) = 0;
	virtual void HandleStopeedDev(const DeviceInfo& dev) = 0;	//��������豸


	//work�ź���
	virtual WorkThdSig* GetSignals() = 0;

signals:
	void SigTaskStatusUpdate(const QString& taskID, int status, const QString& deviceID);
	void SigTaskStatusUpdate1(const SyncBusinessTask& task);
	void SigDeviceStatusUpdate(const QString& deviceID, int devicetType, int status);

};
