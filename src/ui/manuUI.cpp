#include "manuUI.h"
#include "global.h"

#include <QGridLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QUuid>
#include <QMap>


//#include "hv\WebSocketClient.h"
//using namespace hv;


ManuUI::ManuUI(QWidget *parent)
	: QWidget(parent)
	, m_btnGroup(new QButtonGroup())
{
	Init();
}

ManuUI::~ManuUI()
{


}

void ManuUI::Init()
{
	InitUI();
	InitConnect();

	m_devRegTimer = new QTimer(this);
	//connect(m_devRegTimer, &QTimer::timeout, this, &ManuUI::OnSendDevReg);
	m_devRegTimer->setSingleShot(false);
	//ÿ10�뷢��һ���豸ע����Ϣ
	m_devRegTimer->start(10000);

	//��ȡ��ǰ�豸״̬
	//����豸
}

void ManuUI::InitConnect()
{
	connect(m_btnGroup, QOverload<QAbstractButton*>::of(&QButtonGroup::buttonClicked), this, &ManuUI::OnConnBtnClicked);
}

void ManuUI::ClearDevStatus(int idx)
{
	QString slotKey = QString("slot_%1").arg(idx);

	// ����豸ID��ʾ
	if (m_LabMap.contains(slotKey))
	{
		QLabel* devIdLabel = m_devIdLabMap[slotKey];
		devIdLabel->setText("δ����");
		devIdLabel->setToolTip("");
	}

	// ����豸״̬��ʾ
	if (m_devStatusLabMap.contains(slotKey))
	{
		QLabel* statusLabel = m_devStatusLabMap[slotKey];
		statusLabel->setText("����");
		statusLabel->setStyleSheet("QLabel { color: gray; font-weight: bold; }");
	}

	// ��ղ�λӳ��
	m_devMap.remove(idx);
}



void ManuUI::GetDevStatusInfo(const DeviceStatus status, QString& devStatus, QColor& devStatusColor)
{
	switch (status)
	{
	case DeviceStatus::IDLE:
	{
		devStatusColor = Qt::green;
		devStatus = QString::fromLocal8Bit("����");
		break;
	}
	case DeviceStatus::BUSY:
	{
		devStatusColor = QColor(255, 165, 0);
		devStatus = QString::fromLocal8Bit("æµ");
		break;
	}
	case DeviceStatus::ERR:
	{
		devStatusColor = Qt::red;
		devStatus = QString::fromLocal8Bit("����");
		break;
	}
	case DeviceStatus::OFFLINE:
	{
		devStatusColor = Qt::gray;
		devStatus = QString::fromLocal8Bit("����");
		break;
	}
	default:
	{
		devStatusColor = Qt::black;
		devStatus = QString::fromLocal8Bit("δ֪");
		break;
	}

	}
}

