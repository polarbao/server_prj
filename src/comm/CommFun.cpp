
#include <sstream>

#include "CommFun.h"
#include "CLogManager.h"


bool CommFun::FileCopy(const QString& sourceFilePath, const QString& targetFilePath)
{

	// 如果目标文件已存在，先删除
	if (QFile::exists(targetFilePath)) 
	{
		if (!QFile::remove(targetFilePath)) 
		{
			printLog("无法删除已存在的目标文件:" + targetFilePath);
			return false;
		}
	}

	// 复制文件
	if (!QFile::copy(sourceFilePath, targetFilePath)) 
	{
		printLog("文件复制失败:"+ sourceFilePath + "->" + targetFilePath);
		return false;
	}
	return true;
}

bool CommFun::FolderCopy(const QString& sourceDirPath, const QString& targetDirPath)
{
	/*
	1. 判断当前文件夹是否存在
	2. 复制src文件夹数据至dst文件夹中
	*/
	QDir sourceDir(sourceDirPath);
	QDir targetDir(targetDirPath);

	// 检查源文件夹是否存在
	if (!sourceDir.exists()) 
	{
		qDebug() << "源文件夹不存在:" << sourceDirPath;
		return false;
	}

	// 如果目标文件夹不存在，则创建
	if (!targetDir.exists()) {
		if (!targetDir.mkpath(".")) 
		{  // 创建多级目录
			qDebug() << "无法创建目标文件夹:" << targetDirPath;
			return false;
		}
		qDebug() << "已创建目标文件夹:" << targetDirPath;
	}

	// 获取源文件夹中所有条目（文件和子文件夹）
	QFileInfoList entries = sourceDir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);

	foreach(const QFileInfo& entry, entries)
	{
		QString sourcePath = entry.filePath();
		QString targetPath = targetDir.filePath(entry.fileName());

		if (entry.isDir())
		{
			// 递归复制子文件夹
			if (!FolderCopy(sourcePath, targetPath))
			{
				return false;
			}
		}
		else
		{
			// 复制文件
			if (!FileCopy(sourcePath, targetPath))
			{
				return false;
			}
		}
	}
	return true;
}

void CommFun::GetFolderFile(const QString& folderPath, std::vector<std::string>& vFile)
{
	QDir dir(folderPath);
	if (!dir.exists())
	{
		//printLog(QString::fromLocal8Bit("文件夹不存在:%1").arg(folderPath));
		return;
	}

	// 获取文件夹中所有条目（文件和子文件夹），排除.和..
	QFileInfoList entries = dir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);

	foreach(const QFileInfo& entry, entries)
	{
		if (entry.isDir())
		{
			// 递归处理子文件夹
			GetFolderFile(entry.filePath(), vFile);
			//CommFun::GetInstance().GetFolderFile(entry.filePath().toStdString(), vFile);
		}
		else
		{
			// Qt自动处理路径转义，转换为std::string后存入数组
			std::string filePath = entry.filePath().toStdString();
			vFile.push_back(filePath);
		}
	}
}


std::string CommFun::GetCurrentTimeStr()
{
	//  生成原始时间字符串（格式：YYYY-MM-DD HH:MM:SS）
	auto now = std::chrono::system_clock::now();
	std::time_t nowTimeT = std::chrono::system_clock::to_time_t(now);
	std::tm localTime;
	localtime_s(&localTime, &nowTimeT); // Windows线程安全

	std::stringstream ss;
	ss << std::put_time(&localTime, "%Y-%m-%d %H:%M:%S");
	std::string timeStr = ss.str();

	//  使用lambda表达式替换所有非数字字符为_
	std::string sanitized;
	sanitized.reserve(timeStr.size()); // 预分配空间，提高效率

	// 遍历每个字符，通过lambda判断：数字则保留，否则替换为_
	std::transform(timeStr.begin(), timeStr.end(), std::back_inserter(sanitized), [](char c)
	{
		// isdigit要求参数为unsigned char（避免负数），转换后判断是否为数字
		return (isdigit(static_cast<unsigned char>(c))) ? c : '_';
	});
	return sanitized;
}

void CommFun::printLog(const QString& message)
{
	LOG_INFO(message);
	std::cout << message.toLocal8Bit().toStdString().c_str() << std::endl;
}

