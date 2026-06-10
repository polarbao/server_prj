#include "storeLoginUI.h"

#include "global.h"

#include <QGridLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QUuid>
#include <QMap>

//#include "hv\WebSocketClient.h"
//using namespace hv;


StoreLoginUI::StoreLoginUI(QWidget *parent)
    : QWidget(parent)
	, m_btnGroup(new QButtonGroup())
{
	Init();
}

StoreLoginUI::~StoreLoginUI()
{

}

void StoreLoginUI::Init()
{
	InitUI();
	InitConnect();
	//����Ĭ�ϲ���
	SetDefaultParam();
}

void StoreLoginUI::InitConnect()
{
	 connect(m_btnGroup, QOverload<int>::of(&QButtonGroup::buttonClicked), this, &StoreLoginUI::OnBtnClicked);
	//connect(m_btnGroup, QOverload<QAbstractButton*>::of(&QButtonGroup::buttonClicked), this, &StoreLoginUI::OnConnBtnClicked);

}




void StoreLoginUI::SetDefaultParam()
{
	//////dev
	//m_storeId->setText("1000015");
	//m_userName->setText("155557102423");
	//m_pwd->setText("123456");
	//m_httpUrl->setText("http://dev.shop.moonbii.net");
	//m_httpPort->setText("7000");
	//m_wsUrl->setText("ws://dev.shop.moonbii.net/shop/ws/?user_id=1000015&client_type=machine");

	//release
	m_storeId->setText("1000016");
	m_userName->setText("155557102423");
	m_pwd->setText("123456");
	m_httpUrl->setText("http://test.shop.moonbii.net");
	m_httpPort->setText("7010");
	m_wsUrl->setText("ws://test.shop.moonbii.net/shop/ws/?user_id=1000016&client_type=machine");
}

void StoreLoginUI::UpdateButtonStates(bool bConn)
{
	if (m_btnGroup) 
	{
		// ���谴ť����: 0=����, 1=�Ͽ�
		m_btnGroup->button(0)->setEnabled(!bConn); // ���Ӱ�ť
		m_btnGroup->button(1)->setEnabled(bConn);  // �Ͽ���ť
	}
}

