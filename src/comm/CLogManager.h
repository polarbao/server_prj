#pragma once

#include <QtCore/QtCore>
#include "CLogThread.h"

//日志宏定义
#define LOG_ERROR(msg)     writeLog(ELogError, msg, __FILE__, __LINE__)
#define LOG_WARN(msg)      writeLog(ELogWarning, msg, __FILE__, __LINE__)
#define LOG_INFO(msg)      writeLog(ELogInfo, msg, __FILE__, __LINE__)
#define LOG_DEBUG(msg)     writeLog(ELogDebug, msg, __FILE__, __LINE__)
#define LOG_FATAL(msg)     writeLog(ELogFatal, msg, __FILE__, __LINE__)

//日志输出等级
typedef enum ClientLogLevel
{
	ELogNone = 0,
	ELogDebug,
	ELogInfo,
	ELogWarning,
	ELogError,
	ELogFatal
} ClientLogLevel_t;

//日志内容信息
typedef struct LogData 
{
	//时间
    QDateTime dt;
	//模块
    QString strModule;
	//线程
    Qt::HANDLE thread;
	//等级
    ClientLogLevel_t level;
	//日志内容
    QString strLog;

    LogData() 
    {
        dt = QDateTime::currentDateTime();
        strModule = "";
        thread = 0;
        level = ELogNone;
        strLog = "";
    }
} LogData_t;

// 消息输出栏，回调类
class CLogOutputCallBack
{
public:
    CLogOutputCallBack() { };
    virtual ~CLogOutputCallBack() { };

    virtual void outputLog(const LogData_t& logData) { Q_UNUSED(logData); };
};

/** 
*  @author      
*  @class       CLogManager 
*  @brief       日志管理类
*/
class CLogManager : public QObject, public CLogThreadCallBack
{
    Q_OBJECT
public:
    static CLogManager* getInstance();

	/** 
	*  @brief       开启日志 
	*  @param[in]    strLogPath: 文字存储路径
	*  @param[out]   
	*  @return                    
	*/
    void startLog(const QString& strLogPath = "./");

	/** 
	*  @brief       停止日志 
	*  @param[in]    
	*  @param[out]   
	*  @return                    
	*/
    void stopLog();

    // 标准字节
    void log(ClientLogLevel_t level, const char* module, const char* format, ...);
    void logA(ClientLogLevel_t level, const char* module, const char* format, ...);

    // 宽字节支持
    void log(ClientLogLevel_t level, const char* module, const wchar_t* format, ...);
    void logW(ClientLogLevel_t level, const char* module, const wchar_t* format, ...);

	/** 
	*  @brief       设置写日志权限 
	*  @param[in]    
	*  @param[out]   
	*  @return                    
	*/
    void setWriteable(bool bWriteable);
    bool getWriteable();

	/** 
	*  @brief       设置日志额外处理回调对象
	*  @param[in]    
	*  @param[out]   
	*  @return                    
	*/
    void setLogOutputCallBack(CLogOutputCallBack* pLogOutputCallBack);

protected:
	virtual void logRun(CLogThread* pLogThread);

private:
	CLogManager(QObject* parent = 0);
	~CLogManager(void);
	void writeLog(const QString& strLog);
	QString formatLog(const LogData_t& logData);

	// 单例内存回收类
	class CGarbo
	{
	public:
		CGarbo() {}
		~CGarbo()
		{
			if (CLogManager::m_pInstance)
			{
				delete CLogManager::m_pInstance;
				CLogManager::m_pInstance = nullptr;
			}
		}
	};
	static CGarbo m_Garbo;

private:
    static CLogManager* m_pInstance;
    bool m_bWriteable;
    QFile m_logFile0;
    char m_cUTF8[3];
    bool m_bStop;

	//多线程对象
    CLogThread m_logThread;

	//读写锁
    QReadWriteLock m_logWriteLock;

	//日志数据队列
    QList<LogData_t> m_lstLogData;

	//扩展日志输出回调对象
    CLogOutputCallBack* m_pLogOutputCallBack;

	//当前操作日志文件名称
    QString m_strLogFilePath;
};

extern "C" void writeLog(ClientLogLevel level, QString msg, const char* file, int line);

/**
*  @author
*  @class       DBOutputCallBack
*  @brief       日志输出到数据库回调类
*/
class DBOutputCallBack : public CLogOutputCallBack
{
public:
	virtual void outputLog(const LogData_t& logData);
};