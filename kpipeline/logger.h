#ifndef KPIPELINE_LOGGER_H_
#define KPIPELINE_LOGGER_H_

#include <string>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <fmt/core.h>
#include <fmt/std.h>
#include <fmt/chrono.h>

namespace kpipeline
{
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

    template <typename... Args>
    void Log(LogLevel level, const char* filename, int line, fmt::format_string<Args...> fmt, Args&&... args);

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
  void Logger::Log(LogLevel level, const char* filename, int line, fmt::format_string<Args...> fmt, Args&&... args)
  {
    if (level < level_.load())
    {
      return;
    }

    // 在调用线程中格式化消息，以分散CPU开销
    std::string message = fmt::format(fmt, std::forward<Args>(args)...);
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
    const char* short_filename = last_slash ? last_slash + 1 : filename;

    std::string full_log = fmt::format("[{:%Y-%m-%d %H:%M:%S}] [{:^5}] [{}:{}] {}",
                                       fmt::localtime(std::time(nullptr)),
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
#define LOG_IMPL(level, ...) \
    do { \
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
