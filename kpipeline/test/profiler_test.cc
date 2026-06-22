#include "kpipeline/profiler.h"
#include "gtest/gtest.h"
#include <chrono>
#include <thread>
#include <sstream>

// 测试 End() 正确记录耗时
TEST(ProfilerTest, EndRecordsDuration)
{
  kpipeline::Profiler profiler;

  auto start = std::chrono::high_resolution_clock::now();
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  profiler.End("slow_node", start);

  // 通过 PrintReport 间接验证：不崩溃且输出包含节点名
  // （由于 results_ 是 private，我们通过 PrintReport 来验证）
  testing::internal::CaptureStdout();
  profiler.PrintReport();
  std::string output = testing::internal::GetCapturedStdout();

  EXPECT_NE(output.find("slow_node"), std::string::npos);
  EXPECT_NE(output.find("Profiling Report"), std::string::npos);
}

// 测试空报告时 PrintReport 不崩溃
TEST(ProfilerTest, PrintReportHandlesEmpty)
{
  kpipeline::Profiler profiler;

  testing::internal::CaptureStdout();
  profiler.PrintReport();
  std::string output = testing::internal::GetCapturedStdout();

  EXPECT_NE(output.find("No nodes executed"), std::string::npos);
}

// 测试多个节点的记录与排序
TEST(ProfilerTest, MultipleNodesSortedByDuration)
{
  kpipeline::Profiler profiler;

  // 记录三个不同耗时的节点
  auto t1 = std::chrono::high_resolution_clock::now();
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  profiler.End("fast", t1);

  auto t2 = std::chrono::high_resolution_clock::now();
  std::this_thread::sleep_for(std::chrono::milliseconds(80));
  profiler.End("slow", t2);

  auto t3 = std::chrono::high_resolution_clock::now();
  std::this_thread::sleep_for(std::chrono::milliseconds(30));
  profiler.End("medium", t3);

  testing::internal::CaptureStdout();
  profiler.PrintReport();
  std::string output = testing::internal::GetCapturedStdout();

  // 验证三个节点都出现在报告中
  EXPECT_NE(output.find("fast"), std::string::npos);
  EXPECT_NE(output.find("slow"), std::string::npos);
  EXPECT_NE(output.find("medium"), std::string::npos);

  // 验证排序：slow 应该出现在 fast 之前（按耗时降序）
  auto slow_pos = output.find("slow");
  auto fast_pos = output.find("fast");
  EXPECT_LT(slow_pos, fast_pos);
}

// 测试线程安全性：多线程并发调用 End()
TEST(ProfilerTest, ConcurrentEndIsThreadSafe)
{
  kpipeline::Profiler profiler;
  std::vector<std::thread> threads;

  for (int i = 0; i < 10; ++i)
  {
    threads.emplace_back([&profiler, i]()
    {
      auto start = std::chrono::high_resolution_clock::now();
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
      profiler.End("node_" + std::to_string(i), start);
    });
  }

  for (auto& t : threads)
  {
    t.join();
  }

  // 验证不崩溃，且所有节点都被记录
  testing::internal::CaptureStdout();
  profiler.PrintReport();
  std::string output = testing::internal::GetCapturedStdout();

  for (int i = 0; i < 10; ++i)
  {
    EXPECT_NE(output.find("node_" + std::to_string(i)), std::string::npos);
  }
}
