#pragma once

#ifndef LOGGER_H
#define LOGGER_H

#include <map>
#include <chrono>
#include <string>
#include <fstream>
#include <windows.h>
#include <filesystem>

// debug
// #define LOG_FORCE_UNICODE

// 不知道有没有用的兼容工具宏
#if defined(LOG_FORCE_UNICODE) || defined(UNICODE)

#define LOG_UNICODE

#define LOG_IOS			std::wios
#define LOG_FTIME		::wcsftime
#define LOG_TOSTRING	std::to_wstring
#define LOG_SNPRINTF 	::_snwprintf_s
#define LOG_CHAR_TYPE	wchar_t
#define LOG_STRING_TYPE std::wstring
#define LOG_STREAM_TYPE std::wofstream
#define LOG_STRINGSTREAM_TYPE std::wstringstream

#define _LOG_STRING_(s) L ## s
#define LOG_STRING(s) _LOG_STRING_(s)

#else

#define LOG_IOS			std::ios
#define LOG_FTIME		::strftime
#define LOG_CHAR_TYPE	char
#define LOG_TOSTRING	std::to_string
#define LOG_SNPRINTF 	::_snprintf_s
#define LOG_STRING_TYPE std::string
#define LOG_STREAM_TYPE std::ofstream
#define LOG_STRINGSTREAM_TYPE std::stringstream

#define LOG_STRING(s) s

#endif

#define LOG_PREFIX(s) LOG_STRING("[") + s + LOG_STRING("] ")
#define LOG_BUFFER_COUNT(buf) (sizeof(buf) / sizeof((buf)[0]))

namespace Logger
{

	enum class ELevel : std::uint8_t
	{
		LOG_DEBUG,
		LOG_INFO,
		LOG_ERROR,
		LOG_FATAL,
		LOG_WARNING
	};

	const static std::map<ELevel, LOG_STRING_TYPE> mapLevelToString = {
			{ELevel::LOG_INFO,    LOG_STRING("INFO")},
			{ELevel::LOG_DEBUG,   LOG_STRING("DEBUG")},
			{ELevel::LOG_ERROR,   LOG_STRING("ERROR")},
			{ELevel::LOG_FATAL,   LOG_STRING("FATAL")},
			{ELevel::LOG_WARNING, LOG_STRING("WARNING")},
	};

	//===========================================================

	// Console Colors
	// https://learn.microsoft.com/en-us/windows/console/console-screen-buffers
	enum ELogColor : std::uint16_t
	{
		LOG_COLOR_DEFAULT	= FOREGROUND_RED	| FOREGROUND_GREEN	| FOREGROUND_BLUE,
		LOG_COLOR_TIMESTAMP = FOREGROUND_BLUE	| FOREGROUND_GREEN,
		LOG_COLOR_BRACKET	= FOREGROUND_RED	| FOREGROUND_GREEN	| FOREGROUND_BLUE,
		LOG_COLOR_FILELINE	= FOREGROUND_GREEN	| FOREGROUND_BLUE	| FOREGROUND_INTENSITY,
		LOG_COLOR_COUNT		= FOREGROUND_RED	| FOREGROUND_GREEN	| FOREGROUND_BLUE | FOREGROUND_INTENSITY,
	};

	const static std::map<ELevel, std::uint16_t> mapLevelColors =
	{
		{ELevel::LOG_INFO,    FOREGROUND_GREEN	| FOREGROUND_INTENSITY},
		{ELevel::LOG_DEBUG,   FOREGROUND_GREEN	| FOREGROUND_BLUE},
		{ELevel::LOG_ERROR,   FOREGROUND_RED	| FOREGROUND_INTENSITY},
		{ELevel::LOG_FATAL,   FOREGROUND_RED	| FOREGROUND_BLUE	| FOREGROUND_INTENSITY},
		{ELevel::LOG_WARNING, FOREGROUND_RED	| FOREGROUND_GREEN	| FOREGROUND_INTENSITY},
	};

	//===========================================================

	struct Config_t 
	{
		// Config
		bool			bEnableFile = true;
		bool			bEnableColor = true;
		bool			bEnableConsole = true;

		ELevel			eMinLevel = ELevel::LOG_DEBUG;
		LOG_STRING_TYPE szFilePath = LOG_STRING("logs/app.log");