void ManuUI::InitUI()
{
	//main_layout
	QVBoxLayout* pv_main = new QVBoxLayout(this);
	this->setLayout(pv_main);

	//m_connStatusLab = new QLabel("Disconn", this);
	//m_connStatusLab->setStyleSheet("QLabel { color : red; font-weight: bold; }");
	//pv_main->addWidget(m_connStatusLab);


	QVBoxLayout* pv_work = new QVBoxLayout();
	{
		QGroupBox* workGroup = new QGroupBox(u8"����ģ��");
		{
			QVBoxLayout* ph_v = new QVBoxLayout(workGroup);
			{
				QHBoxLayout* ph_lab = new QHBoxLayout();
				{
					QLabel* workLab = new QLabel(u8"�����߳�");
					QLabel* workStatus = new QLabel(u8"�߳�״̬");
					ph_lab->addWidget(workLab, 1);
					ph_lab->addWidget(workStatus, 1);

				}
				ph_v->addLayout(ph_lab);

				QHBoxLayout* ph_btn = new QHBoxLayout();
				{

					QPushButton* startBtn = new QPushButton(u8"��ʼ�߳�", this);
					QPushButton* stopBtn = new QPushButton(u8"ֹͣ�߳�", this);
					QPushButton* addBtn = new QPushButton(u8"�����豸", this);
					ph_btn->addWidget(startBtn, 1);
					ph_btn->addWidget(stopBtn, 1);
					ph_btn->addWidget(addBtn, 1);

					//TODO: �����̹߳����ť
					startBtn->setEnabled(false);
					stopBtn->setEnabled(false);
					addBtn->setEnabled(false);

				}
				ph_v->addLayout(ph_btn);
				QGroupBox* devGroup1 = new QGroupBox(u8"�豸1");
				{
					//ph���з�װ
					QHBoxLayout* pv_devUnit = new QHBoxLayout(devGroup1);
					{
						//devID devStatus

						//title
						QVBoxLayout* pv_devID = new QVBoxLayout();
						{
							QLabel* devLab = new QLabel();
							devLab->setText(u8"�豸ID");

							QLabel* devID = new QLabel();
							devID->setText("00");
							devID->setObjectName("devID_0");
							m_devIdLabMap["slot_0"] = devID;
							m_LabMap[devID->text()] = devID;

							pv_devID->addWidget(devLab);
							pv_devID->addWidget(devID);

						}
						pv_devUnit->addLayout(pv_devID, 1);

						//dev_status
						QVBoxLayout* pv_devStatus = new QVBoxLayout();
						{
							QLabel* titleLab = new QLabel();
							titleLab->setText(u8"�豸״̬");

							QLabel* statusLab = new QLabel();
							statusLab->setText("offline");
							statusLab->setObjectName("devStatus_0");
							m_devStatusLabMap["slot_0"] = statusLab;

							pv_devStatus->addWidget(titleLab);
							pv_devStatus->addWidget(statusLab);

						}
						pv_devUnit->addLayout(pv_devStatus, 1);

						//btn_oper
						QVBoxLayout* pv_btnOper = new QVBoxLayout();
						{
							QHBoxLayout* ph_title = new QHBoxLayout();
							{

								QLabel* titleLab = new QLabel();
								titleLab->setText(u8"�豸����");
								ph_title->addWidget(titleLab);
							}
							pv_btnOper->addLayout(ph_title);

							QHBoxLayout* ph_btn = new QHBoxLayout();
							{
								QPushButton* btn1 = new QPushButton(u8"�ַ�");
								QPushButton* btn2 = new QPushButton(u8"����");
								QPushButton* btn3 = new QPushButton(u8"ȡ��");
								QPushButton* btn4 = new QPushButton(u8"���");

								btn1->setProperty("layout", QVariant::fromValue(pv_devID));
								btn2->setProperty("layout", QVariant::fromValue(pv_devID));
								btn3->setProperty("layout", QVariant::fromValue(pv_devID));
								btn4->setProperty("layout", QVariant::fromValue(pv_devID));


								m_btnGroup->addButton(btn1, EUI::EDOT_Dispatch);
								m_btnGroup->addButton(btn2, EUI::EDOT_Stoppage);
								m_btnGroup->addButton(btn3, EUI::EDOT_Cancel);
								m_btnGroup->addButton(btn4, EUI::EDOT_Complete);


								ph_btn->addWidget(btn1);
								ph_btn->addWidget(btn2);
								ph_btn->addWidget(btn3);
								ph_btn->addWidget(btn4);
							}
							pv_btnOper->addLayout(ph_btn);
						}
						pv_devUnit->addLayout(pv_btnOper, 4);


						//std::function<void()> = []() {};
					}
					devGroup1->setLayout(pv_devUnit);
				}

				QGroupBox* devGroup2 = new QGroupBox(u8"�豸2");
				{
					//ph���з�װ
					QHBoxLayout* pv_devUnit = new QHBoxLayout(devGroup2);
					{
						//devID devStatus

						//title
						QVBoxLayout* pv_devID = new QVBoxLayout();
						{
							QLabel* devLab = new QLabel();
							devLab->setText(u8"�豸ID");

							QLabel* devID = new QLabel();
							devID->setText("01");
							m_LabMap[devID->text()] = devID;
							m_devIdLabMap["slot_1"] = devID;

							pv_devID->addWidget(devLab);
							pv_devID->addWidget(devID);

						}
						pv_devUnit->addLayout(pv_devID, 1);

						//dev_status
						QVBoxLayout* pv_devStatus = new QVBoxLayout();
						{
							QLabel* titleLab = new QLabel();
							titleLab->setText(u8"�豸״̬");

							QLabel* statusLab = new QLabel();
							statusLab->setText("offline");
							statusLab->setObjectName("devStatus_1");
							m_devStatusLabMap["slot_1"] = statusLab;


							pv_devStatus->addWidget(titleLab);
							pv_devStatus->addWidget(statusLab);

						}
						pv_devUnit->addLayout(pv_devStatus, 1);

						//btn_oper
						QVBoxLayout* pv_btnOper = new QVBoxLayout();
						{
							QHBoxLayout* ph_title = new QHBoxLayout();
							{

								QLabel* titleLab = new QLabel();
								titleLab->setText(u8"�豸����");
								ph_title->addWidget(titleLab);
							}
							pv_btnOper->addLayout(ph_title);

							QHBoxLayout* ph_btn = new QHBoxLayout();
							{
								QPushButton* btn1 = new QPushButton(u8"�ַ�");
								QPushButton* btn2 = new QPushButton(u8"����");
								QPushButton* btn3 = new QPushButton(u8"ȡ��");
								QPushButton* btn4 = new QPushButton(u8"���");

								btn1->setProperty("layout", QVariant::fromValue(pv_devID));
								btn2->setProperty("layout", QVariant::fromValue(pv_devID));
								btn3->setProperty("layout", QVariant::fromValue(pv_devID));
								btn4->setProperty("layout", QVariant::fromValue(pv_devID));


								m_btnGroup->addButton(btn1, EUI::EDOT_Dispatch);
								m_btnGroup->addButton(btn2, EUI::EDOT_Stoppage);
								m_btnGroup->addButton(btn3, EUI::EDOT_Cancel);
								m_btnGroup->addButton(btn4, EUI::EDOT_Complete);


								ph_btn->addWidget(btn1);
								ph_btn->addWidget(btn2);
								ph_btn->addWidget(btn3);
								ph_btn->addWidget(btn4);
							}
							pv_btnOper->addLayout(ph_btn);
						}
						pv_devUnit->addLayout(pv_btnOper, 4);


						//std::function<void()> = []() {};
					}
					devGroup2->setLayout(pv_devUnit);
				}

				QGroupBox* devGroup3 = new QGroupBox(u8"�豸3");
				{
					//ph���з�װ
					QHBoxLayout* pv_devUnit = new QHBoxLayout(devGroup3);
					{
						//devID devStatus

						//title
						QVBoxLayout* pv_devID = new QVBoxLayout();
						{
							QLabel* devLab = new QLabel();
							devLab->setText(u8"�豸ID");

							QLabel* devID = new QLabel();
							devID->setText("02");
							m_LabMap[devID->text()] = devID;
							m_devIdLabMap["slot_2"] = devID;


							pv_devID->addWidget(devLab);
							pv_devID->addWidget(devID);

						}
						pv_devUnit->addLayout(pv_devID, 1);

						//dev_status
						QVBoxLayout* pv_devStatus = new QVBoxLayout();
						{
							QLabel* titleLab = new QLabel();
							titleLab->setText(u8"�豸״̬");

							QLabel* statusLab = new QLabel();
							statusLab->setText("offline");
							statusLab->setObjectName("devStatus_2");
							m_devStatusLabMap["slot_2"] = statusLab;

							pv_devStatus->addWidget(titleLab);
							pv_devStatus->addWidget(statusLab);

						}
						pv_devUnit->addLayout(pv_devStatus, 1);

						//btn_oper
						QVBoxLayout* pv_btnOper = new QVBoxLayout();
						{
							QHBoxLayout* ph_title = new QHBoxLayout();
							{

								QLabel* titleLab = new QLabel();
								titleLab->setText(u8"�豸����");
								ph_title->addWidget(titleLab);
							}
							pv_btnOper->addLayout(ph_title);

							QHBoxLayout* ph_btn = new QHBoxLayout();
							{
								QPushButton* btn1 = new QPushButton(u8"�ַ�");
								QPushButton* btn2 = new QPushButton(u8"����");
								QPushButton* btn3 = new QPushButton(u8"ȡ��");
								QPushButton* btn4 = new QPushButton(u8"���");

								btn1->setProperty("layout", QVariant::fromValue(pv_devID));
								btn2->setProperty("layout", QVariant::fromValue(pv_devID));
								btn3->setProperty("layout", QVariant::fromValue(pv_devID));
								btn4->setProperty("layout", QVariant::fromValue(pv_devID));


								m_btnGroup->addButton(btn1, EUI::EDOT_Dispatch);
								m_btnGroup->addButton(btn2, EUI::EDOT_Stoppage);
								m_btnGroup->addButton(btn3, EUI::EDOT_Cancel);
								m_btnGroup->addButton(btn4, EUI::EDOT_Complete);


								ph_btn->addWidget(btn1);
								ph_btn->addWidget(btn2);
								ph_btn->addWidget(btn3);
								ph_btn->addWidget(btn4);
							}
							pv_btnOper->addLayout(ph_btn);
						}
						pv_devUnit->addLayout(pv_btnOper, 4);


						//std::function<void()> = []() {};
					}
					devGroup3->setLayout(pv_devUnit);
				}

				ph_v->addWidget(devGroup1);
				ph_v->addWidget(devGroup2);
				ph_v->addWidget(devGroup3);


			}
		}
		pv_work->addWidget(workGroup);
		pv_work->addStretch();
	}
	pv_main->addLayout(pv_work);


	//���ó�����������а�ť
	for (const auto& it : m_btnGroup->buttons())
	{

		if (m_btnGroup->id(it) != EUI::EDOT_Stoppage)
		{
			it->setEnabled(false);
		}
	}
}


