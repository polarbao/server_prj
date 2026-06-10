# hard2Ser 3.0 系统架构设计与重构实现方案

> [!IMPORTANT]
> 本文档定义了 hard2Ser 3.0 重构的技术实现方案，并规划了**面向长期计划（Go 语言重写业务层）的本地进程间通信 (IPC) 的接口预留设计与数据交互格式**。

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
|  - 任务分发与并发调度 (WorkThdMgr)                                              |
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

## 2. 长期计划 C++ 到 Go 平滑演进的 IPC 接口预留 [NEW]

为了实现在长期计划中用 Go 语言重写业务层时，UI 视图层可以保持“零修改”，我们在短期内引入**本地 IPC 数据通道设计**。

### 2.1 短期与长期的 UI-Backend 耦合对比

```
【短期 C++ 本地内存架构】
[ Qt UI 模块 ] --- 直接内存调用 (通过 IWorkThdMgr 虚接口) ---> [ C++ 业务层模块 (在同一进程中) ]

【长期 Go 语言跨进程架构】
[ Qt UI 进程 / Wails 进程 ] <--- Local WebSocket (Port: 19999) ---> [ Go 业务后台进程 ]
```

### 2.2 预留的 JSON IPC 报文设计 (JSON-RPC 规范)
所有的跨模块操作一律序列化为 JSON 字符串。在 C++ UI 中预先封装这一转换，当未来切换为 Go 进程时，只需将底层的虚接口实现替换为向 `127.0.0.1:19999` 发送 WebSocket 即可。

#### 1. 任务下发请求 (UI -> 后台)
```json
{
    "jsonrpc": "2.0",
    "method": "DispatchTask",
    "params": {
        "taskId": "task-20260610-0001",
        "devId": "scanner-01",
        "taskType": "Scan",
        "payload": "{\"trigger\": true, \"timeout\": 5000}"
    },
    "id": 1
}
```

#### 2. 任务执行状态反馈 (后台 -> UI)
```json
{
    "jsonrpc": "2.0",
    "method": "OnTaskStatusUpdate",
    "params": {
        "taskId": "task-20260610-0001",
        "devId": "scanner-01",
        "status": 2,                       // 0:Pending, 1:Running, 2:Success, 3:Failed
        "resultData": "{\"barcode\": \"6901234567890\"}",
        "errorMsg": ""
    },
    "id": null
}
```

#### 3. 设备连接与健康状态上报 (后台 -> UI)
```json
{
    "jsonrpc": "2.0",
    "method": "OnDeviceStatusUpdate",
    "params": {
        "devId": "scanner-01",
        "devType": 1,
        "status": 1,                       // 0:Offline, 1:Online, 2:Fault
        "latencyMs": 12,
        "packetLossRate": 0.0
    },
    "id": null
}
```

---

## 3. 数据库设计 (Database Schema)

引入 SQLite3 记录本地任务流转状态，提供完整的事务（ACID）保障。

### 3.1 数据库结构类设计 ([IDbService.h](file:///e:/__Code/__Work/hard2ser/hard2Ser_3_0/src/work/IDbService.h))
包含任务存储（`SaveTask`）、任务状态更新（`UpdateTaskStatus`）、以及断网离线暂存（`EnqueueOfflineMessage`）等核心虚函数。

---

## 4. 全协议适配实现类设计 (Polymorphic Adapters)

统一使用 `ICommClient` 作为虚基类，在通信层派生出 `TcpCommClient`、`UdpCommClient`、`WSCommClient`、`MqttCommClient` 和 `CurlCommClient` 等子类，实现协议多态。

---

## 5. 新增高可用模块实现规范

包括：
- **看门狗服务 (Watchdog)**：特权监控线程与心跳门限判断。
- **离线同步服务 (OfflineSyncService)**：网络连接断开时，拦截消息落盘，连通后批量补报。
- **配置管理器 (ConfigManager)**：配置文件动态热重载与参数边界安全验证。
- **诊断指标输出接口 (MetricsExporter)**：基于 HTTP 提供 `/metrics` REST API。
- **AES 加密组件 (Crypto)**：敏感数据本地密文存储。
- **状态机调度引擎 (Task FSM Engine)**：杜绝状态竞争。
