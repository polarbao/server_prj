#include "CommMgr.h"

#include <QMutexLocker>
#include <utility>


CommMgr::CommMgr(QObject* parent)
	: QObject(parent), m_ws(std::make_shared<WSClient>()) 
{
	m_ws->setOnOpen([this]() 
	{
		emit connected();
	});
	m_ws->setOnClose([this]() 
	{
		emit disconnected();
	});
	m_ws->setOnMessage([this](const std::string& m) 
	{
		emit message(QString::fromStdString(m));
	});
	m_ws->setOnError([this](const std::string& e) 
	{
		emit error(QString::fromStdString(e));
	});

	m_heartTimer.setInterval(5000);
	connect(&m_heartTimer, &QTimer::timeout, this, &CommMgr::sendHeartbeat);
}

CommMgr::~CommMgr() = default;

void CommMgr::connectToUrl(const std::string& url) 
{
	if (m_ws->connect(url)) 
	{
		m_heartTimer.start();
	}
}

void CommMgr::disconnect() 
{
	m_heartTimer.stop();
	m_ws->close();
}

void CommMgr::sendJson(const std::string& jsonText) 
{
	QMutexLocker locker(&m_sendMtx);
	m_ws->sendText(jsonText);
}

void CommMgr::sendHeartbeat() 
{
	static const std::string kPing = "{\"type\":\"ping\"}";
	QMutexLocker locker(&m_sendMtx);
	m_ws->sendText(kPing);
}

