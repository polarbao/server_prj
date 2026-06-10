#pragma once

#include <QtWidgets/QWidget>
#include <QPushButton>
#include <QLineEdit>
#include <QTextEdit>
#include <QLabel>
#include <QListWidget>

	
#include "WSClientMgr.h"
#include "workThdMgr.h"
#include "comm/MessageDefine.h"
#include "scanUI.h"
#include "manuUI.h"
#include "stickUI.h"
#include "storeLoginUI.h"


class hard2Ser_2_0 : public QWidget
{
    Q_OBJECT

public:
    hard2Ser_2_0(QWidget *parent = nullptr);
    ~hard2Ser_2_0();

	void Init();
	void PrintLogInfo(const QString& msg);
	void SendAllDeviceReg();
	void TestTaskRun();
	
	static bool GetFriFlag();

private:
	void InitUI();
	void InitConnect();

signals:
	void SigGetOSSToken2Http();

public slots:


	// ��������״̬�ı�ۺ���
	void OnUpdateNetConnStatus(ConnectionType netType, NetworkStatus status, const QString& message = "");
	void OnHandleRecvMsg(const QString& msgTypeStr, const WSMsgBase& payloadStr);
	void OnHandleHeartBeatAck();
	void OnHandleTaskStatusUpdate(const QString& taskID, int status, const QString& devID);
	void OnHandleTaskStatusUpdate1(const SyncBusinessTask& devID);
	void OnHandleDevStatusUpdate(const QString& devID, int devType, int status);



	void OnUpdateDevStatusUI(const DeviceInfo& info);
	void OnSendDevReg();
	void OnDevBind(const DevBindResp& data);



	//Note: UI�� ģ���豸�ϱ����ϴ���
	void OnUIDevStoppedUpdate(bool bStopped, const DeviceInfo& dev);
	//Note: UI�㣬�ӷ���˻�ȡ��ǰע���豸�����http��½����л�ȡע���豸
	void OnUIGetRegDevSync();
	//Note: UI�㣬�ӷ���˻�ȡ��ǰִ�������豸״̬�������������������豸ӦΪδ���״̬��
	//��ѯ״̬��Ϊ����̬���򽫶�Ӧ�豸�������������(opRes = 4��
	void OnUIGetDevTaskSync();
	void OnRegDevTaskHandle(const DevSereMapResp& data);
	//Note: ҵ���̷߳����豸״̬��UI��
	void OnWorkDevStatusUpdate(const QString& devId);

	//Note: UI�� ��½��Ϣģ��ģ����Բ���
	void OnHandleUITestOper();
	void OnHandleUITestOper2();
//-------------------------------------------------------
//-------------------------------------------------------
//-------------------------------------------------------
	//store_connect_oper
	//ws_connect
	void OnConnSerOper(const LoginMsg& msg);
	void OnDisConnSerOper();
	void OnHandleStoreLogin();
	void OnHandleGetOSSToken();



private:
	//Data
	std::unique_ptr<IWSClientMgr> m_connMgr;
	std::unique_ptr<IWorkThdMgr> m_workMgr;
	QTimer* m_devRegTimer;
	// 1028_add
	std::vector<SyncBusinessTask> m_unnetFinishTaskQue;


	//UI
	QLabel* m_connStatusLab;
	QTextEdit* m_logDisplayEdit;
	QListWidget* m_devStatusWidget;

	ScanUI m_workUI;
	ManuUI m_manuWorkUI;
	StickUI m_stickWorkUI;
	StoreLoginUI m_loginUI;
};