		// flags
		// @todo: 到时候考虑用字符串格式化来优化它，可能会改成这样:
		//	LOG_STRING_TYPE szPrefixFlags = LOG_STRING("{TIMESTAMP}[%Y-%m-%d %H:%M:%S] {LEVEL}[%L] {FILELINE}[%F:%l]");
		bool			bShowFile = true;
		bool			bShowLine = true;
		bool			bShowLevel = true;
		bool			bShowTimestamp = true;
	};

	// 记录重复日志用
	struct LogEntry_t 
	{
		std::uintptr_t	nCount;
		LOG_STRING_TYPE	szContent;
		COORD			vecPosition;
	};

	// 记录日志文件重复用
	struct FileEntry_t 
	{
		std::uintptr_t	nCount;
		std::streampos	posLast;
		LOG_STRING_TYPE	szContent;
	};

	struct Prefix_t
	{
		ELevel eLevel;
		LOG_STRING_TYPE szFile;
		LOG_STRING_TYPE szLine;
		LOG_STRING_TYPE szFileLine;
		LOG_STRING_TYPE szTimeStamp;
	};

	//===========================================================

	// 全局变量
	static HANDLE           hConsole		= INVALID_HANDLE_VALUE;
	static Config_t         stConfig;
	static LOG_STREAM_TYPE  fsLogFile;
	static LOG_STRING_TYPE  szLastMsg;
	static LogEntry_t       stLastEntry;
	static std::uint64_t    nRepeatCount	= 0;
	static FileEntry_t      stLastFileEntry;

	//===========================================================

	// 初始化控制台
	inline bool InitConsole()
	{
		// 如果已经初始化过，直接返回
		if (hConsole != INVALID_HANDLE_VALUE)
			return true;

		// 分配控制台
		if (::AllocConsole() != TRUE)
			return false;

		// 先尝试获取标准句柄
		hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
		if (hConsole != INVALID_HANDLE_VALUE && hConsole != NULL)
			return true;

		// 如果获取失败，再尝试创建新句柄
		hConsole = ::CreateFile(
			LOG_STRING("CONOUT$"),
			GENERIC_WRITE,
			FILE_SHARE_READ | FILE_SHARE_WRITE,
			nullptr,
			OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL,
			nullptr);

		// 然后再设置句柄
		if (hConsole != INVALID_HANDLE_VALUE)
			::SetStdHandle(STD_OUTPUT_HANDLE, hConsole);

		// 返回是否成功
		return hConsole != INVALID_HANDLE_VALUE;
	}

	inline void DetachConsole()
	{
		// 如果分配了控制台，关闭句柄并标记为空句柄
		if (hConsole != INVALID_HANDLE_VALUE)
		{
			::CloseHandle(hConsole);
			hConsole = INVALID_HANDLE_VALUE;
		}

		// 尝试释放控制台
		if (::FreeConsole() != TRUE)
			return;

		// 如果控制台窗口存在，关闭控制台
		HWND hConsoleWindow = ::GetConsoleWindow();
		if (hConsoleWindow != nullptr)
			::PostMessageW(hConsoleWindow, WM_CLOSE, 0U, 0L);
	}

	//===========================================================

	namespace utils
	{
		/**
		* @brief 获取当前时间戳
		*
		* @return 本机时间戳.
		*/
		inline tm GetTimeStamp()
		{
			auto tTime = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

			tm tmLocal;
			::localtime_s(&tmLocal, &tTime);
			return tmLocal;
		}

		/**
		* @brief 获取当前时间戳字符串
		*
		* @param bIsFile 是否输出为文件名格式
		* @return 时间格式化字符串.
		*/
		inline LOG_STRING_TYPE GetTimeStampString(bool bForFile)
		{
			tm tmLocal = GetTimeStamp();
			constexpr std::uint64_t nBufferSize = 32;
			LOG_CHAR_TYPE szBuffer[nBufferSize];
			LOG_FTIME(szBuffer, nBufferSize, bForFile ? LOG_STRING("%Y%m%d_%H%M%S") : LOG_STRING("%Y-%m-%d %H:%M:%S"), &tmLocal);
			return szBuffer;
		}

