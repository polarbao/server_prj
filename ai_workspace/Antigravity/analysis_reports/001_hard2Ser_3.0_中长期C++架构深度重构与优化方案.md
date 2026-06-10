# hard2Ser 3.0 中长期 C++ 架构深度重构与优化方案

本文档为工控网关项目 `hard2Ser 3.0` 在过渡期间及中长期的 C++ 架构深度重构与优化设计方案，旨在使系统完全实现 UI 与核心业务进程解耦、去 Qt 框架依赖以及提高高可用性能。

---

## 1. 物理文件目录重组与规范化方案

### 1.1 当前排布存在的不合理点
- **主窗体散落**：项目的主 UI 控制类 `hard2Ser_2_0.cpp/h/ui/qrc` 目前直接放置于 `src/` 根目录下，这与其内部设立的 `src/ui/` 目录定位冲突，使得 `src/` 根目录下既有业务入口也有 UI 窗体，界限模糊。
- **空目录残留**：`src/work` 中遗留了空的 `manuThd/` 和 `scanThd/` 文件夹，属于历史残留垃圾。

### 1.2 推荐的物理目录结构
重组后的目录排布应体现彻底的**核心分层逻辑**：
```text
src/
├── main.cpp                     # 整个进程唯一的初始化与启动入口
├── comm/                        # 公共基础层 (工具函数、日志、SQLite 零配置依赖)
│   ├── sqlite3.c/h/ext.h        # 本地 SQLite 依赖
│   ├── CommFun.cpp/h            # 跨平台系统工具 (已完全去 Qt)
│   └── MessageDefine.cpp/h      # 协议结构体、统一消息及通用枚举
├── ipc/                         # 通信接口层 (统一多协议 client 适配及 IPC 服务桩)
│   ├── ICommClient.h            # 统一通信通道抽象虚类
│   ├── LocalIpcServer.cpp/h     # 本地 19999 端口 WebSocket 服务端
│   └── WSClientMgr.cpp/h        # 云端网络通信实现 (含本地 SQLite 离线暂存)
├── work/                        # 核心业务层 (硬件控制、业务线程调度及数据持久化)
│   ├── IDbService.h             # 数据库操作接口
│   ├── SqliteDbService.cpp/h    # SQLite3 接口的具体实现
│   ├── workThdMgr.cpp/h         # 业务线程管理器 (集成了 Watchdog 巡检与 IPC 广播)
│   ├── deviceBase.cpp/h         # 各硬件物理基类与派生类 (Scan/Manu/Stick)
│   └── workBase.cpp/h           # 任务业务线程基类与具体线程实现 (Scan/Manu/Stick)
└── ui/                          # GUI 用户交互层 (不含任何核心业务逻辑，仅做数据显示)
    ├── hard2Ser_2_0.cpp/h/ui/qrc# 主窗体逻辑与布局资源 (新移动至此)
    ├── storeLoginUI.cpp/h       # 登录界面
    └── scanUI/manuUI/stickUI.cpp/h # 子业务配置面板
```

### 1.3 物理重组执行步骤
1. **文件移动**：将 `src/hard2Ser_2_0.cpp`、`src/hard2Ser_2_0.h`、`src/hard2Ser_2_0.ui`、`src/hard2Ser_2_0.qrc` 移至 `src/ui/`。
2. **源码引用更新**：
   - 修改 `src/main.cpp` 中的引用路径：由 `#include "hard2Ser_2_0.h"` 变更为 `#include "ui/hard2Ser_2_0.h"`。
3. **清理空目录**：删除 `src/work/manuThd` 和 `src/work/scanThd`。
4. **编译与验证**：重新配置并运行 CMake 编译，验证 `GLOB_RECURSE` 及自动包含是否一切正常。

---

## 2. 过渡期 C++ 架构深度优化计划

在不迁移至 Go 语言重写的过渡期内，可以通过对 C++ 源码做以下改造，让项目达到生产级的健壮度：

### 2.1 核心业务完全脱离 Qt (C++20 标准替代)
- **目标**：让 `comm/`、`ipc/`、`work/` 底层模块的编译依赖完全为 0-Qt，为未来平滑地用 Go 或者是用纯 C++ Daemon 重写后台逻辑做准备。
- **改造内容**：
  1. **线程与并发替换**：使用 C++20 的 `std::jthread` 替代 `QThread`，`std::mutex` 与 `std::unique_lock` 替代 `QMutex` 与 `QMutexLocker`。
  2. **条件变量替换**：使用 `std::condition_variable` 替代原有的信号槽或 Qt 事件泵。
  3. **定时器替换**：在看门狗和网络连接中使用基于 `std::chrono` 与 `std::thread` 的纯 C++ 异步定时器，停止使用 `QTimer`。

### 2.2 统一多协议适配通道 (ICommClient 落地)
- **目标**：实现网络发送机制对具体网络协议的完全透明化。
- **改造内容**：
  1. 基于现有的 `ICommClient.h` 通用虚类，在 `ipc/` 目录下具体实现 `WebSocketCommClient` 及 `MqttCommClient`。
  2. 重构 `WSClientMgrImpl`，令其不再直接包含复杂的 WebSocket 和 HTTP 客户端对象，而是以组合 `std::unique_ptr<ICommClient>` 的形式进行调用。

### 2.3 强类型 JSON 双向序列化
- **目标**：消除繁琐的手动反序列化逻辑，防止业务字段格式解析错误导致的崩溃。
- **改造内容**：
  - 在 `MessageDefine.h` 中，移除手动编写的解析 JSON 方法。
  - 使用 `nlohmann::json` 的 Modern C++ 强类型映射宏，对所有的结构体直接进行反射定义：
    ```cpp
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(BusinessTask, ordersId, proId, devId, op, data)
    ```

### 2.4 指数退避重试与平滑限流重发
- **目标**：防止因断网重连成功的瞬间突发大流量而遭到云端屏蔽或判定为异常。
- **改造内容**：
  - 在 `WSClientMgr` 重新连回时，对 SQLite3 本地缓存队列的 `ProcessCachedMessages()` 加上时间片重传发机制。
  - 采用指数退避（Exponential Backoff）算法，每次发送失败，重试时间乘以 2，并在重传成功后进行平滑的流控（例如限制每秒发送不超过 5 条消息）。
