#include "WSClient.h"


WSClient::WSClient()
	: client_(std::make_unique<hv::WebSocketClient>()) 
{
}

WSClient::~WSClient() 
{
	close();
}

void WSClient::setOnOpen(OnOpen cb) 
{ 
	onOpen_ = std::move(cb); 
}

void WSClient::setOnClose(OnClose cb)
{ onClose_ = std::move(cb); }

void WSClient::setOnMessage(OnMessage cb) 
{ onMessage_ = std::move(cb); }

void WSClient::setOnError(OnError cb)
{ onError_ = std::move(cb); }


bool WSClient::connect(const std::string& url)
{
	if (!client_) return false;

	client_->onopen = [this]() 
	{
		connected_.store(true, std::memory_order_release);
		if (onOpen_) onOpen_();
	};
	client_->onclose = [this]() 
	{
		connected_.store(false, std::memory_order_release);
		if (onClose_) onClose_();
	};
	client_->onmessage = [this](const std::string& msg) 
	{
		if (onMessage_) onMessage_(msg);
	};
	//client_->onerror = [this](const std::string& err) {
	//	if (onError_) onError_(err);
	//};

	return client_->open(url.c_str()) == 0;
}

void WSClient::close() 
{
	if (client_) 
	{
		client_->close();
	}
}

bool WSClient::sendText(const std::string& msg) 
{
	if (!client_ || !connected_.load(std::memory_order_acquire)) return false;
	return client_->send(msg) >= 0;
}

