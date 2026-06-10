#pragma once
#include "WSClient.h"

#include <QObject>
#include <QMutex>
#include <QTimer>

#include <string>
#include <memory>


class CommMgr : public QObject
{
	Q_OBJECT
public:
	CommMgr(QObject* parent = nullptr);
	~CommMgr();

	void connectToUrl(const std::string& url);
	void disconnect();
	void sendJson(const std::string& jsonText);

public slots:
	void sendHeartbeat();

signals:
	void connected();
	void disconnected();
	void message(const QString& msg);
	void error(const QString& err);

private:
	std::shared_ptr<WSClient> m_ws;
	QTimer m_heartTimer;
	QMutex m_sendMtx;
    //mutable std::mutex ws_mutex_; // 保护ws_client_的线程安全
};

