#pragma once

#include <string>
//#include <nlohmann/json.hpp>
#include <functional>
#include <cstdint>

//// 设备状态
//enum class DeviceState {
//    IDLE,       // 空闲
//    BUSY,       // 忙碌
//    OFFLINE,    // 离线
//    ERROR       // 错误
//};
//
//// 业务类型
//enum BusinessType {
//    TYPE1,
//    TYPE2,
//    TYPE3
//};
//
//// 任务状态
//enum TaskState {
//    PENDING,    // 待处理
//    PROCESSING, // 处理中
//    COMPLETED,  // 完成
//    FAILED      // 失败
//};
//
//// 消息类型
//enum MsgType {
//    HEARTBEAT,          // 心跳
//    DEVICE_REGISTER,    // 设备注册
//    DEVICE_STATE,       // 设备状态更新
//    TASK_ASSIGN,        // 任务分配
//    TASK_RESULT,        // 任务结果
//    MSG_ACK             // 消息确认
//};

//// 设备信息结构体
//struct DeviceInfo 
//{
// //   std::string device_id;
// //   BusinessType business_type;
// //   DeviceState state;
// //   std::string desc; // 设备描述
//
// //   nlohmann::json to_json() const 
//	//{
// //       return {
// //           {"device_id", device_id},
// //           {"business_type", static_cast<int>(business_type)},
// //           {"state", static_cast<int>(state)},
// //           {"desc", desc}
// //       };
// //   }
//
// //   static DeviceInfo from_json(const nlohmann::json& j) 
//	//{
// //       return {
// //           j["device_id"],
// //           static_cast<BusinessType>(j["business_type"].get<int>()),
// //           static_cast<DeviceState>(j["state"].get<int>()),
// //           j["desc"]
// //       };
// //   }
//};
//
//// 任务信息结构体
//struct TaskInfo 
//{
// //   std::string task_id;
// //   BusinessType business_type;
// //   std::string device_id; // 目标设备
// //   std::string data;      // 任务数据
// //   TaskState state;
// //   std::string result;    // 任务结果
// //   uint64_t sequence;     // 消息序列号，用于保证顺序
//
// //   nlohmann::json to_json() const 
//	//{
// //       return {
// //           {"task_id", task_id},
// //           {"business_type", static_cast<int>(business_type)},
// //           {"device_id", device_id},
// //           {"data", data},
// //           {"state", static_cast<int>(state)},
// //           {"result", result},
// //           {"sequence", sequence}
// //       };
// //   }
//
// //   static TaskInfo from_json(const nlohmann::json& j) 
//	//{
// //       return {
// //           j["task_id"],
// //           static_cast<BusinessType>(j["business_type"].get<int>()),
// //           j["device_id"],
// //           j["data"],
// //           static_cast<TaskState>(j["state"].get<int>()),
// //           j["result"],
// //           j["sequence"]
// //       };
// //   }
//};
//
//// 通信消息结构体
//struct NetMsg 
//{
// //   MsgType type;
// //   std::string client_id;
// //   nlohmann::json payload;
// //   uint64_t sequence;     // 消息序列号
//
// //   std::string serialize() const 
//	//{
// //       nlohmann::json j;
// //       j["type"] = static_cast<int>(type);
// //       j["client_id"] = client_id;
// //       j["payload"] = payload;
// //       j["sequence"] = sequence;
// //       return j.dump();
// //   }
//
// //   static NetMsg deserialize(const std::string& str) 
//	//{
// //       auto j = nlohmann::json::parse(str);
// //       return {
// //           static_cast<MsgType>(j["type"].get<int>()),
// //           j["client_id"],
// //           j["payload"],
// //           j["sequence"]
// //       };
// //   }
//};
//
