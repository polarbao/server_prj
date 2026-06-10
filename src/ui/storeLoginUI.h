#pragma once

#include <QWidget>
#include <QPushButton>
#include <QButtonGroup>
#include <QLineEdit>
#include <QTextEdit>
#include <QLabel>
#include <QListWidget>

#include "MessageDefine.h"



class StoreLoginUI : public QWidget
{
    Q_OBJECT

public:
	StoreLoginUI(QWidget *parent = nullptr);

    ~StoreLoginUI();


	void Init();


	void PrintLogInfo(const QString& msg);

	//同步WS长连接状态

	// 1015_add_新增网络状态处理函数
	//同步WS长连接状态
	void ConnStatucChange(bool bConn);		//废弃接口：ver_1027
	void NetworkStatusChanged(ConnectionType connType, NetworkStatus status, const QString& message = "");
	// 扩展接口，供后续进行重连按钮操作使用
	void ReconnectAttempt(ConnectionType connType, int attemptCount, int maxAttempts);
	void ReconnectFailed(ConnectionType connType, const QString& reason);


private:
	void InitUI();
	void InitConnect();
	void SetDefaultParam();

	// 1015_add
	void UpdateButtonStates(bool bConn);


public slots:
	// 按钮组触发操作
	void OnBtnClicked(int btnIdx);


signals:
	void SigLoinSerOper(const LoginMsg& msg);
	void SigLogoutSerOper(const LoginMsg& msg);
	void SigStoreIdLogin();
	void SigLoginBindDev();
	void SigSimulateDataOper(bool bCheckedClick);

	//===测试操作
	void SigTestOper();
	void SigTestOper2();

private:
	//Data
	LoginMsg m_loginInfo;
	QButtonGroup* m_btnGroup;

	//UI
	QLabel* m_status;
	QLineEdit* m_storeId;
	QLineEdit* m_userName;
	QLineEdit* m_pwd;
	QLineEdit* m_wsUrl;
	QLineEdit* m_httpUrl;
	QLineEdit* m_httpPort;
};