		/**
		* @brief 获取简短文件名
		*
		* @param wszFilePath 完整绝对路径
		* @return 返回文件名
		*/
		inline LOG_STRING_TYPE GetShortFileName(const LOG_CHAR_TYPE* szFilePath)
		{

			return std::filesystem::path(szFilePath).filename()
#ifdef LOG_UNICODE
				.wstring();
#else
				.string();
#endif
		}

		/**
		* @brief 写入控制台
		*
		* @param szText 输出文本
		* @param wColor 输出颜色
		* @return 返回文件名
		*/
		inline void WriteColorText(const LOG_STRING_TYPE& szText, WORD wColor = ELogColor::LOG_COLOR_DEFAULT)
		{
			if (stConfig.bEnableColor)
				::SetConsoleTextAttribute(hConsole, wColor);

			::WriteConsole(hConsole, szText.c_str(), 
				static_cast<DWORD>(szText.length()), 
				nullptr, nullptr);
		}

		/**
		* @brief 宽字符转UTF-8字符串
		*
		* @param wszText 需要转的字符串
		* @param nCode 源文本编码号
		* @return 返回文件名
		*/
		std::string WideToUTF8(const std::wstring& wszText, UINT nCode = CP_UTF8)
		{
			if (wszText.empty()) return std::string();

			int size_needed = WideCharToMultiByte(nCode, 0, wszText.data(), (int)wszText.size(), nullptr, 0, nullptr, nullptr);
			std::string utf8str(size_needed, 0);
			WideCharToMultiByte(nCode, 0, wszText.data(), (int)wszText.size(), &utf8str[0], size_needed, nullptr, nullptr);
			return utf8str;
		}

		/**
		* @brief UTF-8转宽字符
		*
		* @param szText 需要转的字符串
		* @param nCode 源文本编码号
		* @return 返回文件名
		*/
		std::wstring UTF8ToWide(const std::string& szText, UINT nCode = CP_UTF8)
		{
			if (szText.empty()) return std::wstring();

			int size_needed = MultiByteToWideChar(nCode, 0, szText.data(), (int)szText.size(), nullptr, 0);
			std::wstring wstr(size_needed, 0);
			MultiByteToWideChar(nCode, 0, szText.data(), (int)szText.size(), &wstr[0], size_needed);
			return wstr;
		}

		/**
		* @brief 字符串转UTF-8
		*
		* @param szText 需要转的字符串
		* @param nCode 源文本编码号
		* @return 返回文件名
		*/
		std::string StrToUTF8(const std::string& szText, UINT nCode = 936) 
		{
			std::wstring wszWideStr = UTF8ToWide(szText, 936);
			std::string szUtf8Str = WideToUTF8(wszWideStr);

			// 移除结尾的null字符
			if (!szUtf8Str.empty() && szUtf8Str.back() == '\0') szUtf8Str.pop_back();

			return szUtf8Str;
		}
	}

	//===========================================================