void StoreLoginUI::InitUI()
{
	//main_layout
	QVBoxLayout* pv_main = new QVBoxLayout(this);
	this->setLayout(pv_main);

	QGroupBox* workGroup = new QGroupBox("work oper");
	{
		QVBoxLayout* ph_v = new QVBoxLayout(workGroup);
		{
			QHBoxLayout* ph_wsLogin = new QHBoxLayout();
			{
				QLabel* titleLab = new QLabel(u8"����Id");
				m_storeId = new QLineEdit();
				m_storeId->setPlaceholderText(u8"����������Id");
				QLabel* statusLab = new QLabel(u8"����״̬");
				m_status = new QLabel(u8"δ����");
				m_status->setStyleSheet("QLabel { color : red; font-weight: bold; }");
				QPushButton* connBtn = new QPushButton(u8"����");
				QPushButton* disconnBtn = new QPushButton(u8"�Ͽ�����");
				QPushButton* testOperBtn1 = new QPushButton(u8"��ȡOSS����");
				QPushButton* testOperBtn2 = new QPushButton(u8"test_oper2");
				QPushButton* simulateDataBtn = new QPushButton(u8"ģ������");
				simulateDataBtn->setCheckable(true);
				simulateDataBtn->setChecked(true);
				g_simulateReturnData = true;

				// release�汾�У����ò�����ذ�ť
				//testOperBtn1->setEnabled(false);
				testOperBtn2->setEnabled(false);

				m_btnGroup->setExclusive(false);
				m_btnGroup->addButton(connBtn, EUI::EIOT_Conn);
				m_btnGroup->addButton(disconnBtn, EUI::EIOT_Disconn);
				m_btnGroup->addButton(testOperBtn1, EUI::EIOT_TestStates1);
				m_btnGroup->addButton(testOperBtn2, EUI::EIOT_TestStates2);
				m_btnGroup->addButton(simulateDataBtn, EUI::EIOT_SimulateData);

				ph_wsLogin->addWidget(titleLab, 1);
				ph_wsLogin->addWidget(m_storeId, 1);
				ph_wsLogin->addStretch();
				ph_wsLogin->addWidget(statusLab, 1);
				ph_wsLogin->addWidget(m_status, 1);
				ph_wsLogin->addWidget(connBtn, 1);
				ph_wsLogin->addWidget(disconnBtn, 1);
				ph_wsLogin->addWidget(testOperBtn1, 1);
				ph_wsLogin->addWidget(testOperBtn2, 1);
				ph_wsLogin->addWidget(simulateDataBtn, 1);

			}
			ph_v->addLayout(ph_wsLogin);

			QHBoxLayout* ph_httpLogin = new QHBoxLayout();
			{
				QLabel* userLab = new QLabel(u8"�û���");
				m_userName = new QLineEdit();
				m_userName->setPlaceholderText(u8"�������û���");

				QLabel* pwdLab = new QLabel(u8"����");
				m_pwd = new QLineEdit();
				m_pwd->setPlaceholderText(u8"����������");

				QPushButton* connBtn = new QPushButton(u8"��½");
				QPushButton* disconnBtn = new QPushButton(u8"ͬ���豸����״̬");
				m_btnGroup->addButton(connBtn, EUI::EIOT_Login);
				m_btnGroup->addButton(disconnBtn, EUI::EIOT_LoginBindDev);

				ph_httpLogin->addWidget(userLab, 1);
				ph_httpLogin->addWidget(m_userName, 1);
				ph_httpLogin->addWidget(pwdLab, 1);
				ph_httpLogin->addWidget(m_pwd, 1);
				ph_httpLogin->addWidget(connBtn, 1);
				ph_httpLogin->addWidget(disconnBtn, 1);
				ph_httpLogin->addStretch();

			}
			ph_v->addLayout(ph_httpLogin);

			QHBoxLayout* ph_ws = new QHBoxLayout();
			{
				QLabel* titleLab = new QLabel(u8"������URL");
				m_wsUrl = new QLineEdit();
				m_wsUrl->setPlaceholderText(u8"�����볤����URL");

				QPushButton* updateBtn = new QPushButton(u8"����");
				m_btnGroup->addButton(updateBtn, EUI::EIOT_WSChange);

				ph_ws->addWidget(titleLab, 1);
				ph_ws->addWidget(m_wsUrl, 1);
				ph_ws->addWidget(updateBtn, 1);
				ph_ws->addStretch();

			}
			ph_v->addLayout(ph_ws);


			QHBoxLayout* ph_http = new QHBoxLayout();
			{
				QLabel* urlLab = new QLabel(u8"HTTP_URL");
				m_httpUrl = new QLineEdit();
				m_httpUrl->setPlaceholderText(u8"������HTTP_URL");
				QLabel* portLab = new QLabel(u8"Port");
				m_httpPort = new QLineEdit();
				m_httpPort->setPlaceholderText(u8"������HTTP_port");
				QPushButton* updateBtn = new QPushButton(u8"����");
				m_btnGroup->addButton(updateBtn, EUI::EIOT_HttpChange);


				ph_http->addWidget(urlLab, 1);
				ph_http->addWidget(m_httpUrl, 1);
				ph_http->addWidget(portLab, 1);
				ph_http->addWidget(m_httpPort, 1);
				ph_http->addWidget(updateBtn, 1);
				ph_http->addStretch();


			}
			ph_v->addLayout(ph_http);
		}
	}
	pv_main->addWidget(workGroup);



	//TODO�����ø��°�ť
	for (const auto& it : m_btnGroup->buttons())
	{
		if (m_btnGroup->id(it) == EUI::EIOT_HttpChange ||
			m_btnGroup->id(it) == EUI::EIOT_WSChange)
		{
			it->setEnabled(false);
		}
	}
	//���³�ʼ״̬
	//OnUpdateConnStatus(false);
}


void StoreLoginUI::PrintLogInfo(const QString& msg)
{
	LOG_INFO(QString(msg));
	std::cout << msg.toStdString().c_str() << std::endl;
	//m_logDisplayEdit->append(msg);
	//todo: log_ouptu
	//PrintLogInfo(QString("Connection status: %1").arg(status_text));
}

void StoreLoginUI::OnBtnClicked(int btnIdx)
{

	switch (btnIdx)
	{
		case EUI::EIOT_Conn:
		{
			m_loginInfo.loginUserName = m_userName->text().toStdString();
			m_loginInfo.loginPwd = m_pwd->text().toStdString();
			m_loginInfo.wsUrl = m_wsUrl->text().toStdString();
			m_loginInfo.httpUrlBase = m_httpUrl->text().toStdString();
			m_loginInfo.httpPort = m_httpPort->text().toStdString();
			emit SigLoinSerOper(m_loginInfo);
			//����������comm_mgr�����е�¼����
			break;
		}
		case EUI::EIOT_Disconn:
		{
			emit SigLogoutSerOper(m_loginInfo);
			break;
		}
		case EUI::EIOT_Login:
		{
			emit SigStoreIdLogin();
			break;
		}
		case EUI::EIOT_LoginBindDev:
		{
			emit SigLoginBindDev();
			break;
		}
		case EUI::EIOT_OSSToken:
		{
			break;
		}
		case EUI::EIOT_WSChange:
		{
			break;
		}
		case EUI::EIOT_TestStates1:
		{
			emit SigTestOper();
			break;
		}
		case  EUI::EIOT_TestStates2:
		{
			emit SigTestOper2();
			break;
		}
		case  EUI::EIOT_SimulateData:
		{
			//����
			auto bClicked = m_btnGroup->button(EUI::EIOT_SimulateData)->isChecked();
			//emit SigSimulateDataOper(bClicked);

			//g_simulateReturnData = true;
			if (g_simulateReturnData && !bClicked)
			{
				g_simulateReturnData = false;
				m_btnGroup->button(EUI::EIOT_SimulateData)->setChecked(false);
			}
			else if (!g_simulateReturnData && bClicked)
			{
				g_simulateReturnData = true;
				m_btnGroup->button(EUI::EIOT_SimulateData)->setChecked(true);
			}
			break;
		}
		default:
			break;
	}
}

