#pragma once


#include "CSingleton.h"
#include "global.h"
#include <filesystem>


/**
*  @author      lrz
*  @class       CTheadManger
*  @brief       线程管理类
*/

namespace fs = std::filesystem;
class CommFun : public QObject, public CSingleton<CommFun>
{
	Q_OBJECT
	friend class CSingleton< CommFun>;
public:

	// 复制单个文件
	bool FileCopy(const QString& srcFolder, const QString& dstFolder);

	// 递归复制文件夹
	bool FolderCopy(const QString& srcFolder, const QString& dstFolder);

	// 获取文件夹中所有文件的路径（包括子文件夹）
	void GetFolderFile(const QString& folderPath, std::vector<std::string>& vFile);

	// 获取当前时间戳字符串（- : 空格 替换为_）
	std::string GetCurrentTimeStr();

	// 辅助函数：打印日志
	void printLog(const QString& message);
	

	/**
	*  @brief       数据类型转换函数
	*  @param[in]    QtMap数据类型
	*  @param[out]   std::map数据类型
	*  @return
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


};