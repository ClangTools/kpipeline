#include "kpipeline/logger.h"
#include "gtest/gtest.h"
#include <string>
#include <thread>
#include <vector>
#include <chrono>
#include <atomic>

// 辅助函数：等待 writer 线程处理完队列中的消息
static void WaitForLoggerFlush()
{
  // Logger 使用 wait_for(100ms) 超时，等待两个周期确保处理完毕
  std::this_thread::sleep_for(std::chrono::milliseconds(250));
}

// 辅助类：保存和恢复 Logger 级别
class LoggerTest : public ::testing::Test
{
protected:
  void SetUp() override
  {
    // 保存原始级别
    original_level_ = kpipeline::Logger::Get().GetLevel();
    // 设为 DEBUG 以便测试所有级别
    kpipeline::Logger::Get().SetLevel(kpipeline::LogLevel::DEBUG);
  }

  void TearDown() override
  {
    // 恢复原始级别
    kpipeline::Logger::Get().SetLevel(original_level_);
  }

  kpipeline::LogLevel original_level_;
};

// 测试单例模式：Get() 返回同一实例
TEST_F(LoggerTest, SingletonReturnsSameInstance)
{
  auto& logger1 = kpipeline::Logger::Get();
  auto& logger2 = kpipeline::Logger::Get();
  EXPECT_EQ(&logger1, &logger2);
}

// 测试 SetLevel/GetLevel 基本功能
TEST_F(LoggerTest, SetAndGetLevel)
{
  kpipeline::Logger::Get().SetLevel(kpipeline::LogLevel::DEBUG);
  EXPECT_EQ(kpipeline::Logger::Get().GetLevel(), kpipeline::LogLevel::DEBUG);

  kpipeline::Logger::Get().SetLevel(kpipeline::LogLevel::ERROR);
  EXPECT_EQ(kpipeline::Logger::Get().GetLevel(), kpipeline::LogLevel::ERROR);

  kpipeline::Logger::Get().SetLevel(kpipeline::LogLevel::WARN);
  EXPECT_EQ(kpipeline::Logger::Get().GetLevel(), kpipeline::LogLevel::WARN);

  kpipeline::Logger::Get().SetLevel(kpipeline::LogLevel::INFO);
  EXPECT_EQ(kpipeline::Logger::Get().GetLevel(), kpipeline::LogLevel::INFO);
}

// 测试日志级别过滤：低于设置级别的日志不输出
TEST_F(LoggerTest, LevelFilteringSuppressesLowerLevels)
{
  kpipeline::Logger::Get().SetLevel(kpipeline::LogLevel::WARN);

  testing::internal::CaptureStdout();

  // DEBUG 和 INFO 应该被过滤
  LOG_DEBUG("this debug should be suppressed");
  LOG_INFO("this info should be suppressed");
  LOG_WARN("this warn should appear");
  LOG_ERROR("this error should appear");

  WaitForLoggerFlush();
  std::string output = testing::internal::GetCapturedStdout();

  // DEBUG 和 INFO 不应出现
  EXPECT_EQ(output.find("this debug should be suppressed"), std::string::npos);
  EXPECT_EQ(output.find("this info should be suppressed"), std::string::npos);
  // WARN 和 ERROR 应该出现
  EXPECT_NE(output.find("this warn should appear"), std::string::npos);
  EXPECT_NE(output.find("this error should appear"), std::string::npos);
}

// 测试 ERROR 级别设置时只有 ERROR 输出
TEST_F(LoggerTest, ErrorLevelOnlyShowsErrors)
{
  kpipeline::Logger::Get().SetLevel(kpipeline::LogLevel::ERROR);

  testing::internal::CaptureStdout();

  LOG_DEBUG("suppressed_debug");
  LOG_INFO("suppressed_info");
  LOG_WARN("suppressed_warn");
  LOG_ERROR("visible_error");

  WaitForLoggerFlush();
  std::string output = testing::internal::GetCapturedStdout();

  EXPECT_EQ(output.find("suppressed_debug"), std::string::npos);
  EXPECT_EQ(output.find("suppressed_info"), std::string::npos);
  EXPECT_EQ(output.find("suppressed_warn"), std::string::npos);
  EXPECT_NE(output.find("visible_error"), std::string::npos);
}

// 测试日志格式包含时间戳、级别、文件名、行号、消息
TEST_F(LoggerTest, LogFormatContainsExpectedFields)
{
  kpipeline::Logger::Get().SetLevel(kpipeline::LogLevel::INFO);

  testing::internal::CaptureStdout();

  LOG_INFO("format_test_message");

  WaitForLoggerFlush();
  std::string output = testing::internal::GetCapturedStdout();

  // 应该包含级别标识
  EXPECT_NE(output.find("INFO"), std::string::npos);
  // 应该包含消息内容
  EXPECT_NE(output.find("format_test_message"), std::string::npos);
  // 应该包含短文件名（logger_test.cc 而非完整路径）
  EXPECT_NE(output.find("logger_test.cc"), std::string::npos);
  // 应该包含时间戳格式（YYYY-MM-DD HH:MM:SS）
  // 简单检查是否包含日期分隔符 '-'
  EXPECT_NE(output.find("-"), std::string::npos);
  // 应该包含行号（通过检查冒号分隔的文件名:行号格式）
  EXPECT_NE(output.find(":"), std::string::npos);
}

