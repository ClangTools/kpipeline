#ifndef KPIPELINE_LOGGER_H_
#define KPIPELINE_LOGGER_H_

#include <string>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <cstring> // For strrchr
#include <fmt/format.h>
#include <chrono>
#include <ctime>
#include <sstream>

namespace kpipeline
{
  inline std::string thread_id_to_string(std::thread::id id)
  {
    std::ostringstream oss;
    oss << id;
    return oss.str();
  }

  enum class LogLevel
  {
    DEBUG,
    INFO,
    WARN,
    ERROR
  };

  class Logger
  {
  public:
    static Logger& Get();

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    ~Logger();

    void SetLevel(LogLevel level);
    LogLevel GetLevel() const;

    // 格式字符串类型从 fmt::format_string<Args...> 修改为 const char*
    template <typename... Args>
    void Log(LogLevel level, const char* filename, int line, const char* fmt_str, Args&&... args);

  private:
    Logger();
    void ProcessQueue();

    std::atomic<LogLevel> level_;
    std::queue<std::string> queue_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::thread writer_thread_;
    std::atomic<bool> stop_flag_;
  };

  // 模板成员函数的实现需要放在头文件中
  template <typename... Args>
  void Logger::Log(LogLevel level, const char* filename, int line, const char* fmt_str, Args&&... args)
  {
    if (level < level_.load()) // 使用 .load() 读取 atomic 变量
    {
      return;
    }

    // 在调用线程中格式化消息，以分散CPU开销
    // fmt::format 在 C++11 下仍可接受 const char* 和 variadic templates
    std::string message = fmt::format(fmt_str, std::forward<Args>(args)...);
    std::string level_str;
    switch (level)
    {
    case LogLevel::DEBUG: level_str = "DEBUG";
      break;
    case LogLevel::INFO: level_str = "INFO";
      break;
    case LogLevel::WARN: level_str = "WARN";
      break;
    case LogLevel::ERROR: level_str = "ERROR";
      break;
    }

    // 获取文件名而不是完整路径
    const char* last_slash = strrchr(filename, '/');
#ifdef _WIN32 // Windows 路径分隔符是反斜杠
    const char* last_backslash = strrchr(filename, '\\');
    if (last_backslash > last_slash) { // Prefer backslash if it's closer to end
        last_slash = last_backslash;
    }
#endif
    const char* short_filename = last_slash ? last_slash + 1 : filename;

    // C++11 日期时间格式化: 使用 std::chrono 转换为 std::time_t，然后使用 std::strftime
    std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
    std::time_t tt = std::chrono::system_clock::to_time_t(now);

    // std::localtime 或 std::gmtime 不是线程安全的，在多线程环境下应避免直接使用，
    // 或使用它们的线程安全版本 (如 POSIX 的 localtime_r, Windows 的 localtime_s)。
    // 这里使用条件编译来尝试优先使用线程安全版本。
    struct tm ptm;
#if defined(_MSC_VER)
    // Microsoft Visual C++ 编译器
    localtime_s(&ptm, &tt);
#else
    // POSIX 兼容系统 (如 Linux, macOS)
    if (localtime_r(&tt, &ptm) == nullptr)
    {
      // Fallback or error handling if localtime_r fails (unlikely)
      // For simplicity, we might fall back to non-thread-safe localtime:
      // ptm = *std::localtime(&tt);
    }
#endif
    char time_buffer[64]; // 足够存储 "%Y-%m-%d %H:%M:%S"
    std::strftime(time_buffer, sizeof(time_buffer), "%Y-%m-%d %H:%M:%S", &ptm);

    std::string full_log = fmt::format("[{}] [{:^5}] [{}:{}] {}", // 移除了 chrono 格式占位符
                                       time_buffer, // 传入格式化后的时间字符串
                                       level_str,
                                       short_filename,
                                       line,
                                       message);

    // 将格式化好的消息推入队列
    {
      std::lock_guard<std::mutex> lock(mutex_);
      queue_.push(std::move(full_log));
    }
    cv_.notify_one();
  }

  // 定义方便使用的宏
  // 宏的参数传递需要小心，__VA_ARGS__ 会将所有逗号分隔的参数视为 Log 的参数
#define LOG_IMPL(level, ...) \
    do { \
        /* 先检查日志级别，避免不必要的参数评估和格式化 */ \
        if (level >= kpipeline::Logger::Get().GetLevel()) { \
            kpipeline::Logger::Get().Log(level, __FILE__, __LINE__, __VA_ARGS__); \
        } \
    } while (0)

#define LOG_DEBUG(...) LOG_IMPL(kpipeline::LogLevel::DEBUG, __VA_ARGS__)
#define LOG_INFO(...)  LOG_IMPL(kpipeline::LogLevel::INFO,  __VA_ARGS__)
#define LOG_WARN(...)  LOG_IMPL(kpipeline::LogLevel::WARN,  __VA_ARGS__)
#define LOG_ERROR(...) LOG_IMPL(kpipeline::LogLevel::ERROR, __VA_ARGS__)
} // namespace kpipeline

#endif // KPIPELINE_LOGGER_H_
