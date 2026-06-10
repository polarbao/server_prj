#pragma once
#include <string>
#include <functional>
#include <string_view>
#include <vector>
#include <memory>
#include <optional>

/**
 * @brief 通信协议类型枚举
 */
enum class CommProtocolType
{
    TCP,
    UDP,
    WebSocket,
    MQTT,
    HTTP,
    FTP
};

/**
 * @brief 连接状态枚举
 */
enum class CommConnectionStatus
{
    Disconnected,
    Connecting,
    Connected,
    Faulted
};

/**
 * @brief 抽象的统一数据消息帧结构
 */
struct CommMessage
{
    std::string channel;          // 主题 (如 MQTT Topic)，或 WebSocket 的消息分类，TCP/UDP 可为空
    std::string payload;          // 文本数据负荷 (如 JSON)
    std::vector<uint8_t> rawData; // 二进制原始数据负荷 (可选)
};

/**
 * @brief 统一的多协议通信适配器抽象基类
 */
class ICommClient
{
public:
    virtual ~ICommClient() = default;

    // --- 连接生命周期接口 ---
    
    /**
     * @brief 建立与目标端点的网络连接
     * @param connectionStr 连接描述字符串 (例如 "ws://127.0.0.1:8080" 或 "tcp://192.168.1.10:9000")
     * @return 是否成功启动连接流程
     */
    virtual bool Connect(std::string_view connectionStr) = 0;

    /**
     * @brief 断开与远端的连接并关闭资源
     */
    virtual void Disconnect() = 0;

    /**
     * @brief 判定当前通道是否处于已连接状态
     */
    virtual bool IsConnected() const = 0;

    /**
     * @brief 获取当前通道详细状态
     */
    virtual CommConnectionStatus GetStatus() const = 0;

    // --- 数据双向收发接口 ---

    /**
     * @brief 主动发送消息
     * @param message 统一消息帧
     * @return 发送操作是否执行成功
     */
    virtual bool Send(const CommMessage& message) = 0;

    /**
     * @brief 发布 MQTT 消息 (主要针对 MQTT 等发布订阅协议)
     * @param topic 发布的主题
     * @param payload 消息载荷
     * @return 操作是否成功
     */
    virtual bool Publish(std::string_view topic, const std::string& payload) = 0;

    /**
     * @brief 订阅某个频道/主题
     */
    virtual bool Subscribe(std::string_view topic) = 0;

    /**
     * @brief 取消订阅某个频道/主题
     */
    virtual bool Unsubscribe(std::string_view topic) = 0;

    // --- 大文件传输接口 (支持 FTP / HTTP) ---

    /**
     * @brief 异步/同步上传本地文件到远端路径
     */
    virtual bool UploadFile(std::string_view localPath, std::string_view remotePath) = 0;

    /**
     * @brief 从远端下载文件到本地内存/磁盘
     */
    virtual std::optional<std::vector<uint8_t>> DownloadFile(std::string_view remotePath) = 0;

    // --- 事件回调函数注册 ---
    
    using MessageCallback = std::function<void(const CommMessage&)>;
    using StatusCallback = std::function<void(CommConnectionStatus, std::string_view)>;

    /**
     * @brief 注册接收到远端消息时的异步回调函数
     */
    virtual void RegisterMessageCallback(MessageCallback cb) = 0;

    /**
     * @brief 注册通道网络连接状态变更时的异步回调函数
     */
    virtual void RegisterStatusCallback(StatusCallback cb) = 0;
};
