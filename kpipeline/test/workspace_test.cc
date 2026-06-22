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

// 测试 SetAny/GetAny 保持内部类型正确
TEST(WorkspaceTest, SetAnyAndGetAnyPreservesType)
{
  kpipeline::Workspace ws;

  // 用 Set 存入 int，用 SetAny 传递（模拟 SubGraph 场景）
  ws.Set("original", 42);
  std::any value = ws.GetAny("original");

  // 用 SetAny 存入另一个 workspace
  kpipeline::Workspace ws2;
  ws2.SetAny("transferred", value);

  // 应该能通过原始类型 Get 回来
  EXPECT_EQ(ws2.Get<int>("transferred"), 42);
}

// 测试 SetAny 与 GetAny 对自定义类型的支持
TEST(WorkspaceTest, SetAnyAndGetAnyWithCustomType)
{
  kpipeline::Workspace ws;
  CustomData data{202, "transfer_test"};
  ws.Set("custom", data);

  std::any value = ws.GetAny("custom");

  kpipeline::Workspace ws2;
  ws2.SetAny("custom_copy", value);

  EXPECT_EQ(ws2.Get<CustomData>("custom_copy"), data);
}

// 测试并发读写安全性
TEST(WorkspaceTest, ConcurrentReadWrite)
{
  kpipeline::Workspace ws;
  ws.Set("shared", 0);

  std::vector<std::thread> threads;

  // 5个线程写入
  for (int i = 0; i < 5; ++i)
  {
    threads.emplace_back([&ws, i]()
    {
      ws.Set("key_" + std::to_string(i), i * 10);
    });
  }

  // 同时 5个线程读取
  for (int i = 0; i < 5; ++i)
  {
    threads.emplace_back([&ws]()
    {
      // 读取 shared key，不关心值，只关心不崩溃
      (void)ws.Get<int>("shared");
    });
  }

  for (auto& t : threads)
  {
    t.join();
  }

  // 验证写入的数据
  for (int i = 0; i < 5; ++i)
  {
    EXPECT_EQ(ws.Get<int>("key_" + std::to_string(i)), i * 10);
  }
}

// 测试覆盖已有 key 时值正确更新
TEST(WorkspaceTest, OverwriteExistingKey)
{
  kpipeline::Workspace ws;
  ws.Set("key", 100);
  EXPECT_EQ(ws.Get<int>("key"), 100);

  ws.Set("key", 200);
  EXPECT_EQ(ws.Get<int>("key"), 200);

  // 覆盖为不同类型
  ws.Set("key", std::string("now_string"));
  EXPECT_EQ(ws.Get<std::string>("key"), "now_string");
}

// 测试 GetAny 对不存在的 key 抛出异常
TEST(WorkspaceTest, GetAnyThrowsOnNotFound)
{
  kpipeline::Workspace ws;
  EXPECT_THROW(ws.GetAny("nonexistent"), kpipeline::PipelineException);
}