// 测试 fmt 格式化参数正确替换
TEST_F(LoggerTest, FmtFormatArgsSubstituted)
{
  testing::internal::CaptureStdout();

  LOG_INFO("value={}, name={}", 42, "test");

  WaitForLoggerFlush();
  std::string output = testing::internal::GetCapturedStdout();

  EXPECT_NE(output.find("value=42, name=test"), std::string::npos);
  // 原始格式串不应出现
  EXPECT_EQ(output.find("{}"), std::string::npos);
}

// 测试所有日志级别标签正确
TEST_F(LoggerTest, AllLevelLabelsCorrect)
{
  kpipeline::Logger::Get().SetLevel(kpipeline::LogLevel::DEBUG);

  testing::internal::CaptureStdout();

  LOG_DEBUG("debug_msg");
  LOG_INFO("info_msg");
  LOG_WARN("warn_msg");
  LOG_ERROR("error_msg");

  WaitForLoggerFlush();
  std::string output = testing::internal::GetCapturedStdout();

  EXPECT_NE(output.find("DEBUG"), std::string::npos);
  EXPECT_NE(output.find("INFO"), std::string::npos);
  EXPECT_NE(output.find("WARN"), std::string::npos);
  EXPECT_NE(output.find("ERROR"), std::string::npos);
}

// 测试多线程并发写日志不崩溃，且所有消息最终输出
TEST_F(LoggerTest, ConcurrentLoggingIsThreadSafe)
{
  const int kThreads = 10;
  const int kMessagesPerThread = 5;
  std::atomic<int> total_written{0};

  testing::internal::CaptureStdout();

  std::vector<std::thread> threads;
  for (int t = 0; t < kThreads; ++t)
  {
    threads.emplace_back([t, &total_written]()
    {
      for (int i = 0; i < kMessagesPerThread; ++i)
      {
        LOG_INFO("thread_{}_msg_{}", t, i);
        total_written.fetch_add(1);
      }
    });
  }

  for (auto& th : threads)
  {
    th.join();
  }

  WaitForLoggerFlush();
  std::string output = testing::internal::GetCapturedStdout();

  // 验证所有消息都被输出
  EXPECT_EQ(total_written.load(), kThreads * kMessagesPerThread);
  for (int t = 0; t < kThreads; ++t)
  {
    for (int i = 0; i < kMessagesPerThread; ++i)
    {
      std::string expected = "thread_" + std::to_string(t) + "_msg_" + std::to_string(i);
      EXPECT_NE(output.find(expected), std::string::npos)
        << "Missing message: " << expected;
    }
  }
}

// 测试空消息不崩溃
TEST_F(LoggerTest, EmptyMessageDoesNotCrash)
{
  testing::internal::CaptureStdout();

  EXPECT_NO_THROW(LOG_INFO(""));

  WaitForLoggerFlush();
  std::string output = testing::internal::GetCapturedStdout();

  // 即使空消息也应该包含日志格式
  EXPECT_NE(output.find("INFO"), std::string::npos);
}

// 测试特殊字符不崩溃
TEST_F(LoggerTest, SpecialCharactersDoNotCrash)
{
  testing::internal::CaptureStdout();

  EXPECT_NO_THROW(LOG_INFO("special: \t\n\\\"'<>@#$%^&*()"));
  EXPECT_NO_THROW(LOG_INFO("unicode: hello world"));

  WaitForLoggerFlush();
  std::string output = testing::internal::GetCapturedStdout();

  EXPECT_NE(output.find("special:"), std::string::npos);
}

// 测试 thread_id_to_string 辅助函数
TEST(LoggerUtilTest, ThreadIdToStringReturnsNonEmpty)
{
  std::string id_str = kpipeline::thread_id_to_string(std::this_thread::get_id());
  EXPECT_FALSE(id_str.empty());
}

// 测试不同线程的 ID 字符串不同
TEST(LoggerUtilTest, DifferentThreadsHaveDifferentIds)
{
  std::string main_id = kpipeline::thread_id_to_string(std::this_thread::get_id());
  std::string other_id;

  std::thread t([&other_id]()
  {
    other_id = kpipeline::thread_id_to_string(std::this_thread::get_id());
  });
  t.join();

  EXPECT_NE(main_id, other_id);
}

// 测试日志级别枚举值的顺序
TEST(LoggerUtilTest, LogLevelOrdering)
{
  // 枚举值应该按 DEBUG < INFO < WARN < ERROR 递增
  EXPECT_LT(kpipeline::LogLevel::DEBUG, kpipeline::LogLevel::INFO);
  EXPECT_LT(kpipeline::LogLevel::INFO, kpipeline::LogLevel::WARN);
  EXPECT_LT(kpipeline::LogLevel::WARN, kpipeline::LogLevel::ERROR);
}
