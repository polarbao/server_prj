#pragma once

#include <QtCore/QThread>

class CLogThread;

/** 
*  @author      
*  @class       CLogThreadCallBack 
*  @brief       ��־�̻߳ص���
*/
class CLogThreadCallBack
{
public:
    CLogThreadCallBack() { };
    virtual ~CLogThreadCallBack() { };

	/** 
	*  @brief       ���߳���־ִ�к��� 
	*  @param[in]    
	*  @param[out]   
	*  @return                    
	*/
    virtual void logRun(CLogThread* pLogThread) { };
};

/** 
*  @author      
*  @class       CLogThread 
*  @brief       ���߳���־��
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


