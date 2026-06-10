#pragma once
#include "WebSocketClient.h"

/** 
*  @author      
*  @class       SWSClient 
*  @brief       httpЭ��ͻ���
*/

using namespace hv;

class SWSClient : public WebSocketClient
{
public:
	SWSClient(EventLoopPtr loop = NULL);

	~SWSClient();

	int SetURL(const char* url);
	int TestMultiClientsRunInOneEventLoop(const char* url, int nclients);


private:
	////�Ƿ�ʹ��https
	//bool m_enableHttps = true;
	//int m_timeoutSecond = 3;
	////�����ip
	//std::string m_destIp;
	////����˶˿�
	//unsigned short m_destPort;
	////��¼���ȡ��token
	//std::string m_token;

};
