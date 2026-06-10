# hard2Ser 3.0 技术选型与多协议扩展方案

> [!IMPORTANT]
> 本文档定义了本地数据库技术选型、全协议支持的抽象架构，以及**面向长期 Go 语言迁移的本地进程间通信 (IPC) 技术选型**。

---

## 1. 本地数据库与通信库技术选型对比

### 1.1 数据库选型：SQLite3
为了持久化任务的流转状态、重试队列及本地设备配置，采用 **SQLite3 (使用 SQLiteCpp 进行 C++20 的 RAII 风格包装)**。
- *优势*：支持标准 SQL 查询，适合结构化任务（Task）的生命周期管理，在 Go 语言中也有成熟的 `go-sqlite3` 驱动支持，数据表无损对接。

### 1.2 通信库选型：libhv + paho-mqtt-cpp + libcurl
- **TCP / UDP / WebSocket**：采用 `libhv`。
- **MQTT**：采用 `paho-mqtt-cpp`。
- **HTTP / FTP**：采用 `libcurl`。

---

## 2. 长期 Go 迁移的进程间通信 (IPC) 技术选型 [NEW]

在短期 C++ 改造中，为了预留业务后台被 Go 语言重写后与前台 UI 的交互能力，我们需要对本地进程间通信 (Local IPC) 方案进行技术对比：

| 指标 | 方案 A：Local WebSocket (JSON) (推荐) | 方案 B：gRPC (Protobuf over HTTP/2) | 方案 C：共享内存 (Shared Memory) |
| :--- | :--- | :--- | :--- |
| **性能** | **极高** (本地 127.0.0.1 环回，时延 < 1ms) | **高** (序列化开销极小，支持流式传输) | **极致** (纳秒级，几乎无开销) |
| **依赖体积** | **零额外依赖** (复用 libhv 及 nlohmann/json) | **巨大** (需要引入 gRPC, Protobuf, c-ares 等多库) | **中** (需要引入 Boost.Interprocess 等) |
| **C++ 编译复杂度**| **极简** | **复杂** (需要在 CMake 中配置 Protobuf 自动生成) | **中** |
| **Go 语言兼容度**| **完美** (Go 原生 websocket 极易开发) | **完美** (Go 对 gRPC 原生支持) | **差** (Go 缺乏安全且易用的跨语言共享内存包装) |
| **调试友好度** | **完美** (JSON 数据，明文可读，支持工具抓包) | **一般** (二进制报文，调试需要借助 gpb 等) | **极差** (内存调试极其困难) |

### 选型结论
**推荐采用 方案 A：基于本地 Local WebSocket 传输 JSON-RPC 格式报文**。
- **原因**：这对于本地 127.0.0.1 环回网络而言，带宽和延迟完全不是瓶颈。WebSocket 拥有最优的跨语言开发生态（在 C++/Qt 中使用内置的 `QWebSocket`，在 Go 语言中使用 `gorilla/websocket`）。
- 报文序列化选用 `nlohmann-json`，它与 Go 语言内置的 `encoding/json` 具有最直接的键值映射，调试简便，完全契合 **KISS 设计原则**。

---

## 3. 全协议（TCP/UDP/WS/MQTT/HTTP/FTP）抽象架构设计

通信基类 `ICommClient` 提供统一的接口，隐藏底层的套接字差异。

### 3.1 协议适配抽象基类 ([ICommClient.h](file:///e:/__Code/__Work/hard2ser/hard2Ser_3_0/src/ipc/ICommClient.h))
包含 `Connect`、`Disconnect`、`Send`、`Publish`/`Subscribe`、`UploadFile`/`DownloadFile` 以及事件回调绑定。

---

## 4. vcpkg.json 配置
```json
{
  "name": "hard2ser-3-0",
  "version-string": "3.0.0",
  "dependencies": [
    {
      "name": "paho-mqtt-cpp",
      "version-string": ">=1.2.0"
    },
    {
      "name": "curl",
      "features": [
        "openssl",
        "ssh"
      ]
    },
    {
      "name": "sqlite3",
      "version-string": ">=3.35.0"
    },
    {
      "name": "sqlitecpp",
      "version-string": ">=3.1.0"
    },
    "nlohmann-json"
  ]
}
```
