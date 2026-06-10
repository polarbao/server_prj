#include "SWebSokcetClient.h"




SWSClient::SWSClient(EventLoopPtr loop /*= NULL*/)
	: WebSocketClient(loop)
{

}

SWSClient::~SWSClient()
{

}

int SWSClient::SetURL(const char* url)
{
	// set callbacks
	onopen = [this]() 
	{
		const HttpResponsePtr& resp = getHttpResponse();
		printf("onopen\n%s\n", resp->body.c_str());
		// printf("response:\n%s\n", resp->Dump(true, true).c_str());
	};
	onmessage = [this](const std::string& msg) 
	{
		printf("onmessage(type=%s len=%d): %.*s\n", opcode() == WS_OPCODE_TEXT ? "text" : "binary",
			(int)msg.size(), (int)msg.size(), msg.data());
	};
	onclose = []() 
	{
		printf("onclose\n");
	};

	// ping
	setPingInterval(10000);

	// reconnect: 1,2,4,8,10,10,10...
	reconn_setting_t reconn;
	reconn_setting_init(&reconn);
	reconn.min_delay = 1000;
	reconn.max_delay = 10000;
	reconn.delay_policy = 2;
	setReconnect(&reconn);

	/*
	auto req = std::make_shared<HttpRequest>();
	req->method = HTTP_POST;
	req->headers["Origin"] = "http://example.com";
	req->json["app_id"] = "123456";
	req->json["app_secret"] = "abcdefg";
	printf("request:\n%s\n", req->Dump(true, true).c_str());
	setHttpRequest(req);
	*/

	http_headers headers;
	headers["Origin"] = "http://example.com/";
	return open(url, headers);
}

int SWSClient::TestMultiClientsRunInOneEventLoop(const char* url, int nclients)
{
	//auto loop_thread = std::make_shared<hv::EventLoopThread>();
	//loop_thread->start();

	//std::map<int, WSClientPtr> clients;
	//for (int i = 0; i < nclients; ++i) {
	//	SWSClient* client = new SWSClient(loop_thread->loop());
	//	client->SetURL(url);
	//	clients[i] = WSClientPtr(client);
	//}

	//// press Enter to stop
	//while (getchar() != '\n');
	//loop_thread->stop();
	//loop_thread->join();

	//return 0;
	return 0;
}
