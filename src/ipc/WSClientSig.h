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
	//SigNetworkStatusChanged 函数统一处理
	void SigConnectStatusChanged(bool connected);
	void SigMsgRecevied(const QString& typeStr, const WSMsgBase&  msgData);
	void SigHeartBeatAckReceived();

	//HTTP_Data_Trans
	void SigBindRegDev(const DevBindResp& data);
	//工控启动阶段，同步各设备任务状态
	void SigRegDevTaskSync(const DevSereMapResp& data);
	void SigTestWS(std::string data);
	// const std::map<std::string, std::string>& data

	// 1015_add_网络重连
	void SigNetworkStatusChanged(ConnectionType connType, NetworkStatus status, const QString& message = "");
	void SigReconnectAttempt(ConnectionType connType, int attemptCount, int maxAttempts);
	void SigReconnectFailed(ConnectionType connType, const QString& reason);

	// 1027_同步更新断网情况下缓存完成进程的数据
	void SigSyncCacheFinishTask(const UnifiedMessage& data);
};

