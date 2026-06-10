#include "SingleOSSToken.h"
#include "MessageDefine.h"
#include "CLogManager.h"

#include <regex>
#include <string>
#include <string_view>
#include <functional>
#include <filesystem>
#include <fstream>

void SingleOSSToken::SetOSSParam(OSSTokenParam& ossParam)
{
	if (ossParam.IsEmpty())
	{
		std::cout << "param is empty" << std::endl;
		LOG_INFO(QString("oss_module SetOSSParam_fun, cur_inParam_is_empty"));
		return;
	}
	m_ossParam = ossParam;
	LOG_INFO(QString("oss_module SetOSSParam_fun, set_oss_param_success"));
}

void SingleOSSToken::InitConnect()
{
	if (m_ossParam.IsEmpty())
	{
		std::cout << "param is empty" << std::endl;
		return;
	}

	InitializeSdk();
	m_conf.signatureVersion = SignatureVersionType::V4;
	auto credentialsProvider = std::make_shared<SimpleCredentialsProvider>(m_ossParam.ossId, m_ossParam.ossSerct, m_ossParam.ossToken);
	m_pClient = std::make_shared<OssClient>(m_ossParam.ossEndPoint, credentialsProvider, m_conf);
	m_pClient->SetRegion(m_ossParam.ossRegin);
}


//void SingleOSSToken::GetOSSToken()
//{
//
//}

bool SingleOSSToken::UploadMulti(std::vector<std::string>& fileList, std::string& retInfo, std::vector<std::string>& vURL)
{
	bool ret = true;
	for (auto& it : fileList)
	{
		std::string url;
		ret = UploadSingleFile(it, url);
		if (!ret)
		{
			std::cout << "single ret = false" << std::endl;
			ret = false;
			LOG_ERROR(QString("oss_module UploadMulti_fun, upload_failed cur_file_name = %1").arg(it.data()));
			break;
		}
		vURL.push_back(url);
		LOG_INFO(QString("oss_module UploadMulti_fun, return_url_data = %1").arg(url.data()));
	}
	return ret;
}

bool SingleOSSToken::UploadSingleFile(std::string objPath, std::string& url)
{
	bool ret = false;
	if (objPath.empty())
	{
		std::cout << "obj path is empty" << std::endl;
		LOG_INFO(QString("oss_module UploadSingleFile_fun upload_failed_cur_file_objPath_is_empty"));
		return ret;
	}
	auto  convertPath= [&](const std::string& original) ->std::string
	{
		// ������ʽ��ƥ�䵹���ڶ�����б�ܺ����������
		// (.*\\){2} ƥ��ǰ������������б�ܵĲ���
		// ([^\\]+)\\([^\\]+)$ ����������·��
		std::regex pattern(R"(.*?/([^/]+)/([^/]+)$)");
		std::string replacement = "$1/$2";
		return std::regex_replace(original, pattern, replacement);
	};
	auto objName = convertPath(objPath);

	//Upload��ͬ��ʽ
	std::shared_ptr<std::iostream> content = std::make_shared<std::fstream>(objPath, std::ios::in | std::ios::binary);
	PutObjectRequest request(m_ossParam.ossBucket, objName, content);
	auto outcome = m_pClient->PutObject(request);	
	auto urlData = m_pClient->GeneratePresignedUrl(m_ossParam.ossBucket, objName);
	//Noet: ��ȡǩ������
	auto GetUrlView = [](const std::string& url) -> std::string
	{
		size_t pos = url.find('?');
		return (pos != std::string::npos) ? std::string(std::string_view(url).substr(0, pos)): url;
	};
	// ����ȫ����URL����
	//url = UrlDecode(GetUrlView(urlData.result()));

	// ���ؽ�ȡ�󣬱������2��/������
	auto tmpURL = UrlDecode(GetUrlView(urlData.result()));
	url = convertPath(tmpURL);
	if (!outcome.isSuccess()) 
	{
		//����ճ���־����Log ��ʾ��
		LOG_DEBUG(QString("oss_module upload_single_file_failed, cur_file_name = %1, cur_file_path = %2")
			.arg(objName.data()).arg(objPath.data()));
		LOG_DEBUG(QString("oss_module upload_single_file_failed, err_code = %1, err_msg = %2, err_reqId = %3")
			.arg(outcome.error().Code().data())
			.arg(outcome.error().Message().data())
			.arg(outcome.error().RequestId().data()));

		std::cout << "PutObject fail" <<
			",code:" << outcome.error().Code() <<
			",message:" << outcome.error().Message() <<
			",requestId:" << outcome.error().RequestId() << std::endl;
		ret = false;
	}
	else
	{
		ret = true;
		LOG_DEBUG(QString("oss_module upload_single_file_success, cur_file_name = %1, cur_file_path = %2")
			.arg(objName.data()).arg(objPath.data()));
	}
	return ret;
}

