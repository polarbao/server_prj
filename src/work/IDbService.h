#pragma once
#include <string>
#include <vector>
#include <optional>
#include "comm/MessageDefine.h"

/**
 * @brief 数据库服务抽象接口类
 */
class IDbService
{
public:
    virtual ~IDbService() = default;

    /**
     * @brief 初始化数据库，创建表结构和索引
     * @param dbPath 数据库文件路径
     * @return 是否成功
     */
    virtual bool Initialize(const std::string& dbPath) = 0;

    // --- 任务持久化管理 ---
    
    /**
     * @brief 保存或更新任务信息
     * @param task 业务任务结构
     * @return 是否成功
     */
    virtual bool SaveTask(const BusinessTask& task) = 0;

    /**
     * @brief 更新任务状态
     * @param taskId 任务ID (对应 proId)
     * @param status 任务状态描述/值
     * @param errorMsg 错误说明
     * @return 是否成功
     */
    virtual bool UpdateTaskStatus(const std::string& taskId, int status, const std::string& errorMsg = "") = 0;

    /**
     * @brief 获取单个任务详情
     * @param taskId 任务ID (对应 proId)
     * @return 任务结构
     */
    virtual std::optional<BusinessTask> GetTask(const std::string& taskId) = 0;

    /**
     * @brief 根据状态筛选任务列表
     * @param status 状态值
     * @return 任务列表
     */
    virtual std::vector<BusinessTask> GetTasksByStatus(int status) = 0;

    // --- 离线网络消息暂存队列 ---

    /**
     * @brief 写入待发送的离线同步消息
     * @param payloadType 消息类型描述
     * @param payloadData 消息体 JSON 字符串
     * @return 是否成功
     */
    virtual bool EnqueueOfflineMessage(const std::string& payloadType, const std::string& payloadData) = 0;

    /**
     * @brief 读取一定数量的离线暂存消息
     * @param limit 限制读取条数
     * @return 消息元组列表 (queue_id, payload_type, payload_data)
     */
    virtual std::vector<std::tuple<int, std::string, std::string>> DequeueOfflineMessages(int limit) = 0;

    /**
     * @brief 从暂存队列删除已被成功同步的消息
     * @param queueId 队列ID
     * @return 是否成功
     */
    virtual bool DeleteOfflineMessage(int queueId) = 0;

    // --- 设备配置与拓扑存储 ---

    /**
     * @brief 保存或更新设备拓扑信息
     * @param dev 设备信息
     * @return 是否成功
     */
    virtual bool SaveDeviceTopology(const DeviceInfo& dev) = 0;

    /**
     * @brief 获取所有本地注册的设备拓扑
     * @return 设备列表
     */
    virtual std::vector<DeviceInfo> GetDeviceTopologies() = 0;
};