void ManuUI::SyncRegDevStatus(const std::vector<DeviceInfo>& data)
{
	//Note: ͬ���豸״̬������״̬

	//clear_dev_info

	int slotIdx = 0;
	for (const auto& it : data)
	{
		//2Ϊ��ǰ�����̶߳�Ӧ��ö��
		if (it.devType != 2)
		{
			continue;
		}
		UpdateDevInfo(slotIdx, it);
		auto devId = QString::fromStdString(it.devId);
		m_devMap[slotIdx] = QString::fromStdString(it.devId);
		slotIdx++;
	}

}

void ManuUI::SyncWorkDevStatus(const DeviceInfo& data)
{
	//�ж��豸�����ڵ�slot
	int slotIdx = 0;
	auto valList = m_devMap.values();
	for (auto i = m_devMap.begin(); i != m_devMap.end(); ++i)
	{

		if (i.value() == QString::fromStdString(data.devId))
		{
			slotIdx = i.key();
		}
	}
	UpdateDevInfo(slotIdx, data);
}

void ManuUI::UpdateDevInfo(int idx, const DeviceInfo& devInfo)
{
	QString slotKey = QString("slot_%1").arg(idx);
	QString	devId = QString::fromStdString(devInfo.devId);

	//����ID��ʾ
	if (m_devIdLabMap.contains(slotKey))
	{
		QLabel* devIdLab = m_devIdLabMap[slotKey];
		devIdLab->setText(devId);
		devIdLab->setToolTip(QString("�豸����: %1").arg(devInfo.devType));

		m_devIdLabMap[devId] = devIdLab;
	}

	//�����豸״̬��ʾ
	if (m_devStatusLabMap.contains(slotKey))
	{
		QLabel* devStatusLab = m_devStatusLabMap[slotKey];
		UpdateDevStatusInfo(devStatusLab, devInfo.devStatus);
		m_devStatusLabMap[devId] = devStatusLab;
	}
}

