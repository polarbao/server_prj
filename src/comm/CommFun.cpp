#include "CommFun.h"
#include "CLogManager.h"
#include <filesystem>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <iostream>

namespace fs = std::filesystem;

bool CommFun::FileCopy(const std::string& sourceFilePath, const std::string& targetFilePath)
{
	try
	{
		fs::path srcPath(sourceFilePath);
		fs::path dstPath(targetFilePath);

		if (fs::exists(dstPath))
		{
			fs::remove(dstPath);
		}

		fs::copy_file(srcPath, dstPath, fs::copy_options::overwrite_existing);
		return true;
	}
	catch (const fs::filesystem_error& e)
	{
		printLog("FileCopy failed: " + std::string(e.what()));
		return false;
	}
}

bool CommFun::FolderCopy(const std::string& sourceDirPath, const std::string& targetDirPath)
{
	try
	{
		fs::path srcDir(sourceDirPath);
		fs::path dstDir(targetDirPath);

		if (!fs::exists(srcDir))
		{
			printLog("Source directory does not exist: " + sourceDirPath);
			return false;
		}

		fs::copy(srcDir, dstDir, fs::copy_options::recursive | fs::copy_options::overwrite_existing);
		return true;
	}
	catch (const fs::filesystem_error& e)
	{
		printLog("FolderCopy failed: " + std::string(e.what()));
		return false;
	}
}

void CommFun::GetFolderFile(const std::string& folderPath, std::vector<std::string>& vFile)
{
	try
	{
		fs::path dirPath(folderPath);
		if (!fs::exists(dirPath))
		{
			return;
		}

		for (const auto& entry : fs::recursive_directory_iterator(dirPath))
		{
			if (entry.is_regular_file())
			{
				vFile.push_back(entry.path().string());
			}
		}
	}
	catch (const fs::filesystem_error& e)
	{
		printLog("GetFolderFile failed: " + std::string(e.what()));
	}
}

std::string CommFun::GetCurrentTimeStr()
{
	auto now = std::chrono::system_clock::now();
	std::time_t nowTimeT = std::chrono::system_clock::to_time_t(now);
	std::tm localTime;
	
#if defined(_WIN32)
	localtime_s(&localTime, &nowTimeT);
#else
	localtime_r(&nowTimeT, &localTime);
#endif

	std::stringstream ss;
	ss << std::put_time(&localTime, "%Y-%m-%d %H:%M:%S");
	std::string timeStr = ss.str();

	std::string sanitized;
	sanitized.reserve(timeStr.size());

	std::transform(timeStr.begin(), timeStr.end(), std::back_inserter(sanitized), [](char c)
	{
		return (isdigit(static_cast<unsigned char>(c))) ? c : '_';
	});
	return sanitized;
}

void CommFun::printLog(const std::string& message)
{
	LOG_INFO(QString::fromStdString(message));
	std::cout << message << std::endl;
}