bool SingleOSSToken::DownloadSingleFile(std::string objURL, std::string& retInfo, std::string dstDownPath /*=""*/)
{
	bool ret = false;
	if (objURL.empty())
	{
		std::cout << "obj url is empty" << std::endl;
		LOG_INFO(QString("oss_module DownloadSingleFile_fun, download_failed_cur_obj_url_is_empty"));
		return ret;
	}

	std::regex pattern(R"(^https?://([^.]+)\.(.*?)/(.+)$)");
	std::smatch match;
	if (!std::regex_match(objURL, match, pattern) || match.size() != 4)
	{
		return false;
	}
	auto bucket = match[1].str();
	auto endpoint = match[2].str();
	auto object = match[3].str();

	// TODO���Ƿ��URL���ݽ��н������������Object�����е������ַ���
	// TODO�����serID, �����ļ��д洢
	// ���� '/' ��λ��, ���·������
	size_t slashPos = object.find('/');
	// ��ֻ�ȡ��һ���֣��ļ������ƣ�
	std::string folderName = dstDownPath + object.substr(0, slashPos) +"\\";
	std::string fileName = object.substr(slashPos+1);
	std::cout << "��ȡ���ļ�������: " << folderName << std::endl;
	LOG_INFO(QString("oss_module download_single_file_fun, create_folder_name = %1").arg(folderName.data()));

	// ����ļ����Ƿ��Ѵ���
	if (std::filesystem::exists(folderName))
	{
		std::cout << "�ļ����Ѵ���: " << folderName << std::endl;
		LOG_INFO("oss_module download_single_file_fun, folder_exist");
	}
	else
	{
		// �����ļ���
		if (std::filesystem::create_directory(folderName))
		{
			std::cout << "�ļ��д����ɹ�: " << folderName << std::endl;
			LOG_INFO("oss_module download_single_file_fun, create_folder_success");
		}
		else
		{
			std::cerr << "�ļ��д���ʧ��: " << folderName << std::endl;
			LOG_ERROR("oss_module download_single_file_fun, create_folder_failed");
			return ret;
		}
	}

	std::string downPath = folderName + fileName;
	DownloadObjectRequest request(m_ossParam.ossBucket, object, downPath);
	auto outcome = m_pClient->ResumableDownloadObject(request);

	if (!outcome.isSuccess())
	{
		//����ճ���־����Log ��ʾ��
		LOG_ERROR(QString("oss_module DownloadSingleFile_fun, download_single_file_failed, cur_file_name = %1, down_path = %2")
			.arg(object.data()).arg(downPath.data()));
		LOG_ERROR(QString("oss_module DownloadSingleFile_fun, download_single_file_failed, err_code = %1, err_msg = %2, err_reqId = %3")
			.arg(outcome.error().Code().data())
			.arg(outcome.error().Message().data())
			.arg(outcome.error().RequestId().data()));

		std::cout << "GetObjectToFile fail" <<
			",code:" << outcome.error().Code() <<
			",message:" << outcome.error().Message() <<
			",requestId:" << outcome.error().RequestId() << std::endl;
		ret = false;
		retInfo = outcome.error().Message();
	}
	else
	{
		ret = true;
		retInfo = "download_faile_2_oss_success";
		std::cout << "GetObjectToFile success" << outcome.result().Metadata().ContentLength() << std::endl;
		LOG_INFO(QString("oss_module DownloadSingleFile_fun, download_single_file_success, cur_file_down_path = %1, file_size = %2")
			.arg(object.data()).arg(outcome.result().Metadata().ContentLength()));
	}
	return ret;
}

bool SingleOSSToken::DownloadMulti(std::vector<std::string>& objVec, std::string& retInfo, const std::string& downDstPath)
{
	bool ret = false;
	for (auto& it : objVec)
	{
		ret = DownloadSingleFile(it, retInfo, downDstPath);
		if (!ret)
		{
			std::cout << "download file cur file name = " << it << std::endl;
			LOG_INFO(QString("oss_module DownloadMulti_fun, download_file_failed, file_url = %1").arg(it.data()));
		}
	}
	return ret;
}

std::list<std::string> SingleOSSToken::GetAllFileList()
{
	std::list<std::string> keyList;
	ListObjectsRequest req(m_ossParam.ossBucket);
	auto outcome = m_pClient->ListObjects(req);
	auto list = outcome.result().ObjectSummarys();
	for (auto& it : list)
	{
		keyList.push_back(it.Key());
		LOG_INFO(QString("oss_module GetAllFileList_fun, cur_file_objName = %1").arg(it.Key().data()));
	}
	return keyList;
}


bool SingleOSSToken::JudgeClientIsEmpty()
{
	bool ret = true;
	if (m_pClient && !m_ossParam.IsEmpty())
	{
		ret = false;
	}
	return ret;
}