void ManuUI::UpdateDevStatusInfo(QLabel* lab, const DeviceStatus& data)
{
	if (!lab)
	{
		return;
	}

	QString statusStr;
	QColor statusColor;
	GetDevStatusInfo(data, statusStr, statusColor);
	lab->setText(statusStr);
	lab->setStyleSheet(QString("QLabel { color: %1; font-weight: bold; }").arg(statusColor.name()));
}

void ManuUI::OnConnBtnClicked(QAbstractButton* btn)
{
	//��ȡ��ť������Ӧ��Btn״̬
	auto a1 = btn->text();
	auto a2 = m_btnGroup->id(btn);

	QVariant layoutVar = btn->property("layout");
	if (!layoutVar.canConvert<QVBoxLayout*>())
	{
		LOG_INFO(QString::fromLocal8Bit("�޷���ȡ��ǰ������Ϣ"));
		return;
	}

	//��ȡ��ǰ�豸ID
	QVBoxLayout* layout = layoutVar.value<QVBoxLayout*>();
	QLayoutItem* devItem = layout->itemAt(1);
	if (!devItem)
	{
		LOG_INFO(QString::fromLocal8Bit("��ǰ��������QLabel�ؼ����޷���ȡDev��Ϣ"));
		return;
	}
	QLabel* devLab = qobject_cast<QLabel*>(devItem->widget());
	if (devLab && static_cast<EUI::EDOT>(m_btnGroup->id(btn)) == EUI::EDOT_Stoppage)
	{
		auto devId = devLab->text();
		for (const auto& devIt : m_devMap)
		{
			if (devIt == devId)
			{
				DeviceInfo dev;
				dev.devId = devId.toStdString();
				dev.devType = 2;
				dev.devStatus = DeviceStatus::ERR;
				emit SigDevDestory(true, dev);
				break;
			}
		}
	}
	else
	{
		LOG_INFO(QString::fromLocal8Bit("11111111111111111111"));
	}
}



