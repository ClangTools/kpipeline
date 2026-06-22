#include "logger.h"
#include <iostream>
#include <cstring> // 确保包含 strrchr 需要的头文件，虽然在logger.h中已添加，但保持完整性

using namespace kpipeline;

Logger& Logger::Get()
{
  static Logger instance;
  return instance;
}

Logger::Logger() : level_(LogLevel::WARN), stop_flag_(false)
{
  // std::thread 在 C++11 中可用
  writer_thread_ = std::thread(&Logger::ProcessQueue, this);
}

Logger::~Logger()
{
  stop_flag_.store(true);
  cv_.notify_one();
  if (writer_thread_.joinable())
  {
    writer_thread_.join();
  }
}

void Logger::SetLevel(LogLevel level)
{
  level_.store(level); // 使用 .store() 设置 atomic 变量
}

LogLevel Logger::GetLevel() const
{
  return level_.load(); // 使用 .load() 读取 atomic 变量
}

void Logger::ProcessQueue()
{
  while (true)
  {
    std::unique_lock<std::mutex> lock(mutex_);
    // 使用带超时的 wait，避免析构时 notify 丢失导致线程永久阻塞。
    // 即使 notify_one 在线程写 I/O 期间被错过，线程也会在超时后醒来检查 stop_flag_。
    cv_.wait_for(lock, std::chrono::milliseconds(100),
                 [this] { return !queue_.empty() || stop_flag_.load(); });

    if (stop_flag_.load() && queue_.empty())
    {
      break;
    }

    // 批量处理，减少锁的争用
    std::queue<std::string> local_queue;
    // std::queue::swap 在 C++11 中可用
    local_queue.swap(queue_);
    lock.unlock(); // 释放锁，允许其他线程继续往队列中添加消息

    while (!local_queue.empty())
    {
      std::cout << local_queue.front() << std::endl;
      local_queue.pop();
    }
  }
}