void StoreLoginUI::NetworkStatusChanged(ConnectionType connType, NetworkStatus status, const QString& message /*= ""*/)
{
	QString connTypeStr = (connType == ConnectionType::WEBSOCKET) ? "WebSocket" : "HTTP";
	QString statusStr;
	QColor statusColor;

	switch (status) 
	{
	case NetworkStatus::DISCONNECTED:
		statusStr = u8"�Ͽ�����";
		statusColor = Qt::red;
		break;
	case NetworkStatus::CONNECTING:
		statusStr = u8"������";
		statusColor = Qt::blue;
		break;
	case NetworkStatus::CONNECTED:
		statusStr = u8"������";
		statusColor = Qt::darkGreen;
		break;
	case NetworkStatus::RECONNECTING:
		statusStr = u8"������";
		statusColor = QColor(255, 165, 0); // ��ɫ
		break;
	case NetworkStatus::FAILED:
		statusStr = u8"����ʧ��";
		statusColor = Qt::red;
		break;
	}

	// �������Ҫ����״̬���£�������״̬��ʾ
	if (connType == ConnectionType::WEBSOCKET) 
	{
		m_status->setText(statusStr);
		m_status->setStyleSheet(QString("QLabel { color : %1; font-weight: bold; }").arg(statusColor.name()));

		// ���°�ť״̬
		if (status == NetworkStatus::CONNECTED)
		{
			m_btnGroup->button(EUI::EIOT_Conn)->setEnabled(false);
			m_btnGroup->button(EUI::EIOT_Disconn)->setEnabled(true);
		}
		else
		{
			m_btnGroup->button(EUI::EIOT_Conn)->setEnabled(true);
			m_btnGroup->button(EUI::EIOT_Disconn)->setEnabled(false);
		}
	}

	//QString statusStr = bConn ? u8"������" : u8"δ����";
	//QColor statusColor = bConn ? Qt::darkGreen : Qt::red;
	//m_status->setText(statusStr);
	//m_status->setStyleSheet(QString("QLabel { color : %1; font-weight: bold; }").arg(statusColor.name()));
	PrintLogInfo(QString("Connection status: %1").arg(statusStr));
}

// �����ӿڣ�ver_1027
void StoreLoginUI::ConnStatucChange(bool bConn)
{
	QString statusStr = bConn ? u8"������" : u8"δ����";
	QColor statusColor = bConn ? Qt::darkGreen : Qt::red;
	m_status->setText(statusStr);
	m_status->setStyleSheet(QString("QLabel { color : %1; font-weight: bold; }").arg(statusColor.name()));
	PrintLogInfo(QString("Connection status: %1").arg(statusStr));

	if (bConn)
	{
		m_btnGroup->button(EUI::EIOT_Conn)->setEnabled(false);
		m_btnGroup->button(EUI::EIOT_Disconn)->setEnabled(true);
	}
	else
	{
		m_btnGroup->button(EUI::EIOT_Conn)->setEnabled(true);
		m_btnGroup->button(EUI::EIOT_Disconn)->setEnabled(false);
	}
}

//��չ�ӿ�
void StoreLoginUI::ReconnectAttempt(ConnectionType connType, int attemptCount, int maxAttempts)
{
	QString connTypeStr = (connType == ConnectionType::WEBSOCKET) ? "WebSocket" : "HTTP";
	QString message = QString("%1 ��������: �� %2/%3 ��").arg(connTypeStr).arg(attemptCount).arg(maxAttempts);

	// ����״̬��ʾ
	m_status->setText(message);
	m_status->setStyleSheet("QLabel { color : orange; font-weight: bold; }");

	PrintLogInfo(message);

	// ���������н������Ӱ�ť�����öϿ���ť�������û�ȡ��������
	if (m_btnGroup) 
	{
		m_btnGroup->button(0)->setEnabled(false); // ���Ӱ�ť����
		m_btnGroup->button(1)->setEnabled(true);  // �Ͽ���ť����
	}
}

void StoreLoginUI::ReconnectFailed(ConnectionType connType, const QString& reason)
{
	QString connTypeStr = (connType == ConnectionType::WEBSOCKET) ? "WebSocket" : "HTTP";
	QString message = QString("%1 ����ʧ��: %2").arg(connTypeStr).arg(reason);

	// ����״̬��ʾ
	m_status->setText(message);
	m_status->setStyleSheet("QLabel { color : red; font-weight: bold; }");

	PrintLogInfo(message);

	// ����ʧ�ܺ�ָ����Ӱ�ť
	UpdateButtonStates(false);
}

