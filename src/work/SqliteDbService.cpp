#include "SqliteDbService.h"
#include "sqlite3.h"
#include "comm/CLogManager.h"
#include <chrono>
#include <iostream>

// 获取当前系统毫秒时间戳
static long long GetCurrentTimestampMs()
{
    auto now = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
}

SqliteDbService::SqliteDbService()
    : m_db(nullptr)
{
}

SqliteDbService::~SqliteDbService()
{
    if (m_db)
    {
        sqlite3_close(m_db);
        m_db = nullptr;
    }
}

bool SqliteDbService::Initialize(const std::string& dbPath)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_dbPath = dbPath;

    // 启用多线程模式以确保并发安全 (SQLITE_OPEN_FULLMUTEX)
    int rc = sqlite3_open_v2(dbPath.c_str(), &m_db, 
                             SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX, 
                             nullptr);
    if (rc != SQLITE_OK)
    {
        std::cerr << "Cannot open database: " << sqlite3_errmsg(m_db) << std::endl;
        if (m_db)
        {
            sqlite3_close(m_db);
            m_db = nullptr;
        }
        return false;
    }

    // 1. 创建任务流水线主表
    std::string sqlTasks = 
        "CREATE TABLE IF NOT EXISTS t_business_task ("
        "  task_id TEXT PRIMARY KEY,"       // 对应 proId
        "  orders_id TEXT NOT NULL,"        // 对应 ordersId
        "  dev_id TEXT NOT NULL,"           // 对应 devId
        "  op TEXT NOT NULL,"               // 操作类型
        "  payload TEXT,"                   // JSON 格式业务数据
        "  status INTEGER DEFAULT 0,"       // 0:Pending, 1:Running, 2:Success, 3:Failed, 4:Cancelled
        "  error_msg TEXT,"
        "  updated_at INTEGER NOT NULL"
        ");";
    if (!ExecuteSql(sqlTasks)) return false;

    // 创建任务索引
    ExecuteSql("CREATE INDEX IF NOT EXISTS idx_task_status ON t_business_task(status);");

    // 2. 创建断网离线缓存上报队列表
    std::string sqlQueue = 
        "CREATE TABLE IF NOT EXISTS t_offline_sync_queue ("
        "  queue_id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  payload_type TEXT NOT NULL,"
        "  payload_data TEXT NOT NULL,"
        "  created_at INTEGER NOT NULL"
        ");";
    if (!ExecuteSql(sqlQueue)) return false;

    // 3. 创建设备拓扑缓存表
    std::string sqlDevice = 
        "CREATE TABLE IF NOT EXISTS t_device_topology ("
        "  dev_id TEXT PRIMARY KEY,"
        "  pro_id TEXT,"
        "  pro_end_time INTEGER,"
        "  dev_type INTEGER,"
        "  dev_status INTEGER"
        ");";
    if (!ExecuteSql(sqlDevice)) return false;

    return true;
}

bool SqliteDbService::ExecuteSql(const std::string& sql)
{
    char* zErrMsg = nullptr;
    int rc = sqlite3_exec(m_db, sql.c_str(), nullptr, nullptr, &zErrMsg);
    if (rc != SQLITE_OK)
    {
        std::cerr << "SQL error: " << zErrMsg << " (Query: " << sql << ")" << std::endl;
        sqlite3_free(zErrMsg);
        return false;
    }
    return true;
}

bool SqliteDbService::SaveTask(const BusinessTask& task)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_db) return false;

    // 使用 INSERT OR REPLACE 兼顾首次写入与状态更新
    const char* sql = "INSERT OR REPLACE INTO t_business_task (task_id, orders_id, dev_id, op, payload, status, updated_at) "
                      "VALUES (?, ?, ?, ?, ?, 0, ?);";
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) return false;

    sqlite3_bind_text(stmt, 1, task.proId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, task.ordersId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, task.devId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, task.op.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, task.data.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 6, GetCurrentTimestampMs());

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return (rc == SQLITE_DONE);
}

bool SqliteDbService::UpdateTaskStatus(const std::string& taskId, int status, const std::string& errorMsg)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_db) return false;

    const char* sql = "UPDATE t_business_task SET status = ?, error_msg = ?, updated_at = ? WHERE task_id = ?;";
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) return false;

    sqlite3_bind_int(stmt, 1, status);
    sqlite3_bind_text(stmt, 2, errorMsg.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 3, GetCurrentTimestampMs());
    sqlite3_bind_text(stmt, 4, taskId.c_str(), -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return (rc == SQLITE_DONE);
}

