# hard2Ser 3.0 系统架构设计与重构实现方案

> [!IMPORTANT]
> 本文档定义了 hard2Ser 3.0 重构的技术实现方案，并整合了从历史日志 `log0.txt` 中提取的真实生产环境下的 WebSocket 与 HTTP 报文协议 Schema，以保证重构过程中的绝对兼容。

---

## 1. 系统层级与核心模块架构拓扑

重构后的系统在原有分层基础上，隔离 Qt 框架对非 UI 模块的影响：

```
+---------------------------------------------------------------------------------+
|                              UI 视图层 (View Layer)                             |
|  - ScanUI / ManuUI / StickUI / StoreLoginUI (Qt Widgets / HTML WebUI)           |
+----------------------------------------+----------------------------------------+
                                         | [短期] 接口调用 (IWorkThdMgr) 
                                         | [长期] Local IPC 套接字 (JSON-RPC)
                                         v
+---------------------------------------------------------------------------------+
|                            业务逻辑层 (Business Layer)                          |
|  - 任务分发与并发调度 (WorkThdMgr - 基于设备类型和ID实现动态路由)               |
|  - 设备状态机模型 (DeviceScan, DeviceManu, DeviceStick)                         |
|  - 离线同步服务 (OfflineSyncService), 任务状态机引擎 (TaskFsmEngine)               |
|  - 设备存活心跳检测器 (KeepAliveDetector)                                        |
+----------------------------------------+----------------------------------------+
                                         | 接口隔离 (ICommClient, IDbService)
                                         v
+---------------------------------------------------------------------------------+
|                          通信层 / 基础框架层 (Infrastructure)                   |
|  - 协议适配器子类 (TcpCommClient, UdpCommClient, WSClient, MqttCommClient)       |
|  - 传输适配器子类 (HttpCommClient, FtpCommClient)                                |
|  - 看门狗核心 (Watchdog), 配置管理器 (ConfigManager - 支持热重载与边界检查)        |
+----------------------------------------+----------------------------------------+
                                         | 存取与持久化驱动
                                         v
+---------------------------------------------------------------------------------+
|                              数据持久化层 (Data Layer)                          |
|  - 嵌入式数据库服务 (SQLite3 / SQLiteCpp)                                        |
|  - 阿里云 OSS 上传驱动 (aliCloudOss)                                            |
|  - 本地加密组件 (Crypto - 用于 AES 密码及敏感 Token 加密)                        |
+---------------------------------------------------------------------------------+
```

---

## 2. 真实生产环境协议 Schema 定义 (根据 log0.txt 还原) [NEW]

通过对历史日志分析，提取出以下与云端系统（`moonbii.net`）进行交互的实际报文格式。

### 2.1 HTTP REST 接口协议

#### 1. 设备持续服务查询 (Inquiry Ongoing Service)
*   **请求端点**：`POST /shop/device/ongoing_service/`
*   **请求 Body**：
```json
{
    "device_ids": ["100001", "100002", "100003", "200001", "200002", "200003", "300001", "300002", "300003"]
}
```
*   **响应 Body**：
```json
{
    "status_code": 0,
    "status_msg": "成功",
    "refresh_time": 1764147416
}
```

#### 2. 阿里云 OSS Token 获取
*   **请求端点**：`POST /shop/auth/oss_token`
*   **请求 Body**：
```json
{
    "biz_scene": "hand"
}
```
*   **响应 Body**（包含临时 STS 凭证）：
```json
{
    "status_code": 0,
    "status_msg": "成功",
    "token": {
        "access_key_id": "STS.NXgT45ZCofg8xVp6HKepVeqFt",
        "access_key_secret": "AzrfHK6XBk9DTu1XqEpLSXRSWKxtFYcEA922XcJatsCe",
        "security_token": "CAIS0wJ1q6Ft5B...",
        "bucket": "macbrush-hand",
        "endpoint": "oss-cn-shanghai.aliyuncs.com",
        "region": "cn-shanghai",
        "expire_time": 1764149219
    }
}
```

---

### 2.2 WebSocket 长连接协议

#### 1. 设备列表与状态同步 (Sync Device)
*   **发送报文**（周期上报）：
```json
{
    "msg_id": "000001",
    "msg_type": "sync_device",
    "payload": [
        {
            "device_id": "100001",
            "process_end_time": 1764147476650,
            "process_id": "",
            "status": 2
        }
    ],
    "ts": 1764147416650
}
```
*   **服务器响应**（Type 5）：
```json
{
    "msg": "success",
    "status": "0"
}
```

