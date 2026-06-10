#include "CLogManager.h"

#define LOG_FILE_MAX_SIZE 10*1024*1024
#define LOG_FILE_NAME_0 "/log0.txt"
#define LOG_FILE_UTF8_HEADER1 0xEF
#define LOG_FILE_UTF8_HEADER2 0xBB
#define LOG_FILE_UTF8_HEADER3 0xBF


static QMutex gMutex;
CLogManager* CLogManager::m_pInstance = NULL;
CLogManager::CLogManager(QObject* parent)
:QObject(parent),
CLogThreadCallBack(),
m_bWriteable(true), 
m_logFile0(this),
m_logThread(this, this)
{
    m_cUTF8[0] = LOG_FILE_UTF8_HEADER1;
    m_cUTF8[1] = LOG_FILE_UTF8_HEADER2;
    m_cUTF8[2] = LOG_FILE_UTF8_HEADER3;
    m_lstLogData.clear();
    m_bStop = false;

	DBOutputCallBack* dboutput = new DBOutputCallBack;
	m_pLogOutputCallBack = dboutput;
}


CLogManager* CLogManager::getInstance()
{
    if (m_pInstance == NULL)
    {
        gMutex.lock();
        if (m_pInstance == NULL)
        {
            m_pInstance = new CLogManager;
        }
        gMutex.unlock();
    }

    return m_pInstance;
}


CLogManager::~CLogManager(void)
{
    stopLog();
}


CLogManager::CGarbo CLogManager::m_Garbo;

void CLogManager::startLog(const QString& strLogPath)
{
	if (m_logThread.isRunning())
	{
		return;
	}

    m_strLogFilePath = strLogPath;
    m_strLogFilePath += "log";
    QDir dir;
    if (!dir.exists(m_strLogFilePath))
    {
        if (!dir.mkpath(m_strLogFilePath))
        {
            qDebug() << "dir.mkpath failed.";
        }
    }

    QString strLogFileName = m_strLogFilePath;
    strLogFileName += LOG_FILE_NAME_0;
    m_logFile0.setFileName(strLogFileName);

    if (!m_logFile0.open(QIODevice::Append | QIODevice::Text))
    {
        qDebug() << "Create log file 0 failed.";
    }

    m_bStop = false;
    m_logThread.start();
}

void CLogManager::stopLog()
{
    m_bStop = true;
	if (m_logThread.isRunning())
	{
		m_logThread.wait();
	}
   
    m_logFile0.close();
}


void CLogManager::log(ClientLogLevel_t level, const char* module, const char* format, ...)
{
    va_list args;
    char buffer[1024*10] = {0};
    va_start(args, format);
    vsprintf(buffer, format, args);
    va_end(args);

    LogData_t logData;
    logData.dt = QDateTime::currentDateTime();
    logData.strModule = QString(module);
    logData.thread = QThread::currentThreadId();
    logData.level = level;
    logData.strLog = QString::fromLatin1(buffer);
    QString strLogData = formatLog(logData);
   
    m_logWriteLock.lockForWrite();
    m_lstLogData.append(logData);
    m_logWriteLock.unlock();
}

void CLogManager::logA(ClientLogLevel_t level, const char* module, const char* format, ...)
{
    va_list args;
    char buffer[1024*10] = {0};
    va_start(args, format);
    vsprintf(buffer, format, args);
    va_end(args);

    LogData_t logData;
    logData.dt = QDateTime::currentDateTime();
    logData.strModule = QString(module);
    logData.thread = QThread::currentThreadId();
    logData.level = level;
    logData.strLog = QString::fromLatin1(buffer);
    QString strLogData = formatLog(logData);
    qDebug() << strLogData;

    m_logWriteLock.lockForWrite();
    m_lstLogData.append(logData);
    m_logWriteLock.unlock();
}

void CLogManager::log(ClientLogLevel_t level, const char* module,const wchar_t* format, ...)
{
    va_list args;
    wchar_t buffer[1024*10] = {0};
    va_start(args, format);
    vswprintf(buffer, 1024*10, format, args);
    va_end(args);

    LogData_t logData;
    logData.dt = QDateTime::currentDateTime();
    logData.strModule = QString(module);
    logData.thread = QThread::currentThreadId();
    logData.level = level;
    logData.strLog = QString::fromWCharArray(buffer);
    QString strLogData = formatLog(logData);
 
    m_logWriteLock.lockForWrite();
    m_lstLogData.append(logData);
    m_logWriteLock.unlock();
}