	namespace internal
	{
		inline void WhiteToConsole(const Prefix_t tPrefix, const LOG_STRING_TYPE& szContent)
		{
			if (!stConfig.bEnableConsole) return; // 先判断是否要开控制台
			if (!InitConsole()) return; // 再确认控制台是否初始化成功

			auto Format = [&](const LOG_STRING_TYPE& str, WORD wColor = ELogColor::LOG_COLOR_DEFAULT) -> void
			{
				WORD wNewColor = stConfig.bEnableColor ? wColor : ELogColor::LOG_COLOR_DEFAULT;
				WORD wOtherColor = stConfig.bEnableColor ? ELogColor::LOG_COLOR_BRACKET : ELogColor::LOG_COLOR_DEFAULT;
				utils::WriteColorText(LOG_STRING("["), wOtherColor);
				utils::WriteColorText(str, wNewColor);
				utils::WriteColorText(LOG_STRING("] "), wOtherColor);
			};

			auto Prefix = [&](const Prefix_t tPrefix) -> void
			{
				// 显示时间
				if (stConfig.bShowTimestamp)
					 Format(tPrefix.szTimeStamp,
						ELogColor::LOG_COLOR_TIMESTAMP);

				// 显示日志级别
				if (stConfig.bShowLevel)
					Format(mapLevelToString.at(tPrefix.eLevel),
						mapLevelColors.at(tPrefix.eLevel));

				// 显示文件名和具体行数
				if (stConfig.bShowFile && stConfig.bShowLine)
				{
					Format(tPrefix.szFileLine,
						ELogColor::LOG_COLOR_FILELINE);
				}
				else
				{
					Format(stConfig.bShowFile ? tPrefix.szFile : tPrefix.szLine,
						ELogColor::LOG_COLOR_FILELINE);
				}
			};

			// 检查是否是重复消息
			if (szContent == stLastEntry.szContent)
			{
				// 移动光标到上一行
				::SetConsoleCursorPosition(hConsole, stLastEntry.vecPosition);
				// 输出前缀
				Prefix(tPrefix);
				// 输出控制台
				utils::WriteColorText(szContent);
				// 写入重复计数
				utils::WriteColorText(
					LOG_STRING(" [x") + LOG_TOSTRING(++stLastEntry.nCount) + LOG_STRING("]\n"),
					ELogColor::LOG_COLOR_COUNT);
			}
			else
			{
				// 获取当前控制台信息
				CONSOLE_SCREEN_BUFFER_INFO csbi;
				::GetConsoleScreenBufferInfo(hConsole, &csbi);
				// 输出前缀
				Prefix(tPrefix);
				// 输出控制台
				utils::WriteColorText(szContent + LOG_STRING("\n"));
				// 更新 LastEntry 信息
				stLastEntry.nCount = 1;
				stLastEntry.szContent = szContent;
				stLastEntry.vecPosition = csbi.dwCursorPosition;
			}

			// 重置颜色
			if (stConfig.bEnableColor)
				SetConsoleTextAttribute(hConsole, ELogColor::LOG_COLOR_DEFAULT);
		}

		// 写入文件
		inline void WriteToFile(const Prefix_t tPrefix, const LOG_STRING_TYPE& szContent)
		{
			if (!stConfig.bEnableFile) return;

			auto Prefix = [&](const Prefix_t tPrefix) -> LOG_STRING_TYPE
			{
				LOG_STRINGSTREAM_TYPE ssLogPrefix;

				// 显示时间
				if (stConfig.bShowTimestamp)
					ssLogPrefix << LOG_PREFIX(tPrefix.szTimeStamp);

				// 显示日志级别
				if (stConfig.bShowLevel)
					ssLogPrefix << LOG_PREFIX(mapLevelToString.at(tPrefix.eLevel));

				// 显示文件名和具体行数
				if (stConfig.bShowFile && stConfig.bShowLine)
				{
					ssLogPrefix << LOG_PREFIX(tPrefix.szFileLine);
				}
				else
				{
					ssLogPrefix << (stConfig.bShowFile ? LOG_PREFIX(tPrefix.szFile) : LOG_PREFIX(tPrefix.szLine));
				}

				return ssLogPrefix.str();
			};

			LOG_STRING_TYPE szFullMessage = Prefix(tPrefix) + szContent;

			// 如果文件流没有打开，尝试打开文件
			if (!fsLogFile.is_open())
			{
				// 确保日志文件所在目录存在
				std::filesystem::path logPath(stConfig.szFilePath);
				if (logPath.has_parent_path())
					std::filesystem::create_directories(logPath.parent_path());

				// 如果旧文件存在，进行重命名
				if (GetFileAttributes(stConfig.szFilePath.c_str()) != INVALID_FILE_ATTRIBUTES)
				{
					// 获得文件名
					LOG_STRING_TYPE szNewPath = stConfig.szFilePath;
					// 获得文件名后缀
					const size_t nDot = szNewPath.find_last_of(LOG_STRING("."));
					// 获得格式化时间戳
					const LOG_STRING_TYPE szTimeStamp = LOG_STRING("_") + utils::GetTimeStampString(true);
					// 查看是否有后缀，如果有后缀，则在后缀前插入时间戳，否则直接在文件名后添加时间戳
					(nDot != LOG_STRING_TYPE::npos) ? szNewPath.insert(nDot, szTimeStamp) : szNewPath.append(szTimeStamp);
					// 重命名文件
					MoveFile(stConfig.szFilePath.c_str(), szNewPath.c_str());
				}

				// 打开新文件
				fsLogFile.open(stConfig.szFilePath, LOG_IOS::out);
			}

			// 检查是否是重复消息
			if (szContent == stLastFileEntry.szContent)
			{
				// 移动光标到上一行
				fsLogFile.seekp(stLastFileEntry.posLast);
				// 写入文件
#ifdef LOG_UNICODE
				fsLogFile << utils::WideToUTF8(szFullMessage).c_str() << LOG_STRING(" [x") << ++stLastFileEntry.nCount << LOG_STRING("]\n");
#else
				fsLogFile << utils::StrToUTF8(szFullMessage) << LOG_STRING(" [x") << ++stLastFileEntry.nCount << LOG_STRING("]\n");
#endif
			}
			else
			{
				// 更新 LastFileEntry 信息
				stLastFileEntry.nCount = 1;
				stLastFileEntry.posLast = fsLogFile.tellp();
				stLastFileEntry.szContent = szContent;
				// 写入文件
#ifdef LOG_UNICODE
				fsLogFile << utils::WideToUTF8(szFullMessage).c_str() << LOG_STRING("\n");
#else
				fsLogFile << utils::StrToUTF8(szFullMessage) << LOG_STRING("\n");
#endif
			}

			// 刷新文件
			fsLogFile.flush();
		}
	}

