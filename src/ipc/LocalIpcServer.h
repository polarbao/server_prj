#pragma once
#include <string>
#include <memory>
#include <mutex>
#include <functional>
#include <vector>

namespace hv {
    class WebSocketServer;
    struct WebSocketChannel;
}

class IWorkThdMgr;
struct BusinessTask;
struct DeviceInfo;

/**
 * @brief 本地进程间通信 (IPC) WebSocket 服务端类
 * @note 用于短期内为长期 Go 语言重写业务层做接口预留，实现 UI 与后台的跨进程解耦交互
 */
class LocalIpcServer
{
public:
    LocalIpcServer(IWorkThdMgr* workMgr);
    ~LocalIpcServer();

    /**
     * @brief 启动本地 IPC WebSocket 监听服务
     * @param port 监听端口，默认 19999
     * @return 是否成功启动
     */
    bool Start(int port = 19999);

    /**
     * @brief 停止本地 IPC 服务
     */
    void Stop();

    /**
     * @brief 向所有连接的 UI 客户端推送消息
     * @param jsonMsg JSON 格式消息
     */
    void BroadcastToUi(const std::string& jsonMsg);

    /**
     * @brief 推送任务状态变更通知给 UI
     */
    void SendTaskStatusUpdate(const std::string& taskId, int status, const std::string& devId, const std::string& errorMsg = "");

    /**
     * @brief 推送设备状态变更通知给 UI
     */
    void SendDeviceStatusUpdate(const DeviceInfo& devInfo);

private:
    // 处理从 UI 客户端接收到的消息
    void HandleUiMessage(const std::string& message, const std::shared_ptr<hv::WebSocketChannel>& channel);

private:
    IWorkThdMgr* m_workMgr;
    std::unique_ptr<hv::WebSocketServer> m_server;
    std::vector<std::shared_ptr<hv::WebSocketChannel>> m_channels;
    std::mutex m_channelsMtx;
    bool m_bRunning;
};
