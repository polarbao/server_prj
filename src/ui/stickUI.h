#pragma once

#include <QWidget>

#include <QPushButton>
#include <QButtonGroup>

#include <QLineEdit>
#include <QTextEdit>
#include <QLabel>
#include <QListWidget>

#include "MessageDefine.h"




class StickUI : public QWidget
{
    Q_OBJECT

public:
	StickUI(QWidget *parent = nullptr);
    ~StickUI();


	void Init();

	void SendAllDeviceReg();

	void SyncRegDevStatus(const std::vector<DeviceInfo>& data);

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


	//add
	QMap<QString, QLabel*> m_devIdLabMap;
	QMap<QString, QLabel*> m_devStatusLabMap;
	QMap<int, QString> m_devMap;

};
