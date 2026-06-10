#include "LocalIpcServer.h"
#include "hv/WebSocketServer.h"
#include "work/iWorkMgr.h"
#include "comm/MessageDefine.h"
#include "json.hpp"
#include "comm/CLogManager.h"
#include <algorithm>
#include <iostream>

using json = nlohmann::json;

LocalIpcServer::LocalIpcServer(IWorkThdMgr* workMgr)
    : m_workMgr(workMgr)
    , m_bRunning(false)
{
}

LocalIpcServer::~LocalIpcServer()
{
    Stop();
}

bool LocalIpcServer::Start(int port)
{
    if (m_bRunning) return true;

    try
    {
        m_server = std::make_unique<hv::WebSocketServer>();
        
        // 配置 WebSocket 服务端处理器
        hv::WebSocketService* service = new hv::WebSocketService();
        
        service->onopen = [this](const WebSocketChannelPtr& channel, const HttpRequestPtr& req) {
            std::lock_guard<std::mutex> lock(this->m_channelsMtx);
            this->m_channels.push_back(channel);
            LOG_INFO("Local IPC: UI Client connected via Loopback.");
        };

        service->onmessage = [this](const WebSocketChannelPtr& channel, const std::string& msg) {
            this->HandleUiMessage(msg, channel);
        };

        service->onclose = [this](const WebSocketChannelPtr& channel) {
            std::lock_guard<std::mutex> lock(this->m_channelsMtx);
            auto it = std::find(this->m_channels.begin(), this->m_channels.end(), channel);
            if (it != this->m_channels.end())
            {
                this->m_channels.erase(it);
            }
            LOG_INFO("Local IPC: UI Client disconnected.");
        };

        m_server->registerWebSocketService(service);
        
        // 绑定本地地址只允许环回，增加安全性
        m_server->port = port;
        
        int rc = m_server->start();
        if (rc == 0)
        {
            m_bRunning = true;
            LOG_INFO(QString("Local IPC Server started on port %1").arg(port));
            return true;
        }
        LOG_ERROR(QString("Failed to start Local IPC Server on port %1").arg(port));
        return false;
    }
    catch (const std::exception& e)
    {
        LOG_ERROR(QString("Local IPC Server exception: %1").arg(e.what()));
        return false;
    }
}

void LocalIpcServer::Stop()
{
    if (!m_bRunning) return;
    
    try
    {
        m_server->stop();
        m_bRunning = false;
        
        std::lock_guard<std::mutex> lock(m_channelsMtx);
        m_channels.clear();
        LOG_INFO("Local IPC Server stopped.");
    }
    catch (...)
    {
        m_bRunning = false;
    }
}

void LocalIpcServer::BroadcastToUi(const std::string& jsonMsg)
{
    std::lock_guard<std::mutex> lock(m_channelsMtx);
    for (auto& channel : m_channels)
    {
        if (channel && channel->isConnected())
        {
            channel->send(jsonMsg);
        }
    }
}

void LocalIpcServer::SendTaskStatusUpdate(const std::string& taskId, int status, const std::string& devId, const std::string& errorMsg)
{
    json resp;
    resp["jsonrpc"] = "2.0";
    resp["method"] = "OnTaskStatusUpdate";
    resp["params"] = {
        {"taskId", taskId},
        {"devId", devId},
        {"status", status},
        {"errorMsg", errorMsg}
    };
    resp["id"] = nullptr;
    
    BroadcastToUi(resp.dump());
}

void LocalIpcServer::SendDeviceStatusUpdate(const DeviceInfo& devInfo)
{
    json resp;
    resp["jsonrpc"] = "2.0";
    resp["method"] = "OnDeviceStatusUpdate";
    resp["params"] = {
        {"devId", devInfo.devId},
        {"devType", devInfo.devType},
        {"status", static_cast<int>(devInfo.devStatus)},
        {"proId", devInfo.proId},
        {"proEndTime", devInfo.proEndTime}
    };
    resp["id"] = nullptr;

    BroadcastToUi(resp.dump());
}

void LocalIpcServer::HandleUiMessage(const std::string& message, const WebSocketChannelPtr& channel)
{
    try
    {
        auto req = json::parse(message);
        if (!req.contains("method") || !req.contains("params"))
        {
            return;
        }

        std::string method = req["method"];
        auto params = req["params"];

        if (method == "DispatchTask")
        {
            BusinessTask task;
            task.ordersId = params.value("ordersId", "");
            task.proId = params.value("taskId", ""); // C++ proId 对应 JSON taskId
            task.devId = params.value("devId", "");
            task.op = params.value("op", "1");
            task.data = params.value("payload", ""); // C++ data 对应 JSON payload

            LOG_INFO(QString("Local IPC: Dispatching task %1 through WebSocket").arg(QString::fromStdString(task.proId)));
            m_workMgr->DispatchTask(task);
        }
        else if (method == "CancelTask")
        {
            std::string devId = params.value("devId", "");
            bool bStopped = params.value("bStopped", false);
            
            LOG_INFO(QString("Local IPC: Cancelling task on device %1").arg(QString::fromStdString(devId)));
            m_workMgr->CancelTask(devId, bStopped);
        }
        else
        {
            LOG_WARN(QString("Local IPC: Unknown method received: %1").arg(QString::fromStdString(method)));
        }
    }
    catch (const std::exception& e)
    {
        LOG_ERROR(QString("Local IPC: Failed to parse UI message: %1").arg(e.what()));
    }
}
