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
  stop_flag_.store(true); // 使用 .store() 设置 atomic 变量
  cv_.notify_one();
  if (writer_thread_.joinable())
  {
    writer_thread_.join();
  }
  // 析构时，确保队列中剩余的消息都被处理
  // 这里没有显式处理剩余消息，它们会在 ProcessQueue 循环的最后一次迭代中被处理
  // 如果希望在 join 之后立即 flush 剩余，可以在 join 之前加一个 ProcessQueue 的调用
  // 但当前逻辑在 stop_flag_ 为 true 且队列非空时会继续处理，直到队列为空
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
    // Lambda 表达式在 C++11 中可用
    cv_.wait(lock, [this] { return !queue_.empty() || stop_flag_.load(); }); // 使用 .load() 读取 atomic 变量

    if (stop_flag_.load() && queue_.empty()) // 使用 .load() 读取 atomic 变量
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
