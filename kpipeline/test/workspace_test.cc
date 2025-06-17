#include "kpipeline/workspace.h"
#include "gtest/gtest.h"
#include <string>
#include <thread>
#include <vector>

// 定义一个自定义结构体用于测试
struct CustomData
{
  int id;
  std::string name;

  // 为 GTest 的 ASSERT_EQ 添加比较运算符
  bool operator==(const CustomData& other) const
  {
    return id == other.id && name == other.name;
  }
};

// 测试 Workspace 的基本 Set 和 Get 功能
TEST(WorkspaceTest, SetAndGetSuccess)
{
  kpipeline::Workspace ws;

  // 测试 int
  ws.Set("my_int", 42);
  EXPECT_EQ(ws.Get<int>("my_int"), 42);

  // 测试 std::string
  ws.Set("my_string", std::string("hello"));
  EXPECT_EQ(ws.Get<std::string>("my_string"), "hello");

  // 测试自定义结构体
  CustomData data{101, "test_data"};
  ws.Set("my_custom_data", data);
  EXPECT_EQ(ws.Get<CustomData>("my_custom_data"), data);
}

// 测试当数据不存在时，Get 是否抛出异常
TEST(WorkspaceTest, GetThrowsOnNotFound)
{
  kpipeline::Workspace ws;
  EXPECT_THROW(ws.Get<int>("non_existent_key"), kpipeline::PipelineException);
}

// 测试当请求类型与实际类型不匹配时，Get 是否抛出异常
TEST(WorkspaceTest, GetThrowsOnTypeMismatch)
{
  kpipeline::Workspace ws;
  ws.Set("my_int", 42);
  EXPECT_THROW(ws.Get<std::string>("my_int"), kpipeline::PipelineException);
}

// 测试 Has 方法的正确性
TEST(WorkspaceTest, HasReturnsCorrectly)
{
  kpipeline::Workspace ws;
  ws.Set("existing_key", 123);

  EXPECT_TRUE(ws.Has("existing_key"));
  EXPECT_FALSE(ws.Has("non_existent_key"));
}

// 测试并发环境下的线程安全性
TEST(WorkspaceTest, IsThreadSafe)
{
  kpipeline::Workspace ws;
  std::vector<std::thread> threads;

  // 10个线程并发写入
  for (int i = 0; i < 10; ++i)
  {
    threads.emplace_back([&ws, i]()
    {
      ws.Set("key_" + std::to_string(i), i);
    });
  }

  for (auto& t : threads)
  {
    t.join();
  }
  threads.clear();

  // 检查所有写入是否成功
  for (int i = 0; i < 10; ++i)
  {
    EXPECT_TRUE(ws.Has("key_" + std::to_string(i)));
    EXPECT_EQ(ws.Get<int>("key_" + std::to_string(i)), i);
  }
}
