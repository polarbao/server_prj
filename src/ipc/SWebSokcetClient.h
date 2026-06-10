#pragma once
#include "WebSocketClient.h"

/** 
*  @author      
*  @class       SWSClient 
*  @brief       http协议客户端
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
	////是否使用https
	//bool m_enableHttps = true;
	//int m_timeoutSecond = 3;
	////服务端ip
	//std::string m_destIp;
	////服务端端口
	//unsigned short m_destPort;
	////登录后获取的token
	//std::string m_token;

};
