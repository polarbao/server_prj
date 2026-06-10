#pragma once

#include <atomic>
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <regex>

#include <QObject>
#include <QMetaType>
#include <QString>
#include <QDateTime>
#include <QMap>

#include "CLogManager.h"



//----------------------------enum-------------------------------------
//----------------------------enum-------------------------------------



//----------------------------UI_ENUM----------------------------
//----------------------------UI_ENUM----------------------------
//----------------------------UI_ENUM----------------------------
namespace EUI
{
	typedef enum DevOperType
	{
		EDOT_Begin,
		EDOT_Dispatch,
		EDOT_Stoppage,
		EDOT_Cancel,
		EDOT_Complete,
		EDOT_End
	}EDOT;
	Q_DECLARE_METATYPE(EDOT)


	typedef enum InitOperType
	{
		EIOT_Begin = EDOT_End + 1,
		EIOT_Conn,
		EIOT_Disconn,
		EIOT_Login,
		EIOT_LoginBindDev,
		EIOT_OSSToken,
		EIOT_HTTPConn,
		EIOT_HTTPDisconn,
		EIOT_WSChange,
		EIOT_HttpChange,
		EIOT_TestStates1,
		EIOT_TestStates2,
		EIOT_SimulateData,	//模拟数据方式
		EIOT_End
	}EIOT;
	Q_DECLARE_METATYPE(EIOT)



	typedef enum MoveUIBtnType
	{
		EMUIBT_Begin = EDOT_End + 1,
		//按钮分类
		EMUIBT_XAxis,
		EMUIBT_YAxis,
		EMUIBT_ZAxis,
		//具体btn类型
		EMUIBT_XAxisForward,
		EMUIBT_XAxisBackward,

		EMUIBT_YAxisForward,
		EMUIBT_YAxisBackword,

		EMUIBT_ZAxisForward,
		EMUIBT_ZAxisBackward,
		EMUIBT_Reset,
		EMUIBT_EmergencyStop,
		EMUIBT_Stop,
		EMUIBT_End
	}EMUIBT;
	Q_DECLARE_METATYPE(EMUIBT)
}

namespace ELOGIC
{
	typedef enum WorkType
	{
		EWT_Begin,
		EWT_ScanWork,
		EWT_ManuWork,
		EWT_StickWork,
		EWT_End
	}EWT;
	Q_DECLARE_METATYPE(EWT)


}


