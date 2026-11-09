#pragma once

#include <chrono>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <windows.h>

class Logger
{
public:
	static void Initialize()
	{
#if defined(_DEBUG) || defined(VVV_DEVELOPMENT)
		const std::lock_guard lock(mutex);
		if (stream.is_open()) return;
		auto root = std::filesystem::absolute(std::filesystem::path(__FILE__)).parent_path();
		while (!root.empty() && !std::filesystem::exists(root / "VEER.sln"))
		{
			const auto parent = root.parent_path();
			if (parent == root) throw std::runtime_error("Logger: project folder not found.");
			root = parent;
		}
		if (root.empty()) throw std::runtime_error("Logger: project folder not found.");
		std::filesystem::create_directories(root / "Log");
		const auto now = std::chrono::system_clock::now();
		const auto time = std::chrono::system_clock::to_time_t(now);
		std::tm local{};
		localtime_s(&local, &time);
		std::ostringstream name;
		name << std::put_time(&local, "%Y_%m_%d_%H_%M_%S") << '_'
			<< std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() % 1000
			<< '_' << GetCurrentProcessId() << ".log";
		path = root / "Log" / name.str();
		stream.open(path, std::ios::binary | std::ios::out);
		if (!stream) throw std::runtime_error("Logger: cannot create log file.");
#endif
	}

	static void Write(const char* file, int line, std::string_view message) noexcept
	{
#if defined(_DEBUG) || defined(VVV_DEVELOPMENT)
		try
		{
			const std::lock_guard lock(mutex);
			if (!stream.is_open()) return;
			const auto now = std::chrono::system_clock::now();
			const auto time = std::chrono::system_clock::to_time_t(now);
			std::tm local{};
			localtime_s(&local, &time);
			while (!message.empty() && (message.back() == '\n' || message.back() == '\r')) message.remove_suffix(1);
			stream << '[' << std::put_time(&local, "%Y-%m-%d %H:%M:%S") << '.'
				<< std::setfill('0') << std::setw(3)
				<< std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() % 1000
				<< "] [" << std::filesystem::path(file).filename().string() << ':' << line << "] "
				<< message << '\n';
			stream.flush();
		}
		catch (...) {}
#endif
	}

	template<class... Args> requires (sizeof...(Args) > 0)
	static void Write(const char* file, int line, const char* format, Args... args) noexcept
	{
#if defined(_DEBUG) || defined(VVV_DEVELOPMENT)
		try
		{
			const int length = std::snprintf(nullptr, 0, format, args...);
			if (length < 0) return;
			std::string message(static_cast<size_t>(length) + 1, '\0');
			std::snprintf(message.data(), message.size(), format, args...);
			message.resize(length);
			Write(file, line, message);
		}
		catch (...) {}
#endif
	}

	static std::filesystem::path GetPath() { const std::lock_guard lock(mutex); return path; }

private:
	inline static std::mutex mutex;
	inline static std::ofstream stream;
	inline static std::filesystem::path path;
};

#if defined(_DEBUG) || defined(VVV_DEVELOPMENT)
#define Log(...) Logger::Write(__FILE__, __LINE__, __VA_ARGS__)
#else
#define Log(...) ((void)0)
#endif
