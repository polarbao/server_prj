#include "WSClientWrapper.h"
#include "hv/hlog.h"
#include "CLogManager.h"


//----------------------------WSClientWrapperImpl----------------------------------------------
//----------------------------WSClientWrapperImpl----------------------------------------------
//----------------------------WSClientWrapperImpl----------------------------------------------

WSClientWrapperImpl::WSClientWrapperImpl()
	: ws_client_(std::make_unique<hv::WebSocketClient>())
{
	on_open_callback_ = []()
	{
		hlogi("WS connected.");
	};

	on_message_callback_ = [](const std::string& msg)
	{
		hlogi("WebSocket received: %s", msg.c_str());
	};

	on_close_callback_ = []()
	{
		hlogi("WS connected.");
	};

	on_error_callback_ = [](int err, const std::string& msg)
	{
		hloge("WebSocket error: %d, %s", err, msg.c_str());
	};


	ws_client_->onopen = [this]() 
	{
		if (on_open_callback_) {
			on_open_callback_();
		}
	};

	ws_client_->onmessage = [this](const std::string& msg) 
	{
		if (on_message_callback_) 
		{
			on_message_callback_(msg);
		}
	};

	ws_client_->onclose = [this]() 
	{
		if (on_close_callback_) {
			on_close_callback_();
		}
	};


	//ws_client_->onconnectfail = [this](int err) 
	//{
	//	if (on_error_callback_) {
	//		on_error_callback_(err, "Connect failed");
	//	}
	//};

}

WSClientWrapperImpl::~WSClientWrapperImpl()
{
	Disconnect();
}

void WSClientWrapperImpl::Connect(const std::string& url)
{
	if (!ws_client_->isConnected())
	{	
		ws_client_->open(url.c_str());
	}
}

void WSClientWrapperImpl::Disconnect()
{
	if (ws_client_->isConnected())
	{
		ws_client_->close();
	}
}

void WSClientWrapperImpl::Send(const std::string& message)
{
	if (ws_client_->isConnected())
	{
		ws_client_->send(message);
	}
}

void WSClientWrapperImpl::SetOnOpenCallback(OnConnectCallback callback)
{
	on_open_callback_ = std::move(callback);
}

void WSClientWrapperImpl::SetOnMessageCallback(OnMessageCallback callback)
{
	on_message_callback_ = std::move(callback);
}

void WSClientWrapperImpl::SetOnCloseCallback(OnDisconnectCallback callback)
{
	on_close_callback_ = std::move(callback);

}

void WSClientWrapperImpl::SetOnErrorCallback(OnErrorCallback callback)
{
	on_error_callback_ = std::move(callback);
}

bool WSClientWrapperImpl::IsConnected() const
{
	return ws_client_->isConnected();

}


//----------------------------WSClientWrapper----------------------------------------------
//----------------------------WSClientWrapper----------------------------------------------
//----------------------------WSClientWrapper----------------------------------------------
WSClientWrapper::WSClientWrapper()
	: p_impl_(std::make_unique<WSClientWrapperImpl>()) 
{
}

WSClientWrapper::~WSClientWrapper() = default;

void WSClientWrapper::Connect(const std::string& url) 
{
	p_impl_->Connect(url);
}

void WSClientWrapper::Disconnect() 
{
	p_impl_->Disconnect();
}

void WSClientWrapper::Send(const std::string& message) 
{
	p_impl_->Send(message);
}

void WSClientWrapper::SetOnOpenCallback(OnConnectCallback callback)
{
	p_impl_->SetOnOpenCallback(std::move(callback));
}

void WSClientWrapper::SetOnMessageCallback(OnMessageCallback callback)
{
	p_impl_->SetOnMessageCallback(std::move(callback));
}

void WSClientWrapper::SetOnCloseCallback(OnDisconnectCallback callback)
{
	p_impl_->SetOnCloseCallback(std::move(callback));
}

void WSClientWrapper::SetOnErrorCallback(OnErrorCallback callback)
{
	p_impl_->SetOnErrorCallback(std::move(callback));
}

bool WSClientWrapper::IsConnected() const 
{
	return p_impl_->IsConnected();
}