void CLogManager::logW(ClientLogLevel_t level, const char* module,const wchar_t* format, ...)
{
    va_list args;
    wchar_t buffer[1024*10] = {0};
    va_start(args, format);
    vswprintf(buffer, 1024*10, format, args);
    va_end(args);

    LogData_t logData;
    logData.dt = QDateTime::currentDateTime();
    logData.strModule = QString(module);
    logData.thread = QThread::currentThreadId();
    logData.level = level;
    logData.strLog = QString::fromWCharArray(buffer);
    QString strLogData = formatLog(logData);
    qDebug() << strLogData;

    m_logWriteLock.lockForWrite();
    m_lstLogData.append(logData);
    m_logWriteLock.unlock();
}

void CLogManager::setWriteable(bool bWriteable)
{
    m_bWriteable = bWriteable;
}

bool CLogManager::getWriteable()
{
    return m_bWriteable;
}

void CLogManager::setLogOutputCallBack(CLogOutputCallBack* pLogOutputCallBack)
{
    m_pLogOutputCallBack = pLogOutputCallBack;
}

void CLogManager::writeLog(const QString& strLog)
{
    QFile *pFile = NULL;
    qint64 iSize = 0;

	iSize = m_logFile0.size();
	if (iSize >= LOG_FILE_MAX_SIZE)      // log0.txt�Ѵﵽ��󳤶�, �л���log1.txt
	{
		m_logFile0.close();
		QString strNewFileName = m_strLogFilePath + "/" + QDateTime::currentDateTime().toString("log_yyyyMMddhhmmss") + ".txt";
		bool bRet = m_logFile0.copy(strNewFileName);
		if (!bRet)
		{
			qDebug() << "copy strNewFileName failed.";
		}
		m_logFile0.resize(0);
		if (!m_logFile0.open(QIODevice::Append | QIODevice::Text))
		{
			qDebug() << "Create m_logFile0 failed.";
		}
		pFile = &m_logFile0;
	}
	else
	{
		pFile = &m_logFile0;
	}

    // �������Զ�����
    if (!pFile->exists())
    {
        if (!pFile->open(QIODevice::Append | QIODevice::Text))
        {
            qDebug() << "Create pFile failed.";
        }
    }

    // �����ļ���ʽΪ��BOM utf8��ʽ
    if(iSize == 0)
    {
        pFile->write(m_cUTF8, 3);
    }

    QTextStream out(pFile);
    out.setCodec("UTF-8");
    out << strLog << "\n";
    pFile->flush();
}

QString CLogManager::formatLog(const LogData_t& logData)
{
    QString strLogData = QString("[%1][%2][%3][%4]:%5")
        .arg(logData.dt.toString("yyyy-MM-dd hh:mm:ss zzz"))
        .arg((unsigned long long)logData.thread)
        .arg(logData.level)
        .arg(logData.strModule)
        .arg(logData.strLog);

    return strLogData;
}

void CLogManager::logRun(CLogThread* pLogThread)
{
    while(1)
    {
        LogData_t logData;
        if(m_logWriteLock.tryLockForRead(10))
        {
            if(m_lstLogData.size() > 0)
            {
                logData = m_lstLogData.first();
                m_lstLogData.removeFirst();
                m_logWriteLock.unlock();

                if (m_bWriteable)   // д��־
                {
                    writeLog(formatLog(logData));
                }

                if (m_pLogOutputCallBack != NULL)   // ��Ϣ����������־
                {
                    m_pLogOutputCallBack->outputLog(logData);
                }
            }
            else
            {
                m_logWriteLock.unlock();
                pLogThread->sleep(30);
            }
        }

        if(m_bStop)         // ȫ����ȡ��������ݣ�Ȼ���˳�
        {
            m_logWriteLock.lockForRead();
            while(m_lstLogData.size() > 0)
            {
                logData = m_lstLogData.first();
                m_lstLogData.removeFirst();

                if(m_bWriteable)
                {
                    writeLog(formatLog(logData));
                }

                //if (m_pLogOutputCallBack != NULL)   // ��Ϣ����������־
                //{
                //    m_pLogOutputCallBack->outputLog(logData);
                //}
            }

            m_logWriteLock.unlock();

            return;
        }
    }
}

void writeLog(ClientLogLevel level, QString msg, const char* file, int line)
{
	char key;
#ifdef _MSC_VER
	key = '\\';
#else
	key = '/';
#endif
	const char* filename = strrchr(file, key) ? strrchr(file, key) + 1 : file;

	QString strSop = msg.left(2040);
	wchar_t sop[2048] = { 0 };
	strSop.toWCharArray(sop);
	QString title;
	title += filename;
	title += "-";
	title += QString::number(line);

	CLogManager::getInstance()->log(level, title.toLocal8Bit().data(), sop);
}

void DBOutputCallBack::outputLog(const LogData_t& logData)
{
	
}
