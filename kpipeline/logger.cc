#include "logger.h"
#include <iostream>
#include <cstring>

using namespace kpipeline;

Logger& Logger::Get()
{
  static Logger instance;
  return instance;
}

Logger::Logger() : level_(LogLevel::WARN), stop_flag_(false)
{
  writer_thread_ = std::thread(&Logger::ProcessQueue, this);
}

Logger::~Logger()
{
  stop_flag_ = true;
  cv_.notify_one();
  if (writer_thread_.joinable())
  {
    writer_thread_.join();
  }
}

void Logger::SetLevel(LogLevel level)
{
  level_.store(level);
}

LogLevel Logger::GetLevel() const
{
  return level_.load();
}

void Logger::ProcessQueue()
{
  while (true)
  {
    std::unique_lock<std::mutex> lock(mutex_);
    cv_.wait(lock, [this] { return !queue_.empty() || stop_flag_; });

    if (stop_flag_ && queue_.empty())
    {
      break;
    }

    // 批量处理，减少锁的争用
    std::queue<std::string> local_queue;
    local_queue.swap(queue_);
    lock.unlock();

    while (!local_queue.empty())
    {
      std::cout << local_queue.front() << std::endl;
      local_queue.pop();
    }
  }
}
