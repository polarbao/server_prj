#pragma once

#include <QWidget>

#include <QPushButton>
#include <QButtonGroup>

#include <QLineEdit>
#include <QTextEdit>
#include <QLabel>
#include <QListWidget>

#include "MessageDefine.h"




class ScanUI : public QWidget
{
    Q_OBJECT

public:
	ScanUI(QWidget *parent = nullptr);

    ~ScanUI();


	void Init();


	void PrintLogInfo(const QString& msg);

	void SendAllDeviceReg();

	//注册时，同步国有设备状态
	void SyncRegDevStatus(const std::vector<DeviceInfo>& data);
	//工作时，单设备状态发送变化
	void SyncWorkDevStatus(const DeviceInfo& data);


	void UpdateDevInfo(int idx, const DeviceInfo& data);

	void UpdateDevStatusInfo(QLabel* lab, const DeviceStatus& data);



private:
	void InitUI();
	void InitConnect();


	void ClearDevStatus(int idx);
	void GetDevStatusInfo(const DeviceStatus status, QString& devStatus, QColor& devStatusColor);

public slots:
	void OnConnBtnClicked(QAbstractButton* btn);


signals:
	void SigDevDestory(bool bDestory, DeviceInfo& dev);



private:
	//Data
	QTimer* m_devRegTimer;

	QList<QLabel*> m_devLabList;
	QMap<QString, QLabel*> m_LabMap;
	QButtonGroup* m_btnGroup;


	//UI
	QLabel* m_workStatusLab;
	QLabel* m_connStatusLab;


	QTextEdit* m_logDisplayEdit;
	QListWidget* m_devStatusWidget;
	QPushButton* m_connBtn;
	QPushButton* m_disconnBtn;
	QMap<QString, QPushButton*> m_workStartBtn;
	QMap<QString, QPushButton*> m_workStopBtn;


	//add
	QMap<QString, QLabel*> m_devIdLabMap;
	QMap<QString, QLabel*> m_devStatusLabMap;
	QMap<int, QString> m_devMap;



};