std::optional<BusinessTask> SqliteDbService::GetTask(const std::string& taskId)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_db) return std::nullopt;

    const char* sql = "SELECT orders_id, task_id, dev_id, op, payload FROM t_business_task WHERE task_id = ?;";
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) return std::nullopt;

    sqlite3_bind_text(stmt, 1, taskId.c_str(), -1, SQLITE_TRANSIENT);

    std::optional<BusinessTask> result = std::nullopt;
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        BusinessTask task;
        task.ordersId = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        task.proId = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        task.devId = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        task.op = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        const char* payload = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        if (payload) task.data = payload;

        result = task;
    }

    sqlite3_finalize(stmt);
    return result;
}

std::vector<BusinessTask> SqliteDbService::GetTasksByStatus(int status)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<BusinessTask> list;
    if (!m_db) return list;

    const char* sql = "SELECT orders_id, task_id, dev_id, op, payload FROM t_business_task WHERE status = ? ORDER BY updated_at ASC;";
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) return list;

    sqlite3_bind_int(stmt, 1, status);

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        BusinessTask task;
        task.ordersId = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        task.proId = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        task.devId = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        task.op = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        const char* payload = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        if (payload) task.data = payload;

        list.push_back(task);
    }

    sqlite3_finalize(stmt);
    return list;
}

bool SqliteDbService::EnqueueOfflineMessage(const std::string& payloadType, const std::string& payloadData)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_db) return false;

    const char* sql = "INSERT INTO t_offline_sync_queue (payload_type, payload_data, created_at) VALUES (?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) return false;

    sqlite3_bind_text(stmt, 1, payloadType.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, payloadData.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 3, GetCurrentTimestampMs());

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return (rc == SQLITE_DONE);
}

std::vector<std::tuple<int, std::string, std::string>> SqliteDbService::DequeueOfflineMessages(int limit)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<std::tuple<int, std::string, std::string>> list;
    if (!m_db) return list;

    const char* sql = "SELECT queue_id, payload_type, payload_data FROM t_offline_sync_queue ORDER BY created_at ASC LIMIT ?;";
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) return list;

    sqlite3_bind_int(stmt, 1, limit);

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        int queueId = sqlite3_column_int(stmt, 0);
        std::string pType = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        std::string pData = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));

        list.push_back(std::make_tuple(queueId, pType, pData));
    }

    sqlite3_finalize(stmt);
    return list;
}

bool SqliteDbService::DeleteOfflineMessage(int queueId)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_db) return false;

    const char* sql = "DELETE FROM t_offline_sync_queue WHERE queue_id = ?;";
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) return false;

    sqlite3_bind_int(stmt, 1, queueId);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return (rc == SQLITE_DONE);
}

bool SqliteDbService::SaveDeviceTopology(const DeviceInfo& dev)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_db) return false;

    const char* sql = "INSERT OR REPLACE INTO t_device_topology (dev_id, pro_id, pro_end_time, dev_type, dev_status) "
                      "VALUES (?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) return false;

    sqlite3_bind_text(stmt, 1, dev.devId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, dev.proId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 3, dev.proEndTime);
    sqlite3_bind_int(stmt, 4, dev.devType);
    sqlite3_bind_int(stmt, 5, static_cast<int>(dev.devStatus));

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return (rc == SQLITE_DONE);
}

std::vector<DeviceInfo> SqliteDbService::GetDeviceTopologies()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<DeviceInfo> list;
    if (!m_db) return list;

    const char* sql = "SELECT dev_id, pro_id, pro_end_time, dev_type, dev_status FROM t_device_topology;";
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) return list;

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        DeviceInfo dev;
        dev.devId = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        
        const char* proId = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        if (proId) dev.proId = proId;
        
        dev.proEndTime = sqlite3_column_int64(stmt, 2);
        dev.devType = sqlite3_column_int(stmt, 3);
        dev.devStatus = static_cast<DeviceStatus>(sqlite3_column_int(stmt, 4));

        list.push_back(dev);
    }

    sqlite3_finalize(stmt);
    return list;
}
