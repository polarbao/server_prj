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
		// 正则表达式：匹配倒数第二个反斜杠后的所有内容
		// (.*\\){2} 匹配前面至少两个反斜杠的部分
		// ([^\\]+)\\([^\\]+)$ 捕获倒数两级路径
		std::regex pattern(R"(.*?/([^/]+)/([^/]+)$)");
		std::string replacement = "$1/$2";
		return std::regex_replace(original, pattern, replacement);
	};
	auto objName = convertPath(objPath);

	//Upload不同方式
	std::shared_ptr<std::iostream> content = std::make_shared<std::fstream>(objPath, std::ios::in | std::ios::binary);
	PutObjectRequest request(m_ossParam.ossBucket, objName, content);
	auto outcome = m_pClient->PutObject(request);	
	auto urlData = m_pClient->GeneratePresignedUrl(m_ossParam.ossBucket, objName);
	//Noet: 截取签名参数
	auto GetUrlView = [](const std::string& url) -> std::string
	{
		size_t pos = url.find('?');
		return (pos != std::string::npos) ? std::string(std::string_view(url).substr(0, pos)): url;
	};
	// 返回全部的URL数据
	//url = UrlDecode(GetUrlView(urlData.result()));

	// 返回截取后，保留倒数2个/的数据
	auto tmpURL = UrlDecode(GetUrlView(urlData.result()));
	url = convertPath(tmpURL);
	if (!outcome.isSuccess()) 
	{
		//输出日常日志至，Log 显示框
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

	// TODO：是否对URL数据进行解码操作（处理Object名称中的特殊字符）
	// TODO：拆分serID, 创建文件中存储
	// 查找 '/' 的位置, 拆分路径数据
	size_t slashPos = object.find('/');
	// 拆分获取第一部分（文件夹名称）
	std::string folderName = dstDownPath + object.substr(0, slashPos) +"\\";
	std::string fileName = object.substr(slashPos+1);
	std::cout << "提取的文件夹名称: " << folderName << std::endl;
	LOG_INFO(QString("oss_module download_single_file_fun, create_folder_name = %1").arg(folderName.data()));

	// 检查文件夹是否已存在
	if (std::filesystem::exists(folderName))
	{
		std::cout << "文件夹已存在: " << folderName << std::endl;
		LOG_INFO("oss_module download_single_file_fun, folder_exist");
	}
	else
	{
		// 创建文件夹
		if (std::filesystem::create_directory(folderName))
		{
			std::cout << "文件夹创建成功: " << folderName << std::endl;
			LOG_INFO("oss_module download_single_file_fun, create_folder_success");
		}
		else
		{
			std::cerr << "文件夹创建失败: " << folderName << std::endl;
			LOG_ERROR("oss_module download_single_file_fun, create_folder_failed");
			return ret;
		}
	}

	std::string downPath = folderName + fileName;
	DownloadObjectRequest request(m_ossParam.ossBucket, object, downPath);
	auto outcome = m_pClient->ResumableDownloadObject(request);

	if (!outcome.isSuccess())
	{
		//输出日常日志至，Log 显示框
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
