#pragma once

#include <QObject>
#include <QString>
#include <memory>
#include "comm/MessageDefine.h"
#include "HttpRepParser.h"

//using namespace hv;

class WSClientSig : public QObject
{
	Q_OBJECT
public:
	explicit WSClientSig(QObject* par = nullptr);

signals:
	//SigNetworkStatusChanged ����ͳһ����
	void SigConnectStatusChanged(bool connected);
	void SigMsgRecevied(const QString& typeStr, const WSMsgBase&  msgData);
	void SigHeartBeatAckReceived();

	//HTTP_Data_Trans
	void SigBindRegDev(const DevBindResp& data);
	//��������׶Σ�ͬ�����豸����״̬
	void SigRegDevTaskSync(const DevSereMapResp& data);
	void SigTestWS(std::string data);
	// const std::map<std::string, std::string>& data

	// 1015_add_��������
	void SigNetworkStatusChanged(ConnectionType connType, NetworkStatus status, const QString& message = "");
	void SigReconnectAttempt(ConnectionType connType, int attemptCount, int maxAttempts);
	void SigReconnectFailed(ConnectionType connType, const QString& reason);

	// 1027_ͬ�����¶�������»�����ɽ��̵�����
	void SigSyncCacheFinishTask(const UnifiedMessage& data);
};

