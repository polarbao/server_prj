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
	*  @brief       ����OSS���� 
	*  @param[in]    
	*  @param[out]   
	*  @return                    
	*/
	void InitConnect();

	///**
	//*  @brief       ��ȡOSS Token����
	//*  @param[in]
	//*  @param[out]
	//*  @return
	//*/
	//void GetOSSToken();

	/**
	*  @brief       ����OSS����
	*  @param[in]
	*  @param[out]
	*  @return
	*/
	void SetOSSParam(OSSTokenParam& ossParam);

	/**
	*  @brief       �ϴ��ļ������ļ�
	*  @param[in]	fileList	�ϴ��ļ����ص�ַ����
	*  @param[out]	retInfo		�ɹ�/������Ϣ
	*  @param[out]	vURL		����URL�б�
	*  @return		bool		�Ƿ�����ɹ�
	*/
	bool UploadMulti(std::vector<std::string>& fileList, std::string& retInfo, std::vector<std::string>& vURL);

	/**
	*  @brief       �����ļ������ļ�
	*  @param[in]	objVec		�����ļ�URL����
	*  @param[out]	retInfo		�ɹ�/������Ϣ
	*  @return		bool		�Ƿ�����ɹ�
	*/
	bool DownloadMulti(std::vector<std::string>& objVec, std::string& retInfo, const std::string& downDstPath);

	/**
	*  @brief       ����OSS�д洢����������
	*  @param[in]
	*  @param[out]
	*  @return		std::list<std::string> bucketName�����д洢����
	*/
	std::list<std::string> GetAllFileList();

	/**
	*  @brief       ��ȡOSS�ͻ���
	*  @param[in]
	*  @param[out]
	*  @return		std::shared_ptr<OssClient>
	*/
	inline std::shared_ptr<OssClient> GetClienData() const { return m_pClient; }

	/**
	*  @brief       ��ȡOSS�ͻ��������ļ�
	*  @param[in]
	*  @param[out]
	*  @return		ClientConfiguration
	*/
	inline ClientConfiguration GetCfgData() const { return m_conf; }
	   
//private:
	/**
	*  @brief       �жϿͻ����Ƿ��ʼ������
	*  @param[in]
	*  @param[out]
	*  @return
	*/
	bool JudgeClientIsEmpty();

	/**
	*  @brief       �ϴ��ļ������ļ�
	*  @param[in]	objPath		�ϴ��ļ���ַ
	*  @param[out]	url			�ϴ��ļ���Ӧ��OSS URL����
	*  @return
	*/
	bool UploadSingleFile(std::string objPath, std::string& url);

	/**
	*  @brief       �����ļ������ļ�
	*  @param[in]	objPath		����URL��ַ
	*  @param[out]	retInfo		�ɹ�/ʧ����Ϣ
	*  @param[out]	dstDownPath	todo:���뱾�ر����ļ��е�ַ
	*  @return
	*/
	bool DownloadSingleFile(std::string objURL, std::string& retInfo, std::string dstDownPath = "D:\\OSSToken_Test\\Down\\");


private:
	OSSTokenParam m_ossParam;						// oss����
	ClientConfiguration m_conf;						// oss�ͻ���������Ϣ
	std::shared_ptr<OssClient> m_pClient;			// oss�ͻ���
};

