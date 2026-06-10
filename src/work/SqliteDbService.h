#pragma once
#include "IDbService.h"
#include <mutex>
#include <tuple>

struct sqlite3;

/**
 * @brief 基于 SQLite3 的本地持久化数据库服务实现类
 */
class SqliteDbService : public IDbService
{
public:
    SqliteDbService();
    ~SqliteDbService() override;

    // --- 生命周期与配置接口 ---
    bool Initialize(const std::string& dbPath) override;

    // --- 任务持久化管理 ---
    bool SaveTask(const BusinessTask& task) override;
    bool UpdateTaskStatus(const std::string& taskId, int status, const std::string& errorMsg = "") override;
    std::optional<BusinessTask> GetTask(const std::string& taskId) override;
    std::vector<BusinessTask> GetTasksByStatus(int status) override;

    // --- 离线网络消息暂存队列 ---
    bool EnqueueOfflineMessage(const std::string& payloadType, const std::string& payloadData) override;
    std::vector<std::tuple<int, std::string, std::string>> DequeueOfflineMessages(int limit) override;
    bool DeleteOfflineMessage(int queueId) override;

    // --- 设备配置与拓扑存储 ---
    bool SaveDeviceTopology(const DeviceInfo& dev) override;
    std::vector<DeviceInfo> GetDeviceTopologies() override;

private:
    // 执行无结果的 SQL 语句 (建表等)
    bool ExecuteSql(const std::string& sql);

private:
    sqlite3* m_db;
    std::string m_dbPath;
    mutable std::mutex m_mutex; // 保证多线程并发安全
};