#### 2. 双向保活心跳 (Heartbeat)
*   **心跳发送**（每 5 秒一次）：
```json
{
    "msg_id": "123456",
    "msg_type": "heartbeat",
    "payload": {
        "data": "ping"
    },
    "ts": 1764147417265
}
```
*   **心跳响应**：收到云端 ACK。

#### 3. 云端任务推送 (Task Push)
*   **云端推送报文**（Type 7）：
```json
{
    "device_id": "200001",
    "nail_model": "http://macbrush-shop-image.oss-cn-shanghai.aliyuncs.com/simulate_scan_data/20250811145822_101.ply",
    "op": 1,
    "process_id": "2025112615385800099113_2",
    "service_id": "2025112615385800099113"
}
```
*   *设计适配点*：`nail_model` 为核心输入 3D 点云文件（.ply），重构的业务分发器需要将此 URL 透传至具体的设备类。

#### 4. 任务处理进度上报 (Sync Process)
*   **发送报文**（Type 6）：
```json
{
    "msg_id": "{e785f3bc-9250-4dcf-95ff-417a88599837}",
    "msg_type": "sync_process",
    "payload": {
        "device_id": "100001",
        "finger_model": "[]",
        "op": 1,
        "process_end_time": 1764147867557,
        "process_id": "2025112616590500059829_1"
    },
    "ts": 1764147567557
}
```

---

## 3. 面向未来的设备无关通用任务协议设计

为了避免“后续增加设备类型时需要频繁修改云端/服务端下发的协议报文”，hard2Ser 3.0 采用了**“通用元数据 + 动态黑盒荷载”**的协议设计思想。

### 3.1 协议设计原则：路由与荷载分离 (Separation of Routing & Payload)
网关的通信层和任务分发器（`WorkThdMgr`）不应该去解析具体硬件设备的私有控制参数。协议报文被拆分为：
1. **统一路由信息 (Metadata)**：包括 `taskId`（任务ID）、`devId`（设备唯一标识）、`devType`（设备大类）和 `action`（操作动作）。网关核心通过这四个字段将任务路由到对应的物理驱动线程。
2. **设备私有荷载 (Black-box Payload)**：一个通用的 JSON 字符串或嵌套 JSON 对象。网关核心层将其视为“黑盒”，不作解包，直接透传给具体的硬件驱动类。

```
                    +------------------------------+
                    |    服务端下发统一协议报文     |
                    +--------------+---------------+
                                   |
                     [ 路由与分发层 (网关核心) ]
               (仅解析 devId/devType，不看 payload 内容)
                                   |
                +------------------+------------------+
                | (路由至 ScanWork)                   | (路由至 LaserPrinter)
                v                                     v
       +------------------+                  +------------------+
       |   DeviceScan 驱动 |                  |  DevicePrinter   |
       |  (仅解析扫码payload) |                  | (解析打印机payload)|
       +------------------+                  +------------------+
```

### 3.2 长期计划 C++ 到 Go 平滑演进的 IPC 接口预留
所有的跨模块操作一律序列化为 JSON 字符串。在 C++ UI 中预先封装这一转换，当未来切换为 Go 进程时，只需将底层的虚接口实现替换为向 `127.0.0.1:19999` 发送 WebSocket 即可。

---

## 4. 数据库设计 (Database Schema)

引入 SQLite3 记录本地任务流转状态，提供完整的事务（ACID）保障。

### 4.1 数据库结构类设计 ([IDbService.h](file:///e:/__Code/__Work/hard2ser/hard2Ser_3_0/src/work/IDbService.h))
包含任务存储（`SaveTask`）、任务状态更新（`UpdateTaskStatus`）、以及断网离线暂存（`EnqueueOfflineMessage`）等核心虚函数。

---

## 5. 全协议适配实现类设计 (Polymorphic Adapters)

统一使用 `ICommClient` 作为虚基类，在通信层派生出 `TcpCommClient`、`UdpCommClient`、`WSCommClient`、`MqttCommClient` 和 `CurlCommClient` 等子类，实现协议多态。

---

## 6. 新增高可用模块实现规范

- **看门狗服务 (Watchdog)**：特权监控线程与心跳门限判断。
- **离线同步服务 (OfflineSyncService)**：网络连接断开时，拦截消息落盘，连通后批量补报。
- **配置管理器 (ConfigManager)**：配置文件动态热重载与参数边界安全验证。
- **诊断指标输出接口 (MetricsExporter)**：基于 HTTP 提供 `/metrics` REST API。
- **AES 加密组件 (Crypto)**：敏感数据本地密文存储。
- **状态机调度引擎 (Task FSM Engine)**：杜绝状态竞争。
