#pragma once
#include "CSingleton.h"
#include <string>
#include <vector>
#include <map>

/**
*  @class       CommFun
*  @brief       通用助手函数类 (去 Qt 依赖，使用 std::filesystem)
*/
class CommFun : public CSingleton<CommFun>
{
	friend class CSingleton<CommFun>;
public:
	// 拷贝单个文件
	bool FileCopy(const std::string& sourceFilePath, const std::string& targetFilePath);

	// 递归拷贝文件夹
	bool FolderCopy(const std::string& sourceDirPath, const std::string& targetDirPath);

	// 获取文件夹下所有的文件路径（递归）
	void GetFolderFile(const std::string& folderPath, std::vector<std::string>& vFile);

	// 获取当前时间戳字符串 YYYY_MM_DD_HH_MM_SS
	std::string GetCurrentTimeStr();

	// 打印日志
	void printLog(const std::string& message);

	/**
	*  @brief       将 Qt QMap 转换为 std::map 的模板助手函数
	*/
	template<typename QtMap, typename StdMap>
	StdMap ConvertQMap2StdMap(const QtMap& qtMap)
	{
		StdMap stdMap;
		for (auto it = qtMap.begin(); it != qtMap.end(); ++it)
		{
			stdMap[it.key().toStdString()] = it.value().toStdString();
		}
		return stdMap;
	}

private:
	CommFun() = default;
	~CommFun() = default;
};
