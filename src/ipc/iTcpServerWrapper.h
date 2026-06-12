#pragma once

#include <memory>
#include <string>
#include <functional>
#include <map>

#include <QObject>

#include "MessageDefine.h"

/**
 * @brief TCP服务器接口类
 * @author hard2Ser Team
 * @brief 提供TCP服务器的统一接口，支持多客户端连接管理
 */
class ITcpServerWrapper //: public QObject
{
	//Q_OBJECT
public:
	virtual ~ITcpServerWrapper() = default;
	

	/**
	 * @brief 启动TCP服务器
	 * @param ip 绑定的IP地址，默认为"0.0.0.0"
	 * @param port 监听端口
	 * @return 成功返回true，失败返回false
	 */
	virtual bool Start(const std::string& ip = "0.0.0.0", int port = 8080) = 0;

	
	/**
	 * @brief 停止TCP服务器
	 */
	virtual void Stop() = 0;

	/**
	 * @brief 检查服务器是否正在运行
	 * @return 运行中返回true，否则返回false
	 */
	virtual bool IsRunning() const = 0;

	/**
	 * @brief 获取当前监听的端口
	 * @return 端口号，未启动时返回0
	 */
	virtual int GetPort() const = 0;

	/**
	 * @brief 发送数据到指定连接
	 * @param connId 连接ID
	 * @param data 要发送的数据
	 * @return 成功返回true，失败返回false
	 */
	virtual bool SendData(int connId, const std::string& data) = 0;

	/**
	 * @brief 广播数据到所有连接的客户端
	 * @param data 要广播的数据
	 * @return 成功发送的连接数
	 */
	virtual int BroadcastData(const std::string& data) = 0;

	/**
	 * @brief 断开指定连接
	 * @param connId 连接ID
	 * @return 成功返回true，失败返回false
	 */
	virtual bool DisconnectClient(int connId) = 0;

	/**
	 * @brief 获取当前连接数
	 * @return 连接数
	 */
	virtual int GetConnectionCount() const = 0;

	/**
	 * @brief 获取所有连接信息
	 * @return 连接信息列表
	 */
	virtual std::map<int, TcpConnection> GetAllConnections() const = 0;


	/**
	 * @brief 设置新连接回调
	 * @param callback 回调函数 (TcpConnection)
	 */
	virtual void SetOnConnectionCallback(std::function<void(const TcpConnection&)> callBack) = 0;

	/**
	 * @brief 设置连接断开回调
	 * @param callback 回调函数 (connId)
	 */
	virtual void SetOnDisconnectionCallback(std::function<void(int)> callback) = 0;


	/**
	 * @brief 设置消息接收回调
	 * @param callback 回调函数 (OldTcpMessage)
	 */
	virtual void SetOnMessageCallback(std::function<void(const OldTcpMessage&)> callBack) = 0;

	/**
	 * @brief 设置错误回调
	 * @param callback 回调函数 (errorCode, errorMessage)
	 */
	virtual void SetOnErrorCallback(std::function<void(int, const std::string&)> callBack) = 0;
	
	/**
	 * @brief 设置服务器配置
	 * @param maxConnections 最大连接数，默认1000
	 * @param keepAlive 是否启用心跳，默认true
	 * @param timeout 连接超时时间(秒)，默认60
	 */
	virtual void SetServerConfig(int maxConnection = 1000, bool keepAlive = true, int timeOut = 60) = 0;
};
