#pragma once

#include <QtCore/QThread>

class CLogThread;

/** 
*  @author      
*  @class       CLogThreadCallBack 
*  @brief       日志线程回调类
*/
class CLogThreadCallBack
{
public:
    CLogThreadCallBack() { };
    virtual ~CLogThreadCallBack() { };

	/** 
	*  @brief       多线程日志执行函数 
	*  @param[in]    
	*  @param[out]   
	*  @return                    
	*/
    virtual void logRun(CLogThread* pLogThread) { };
};

/** 
*  @author      
*  @class       CLogThread 
*  @brief       多线程日志类
*/
class CLogThread : public QThread
{
    Q_OBJECT

public:
    CLogThread(QObject * parent, CLogThreadCallBack* pCallBack);
    ~CLogThread();

    void sleep(long ms);
    int exec();
protected:
    virtual void run();
    CLogThreadCallBack* m_pCallBack;
};


