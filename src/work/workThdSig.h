#pragma once

#include <QObject>
#include <QString>
#include <memory>
#include "comm/MessageDefine.h"

//using namespace hv;

class WorkThdSig : public QObject
{
	Q_OBJECT
public:
	explicit WorkThdSig(QObject* par = nullptr);

signals:
	void SigTaskStatusUpdate(const QString& taskID, int status, const QString& deviceID);
	void SigTaskStatusUpdate1(const SyncBusinessTask& taskData);

	void SigDeviceStatusUpdate(const QString& deviceID, int devicetType, int status);

	void SigUpdateDevStatus2UI(const QString& devId);

	void SigDevRegFinished();

	//ͬ���豸������Ϣ��UI��
	void SigSyncDevStopped(const DeviceInfo& devId);

	//��ȡ/����OSS����
	void SigGetOSSToken();
};

