#pragma once

#include "ipc/HttpRepParser.h"
#include "CSingleton.h"

#include <vector>
#include <list>
#include <alibabacloud/oss/OssClient.h>

using namespace AlibabaCloud::OSS;

class SingleOSSToken : public CSingleton<SingleOSSToken>
{
	friend CSingleton<SingleOSSToken>;
public:


	/** 
	*  @brief       设置OSS连接 
	*  @param[in]    
	*  @param[out]   
	*  @return                    
	*/
	void InitConnect();

	///**
	//*  @brief       获取OSS Token数据
	//*  @param[in]
	//*  @param[out]
	//*  @return
	//*/
	//void GetOSSToken();

	/**
	*  @brief       设置OSS参数
	*  @param[in]
	*  @param[out]
	*  @return
	*/
	void SetOSSParam(OSSTokenParam& ossParam);

	/**
	*  @brief       上传文件（多文件
	*  @param[in]	fileList	上传文件本地地址数组
	*  @param[out]	retInfo		成功/错误信息
	*  @param[out]	vURL		返回URL列表
	*  @return		bool		是否操作成功
	*/
	bool UploadMulti(std::vector<std::string>& fileList, std::string& retInfo, std::vector<std::string>& vURL);

	/**
	*  @brief       下载文件（多文件
	*  @param[in]	objVec		下载文件URL数组
	*  @param[out]	retInfo		成功/错误信息
	*  @return		bool		是否操作成功
	*/
	bool DownloadMulti(std::vector<std::string>& objVec, std::string& retInfo, const std::string& downDstPath);

	/**
	*  @brief       返回OSS中存储的所有数据
	*  @param[in]
	*  @param[out]
	*  @return		std::list<std::string> bucketName中所有存储数据
	*/
	std::list<std::string> GetAllFileList();

	/**
	*  @brief       获取OSS客户端
	*  @param[in]
	*  @param[out]
	*  @return		std::shared_ptr<OssClient>
	*/
	inline std::shared_ptr<OssClient> GetClienData() const { return m_pClient; }

	/**
	*  @brief       获取OSS客户端配置文件
	*  @param[in]
	*  @param[out]
	*  @return		ClientConfiguration
	*/
	inline ClientConfiguration GetCfgData() const { return m_conf; }
	   
//private:
	/**
	*  @brief       判断客户端是否初始化设置
	*  @param[in]
	*  @param[out]
	*  @return
	*/
	bool JudgeClientIsEmpty();

	/**
	*  @brief       上传文件（单文件
	*  @param[in]	objPath		上传文件地址
	*  @param[out]	url			上传文件对应的OSS URL数据
	*  @return
	*/
	bool UploadSingleFile(std::string objPath, std::string& url);

	/**
	*  @brief       下载文件（单文件
	*  @param[in]	objPath		下载URL地址
	*  @param[out]	retInfo		成功/失败信息
	*  @param[out]	dstDownPath	todo:传入本地保存文件夹地址
	*  @return
	*/
	bool DownloadSingleFile(std::string objURL, std::string& retInfo, std::string dstDownPath = "D:\\OSSToken_Test\\Down\\");


private:
	OSSTokenParam m_ossParam;						// oss参数
	ClientConfiguration m_conf;						// oss客户端配置信息
	std::shared_ptr<OssClient> m_pClient;			// oss客户端
};