	//===========================================================

	inline void Log(ELevel eLevel, LOG_STRING_TYPE szFile, int nLine, const LOG_CHAR_TYPE* szLogText)
	{
		if (eLevel < stConfig.eMinLevel) return;

		Prefix_t result;
		result.szTimeStamp = utils::GetTimeStampString(false);
		result.eLevel = eLevel;
		result.szFile = szFile;
		result.szLine = LOG_TOSTRING(nLine);
		result.szFileLine = szFile.append(LOG_STRING(":")).append(LOG_TOSTRING(nLine));

		// 控制台
		internal::WhiteToConsole(result, szLogText);

		// 文件输出
		internal::WriteToFile(result, szLogText);
	}

	template<typename... Args>
	inline void Log(ELevel eLevel, LOG_STRING_TYPE szFile, int nLine, const LOG_CHAR_TYPE* szFormat, Args... args)
	{
		LOG_CHAR_TYPE szOutputBuffer[512] = { 0 };
		LOG_SNPRINTF(szOutputBuffer, LOG_BUFFER_COUNT(szOutputBuffer), _TRUNCATE, szFormat, args...);
		Log(eLevel, szFile, nLine, szOutputBuffer);
	}
}

#define _LOG(level, msg) \
    Logger::Log(level, Logger::utils::GetShortFileName(LOG_STRING(__FILE__)), __LINE__, msg)

#define _LOGF(level, format, ...) \
    Logger::Log(level, Logger::utils::GetShortFileName(LOG_STRING(__FILE__)), __LINE__, format, __VA_ARGS__)

#define LOGF_DEBUG(format, ...)			_LOGF(Logger::ELevel::LOG_DEBUG,      LOG_STRING(format), __VA_ARGS__)
#define LOGF_INFO(format, ...)			_LOGF(Logger::ELevel::LOG_INFO,       LOG_STRING(format), __VA_ARGS__)
#define LOGF_WARNING(format, ...)		_LOGF(Logger::ELevel::LOG_WARNING,    LOG_STRING(format), __VA_ARGS__)
#define LOGF_ERROR(format, ...)			_LOGF(Logger::ELevel::LOG_ERROR,      LOG_STRING(format), __VA_ARGS__)
#define LOGF_FATAL(format, ...)			_LOGF(Logger::ELevel::LOG_FATAL,      LOG_STRING(format), __VA_ARGS__)

#define LOG_DEBUG(msg)					_LOG(Logger::ELevel::LOG_DEBUG,      LOG_STRING(msg))
#define LOG_INFO(msg)					_LOG(Logger::ELevel::LOG_INFO,       LOG_STRING(msg))
#define LOG_WARNING(msg)				_LOG(Logger::ELevel::LOG_WARNING,    LOG_STRING(msg))
#define LOG_ERROR(msg)					_LOG(Logger::ELevel::LOG_ERROR,      LOG_STRING(msg))
#define LOG_FATAL(msg)					_LOG(Logger::ELevel::LOG_FATAL,      LOG_STRING(msg))

#endif // LOGGER_H
