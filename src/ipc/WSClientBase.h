#pragma once

#include <string>
#include <functional>


class WSClientBase {
public:

	using OnOpen = std::function<void()>;						//ConnectCallback 连接结果回调
	using OnClose = std::function<void()>;	
	using OnMessage = std::function<void(const std::string&)>;	//MessageCallback 消息接收回调
	using OnError = std::function<void(const std::string&)>;

	virtual ~WSClientBase() = default;

	virtual void setOnOpen(OnOpen cb) = 0;
	virtual void setOnClose(OnClose cb) = 0;
	virtual void setOnMessage(OnMessage cb) = 0;
	virtual void setOnError(OnError cb) = 0;


	virtual bool connect(const std::string& url) = 0;
	virtual void close() = 0;
	virtual bool sendText(const std::string& text) = 0;
	//virtual bool isConnected() const = 0;

};

