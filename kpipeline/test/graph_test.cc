#include "kpipeline/graph.h"
#include <utility>
#include <atomic>
#include <chrono>
#include <thread>
#include "gtest/gtest.h"

// --- 在测试文件中定义简单的节点用于测试 ---

// 一个简单的节点，将输入加一
class AddOneNode : public kpipeline::Node
{
public:
  // 为了测试方便，我们绕开 JSON 构造
  AddOneNode(std::string name, std::string input, std::string output)
    : Node(std::move(name), {std::move(input)}, {std::move(output)})
  {
  }

  void Execute(kpipeline::Workspace& ws) const override
  {
    int val = ws.Get<int>(inputs_.at(0));
    ws.Set(outputs_.at(0), val + 1);
  }

};

// 一个简单的节点，将输入乘以二
class MultiplyByTwoNode : public kpipeline::Node
{
public:
  MultiplyByTwoNode(std::string name, std::string input, std::string output)
    : Node(std::move(name), {std::move(input)}, {std::move(output)})
  {
  }

  void Execute(kpipeline::Workspace& ws) const override
  {
    int val = ws.Get<int>(inputs_.at(0));
    ws.Set(outputs_.at(0), val * 2);
  }

};

// 测试一个简单的线性图 (A->B)
TEST(GraphTest, LinearExecution)
{
  kpipeline::Graph graph;
  graph.AddNode(std::make_shared<AddOneNode>("AddOne", "input", "intermediate"));
  graph.AddNode(std::make_shared<MultiplyByTwoNode>("MulTwo", "intermediate", "output"));

  kpipeline::Workspace ws;
  ws.Set("input", 10);

  // 使用单线程运行以保证结果可预测
  graph.Run(ws, 1);

  ASSERT_TRUE(ws.Has("output"));
  // 预期结果: (10 + 1) * 2 = 22
  EXPECT_EQ(ws.Get<int>("output"), 22);
}

// 测试一个并行的图
TEST(GraphTest, ParallelExecution)
{
  kpipeline::Graph graph;
  graph.AddNode(std::make_shared<AddOneNode>("AddOne", "input", "output1"));
  graph.AddNode(std::make_shared<MultiplyByTwoNode>("MulTwo", "input", "output2"));

  kpipeline::Workspace ws;
  ws.Set("input", 10);

  // 使用多线程运行
  graph.Run(ws, 2);

  ASSERT_TRUE(ws.Has("output1"));
  ASSERT_TRUE(ws.Has("output2"));
  // 预期结果: 10 + 1 = 11
  EXPECT_EQ(ws.Get<int>("output1"), 11);
  // 预期结果: 10 * 2 = 20
  EXPECT_EQ(ws.Get<int>("output2"), 20);
}

// 测试图的循环检测功能
TEST(GraphTest, CycleDetection)
{
  kpipeline::Graph graph;
  // NodeA: in_b -> out_a
  graph.AddNode(std::make_shared<AddOneNode>("NodeA", "in_b", "out_a"));
  // NodeB: out_a -> in_b  (形成 A -> B -> A 的循环)
  graph.AddNode(std::make_shared<AddOneNode>("NodeB", "out_a", "in_b"));

  kpipeline::Workspace ws;

  // 预期 Run 方法会因为检测到循环而抛出异常
  EXPECT_THROW(graph.Run(ws), kpipeline::PipelineException);
}

// --- 在测试文件中定义更多测试节点 ---

// 一个抛出异常的节点
class ThrowingNode : public kpipeline::Node
{
public:
  ThrowingNode(std::string name, std::string input)
    : Node(std::move(name), {std::move(input)}, {})
  {
  }

  void Execute(kpipeline::Workspace& ws) const override
  {
    throw std::runtime_error("intentional error from " + name_);
  }
};

// 一个产出重复输出的节点
class DuplicateOutputNode : public kpipeline::Node
{
public:
  DuplicateOutputNode(std::string name, std::string input, std::string output)
    : Node(std::move(name), {std::move(input)}, {std::move(output)})
  {
  }

  void Execute(kpipeline::Workspace& ws) const override
  {
    ws.Set(outputs_.at(0), 42);
  }
};

// 独占节点：验证互斥执行
class ExclusiveNode : public kpipeline::Node
{
public:
  ExclusiveNode(std::string name, std::string input, std::string output, std::atomic<int>& concurrent_count)
    : Node(std::move(name), {std::move(input)}, {std::move(output)}),
      concurrent_count_(concurrent_count)
  {
    this->exclusive_ = true;
  }

  void Execute(kpipeline::Workspace& ws) const override
  {
    int prev = concurrent_count_.fetch_add(1);
    // 独占节点执行时，不应该有其他独占节点同时执行
    // 注意：这个断言只在独占节点之间有意义
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    concurrent_count_.fetch_sub(1);
    ws.Set(outputs_.at(0), ws.Get<int>(inputs_.at(0)) + 1);
  }

private:
  std::atomic<int>& concurrent_count_;
};

// 测试节点抛出异常时 Graph 终止并重新抛出
TEST(GraphTest, NodeExceptionHaltsGraph)
{
  kpipeline::Graph graph;
  graph.AddNode(std::make_shared<AddOneNode>("AddOne", "input", "intermediate"));
  graph.AddNode(std::make_shared<ThrowingNode>("Thrower", "intermediate"));

  kpipeline::Workspace ws;
  ws.Set("input", 10);

  EXPECT_THROW(graph.Run(ws, 1), std::runtime_error);
}

// 测试空图 Run 不报错
TEST(GraphTest, EmptyGraphRunIsNoOp)
{
  kpipeline::Graph graph;
  kpipeline::Workspace ws;

  // 空图 Run 应该直接返回，不抛出异常
  EXPECT_NO_THROW(graph.Run(ws, 1));
}

// 测试两个节点产生相同 output 时抛出异常
TEST(GraphTest, DuplicateOutputThrows)
{
  kpipeline::Graph graph;
  graph.AddNode(std::make_shared<DuplicateOutputNode>("NodeA", "input", "shared_output"));
  graph.AddNode(std::make_shared<DuplicateOutputNode>("NodeB", "input", "shared_output"));

  kpipeline::Workspace ws;
  ws.Set("input", 10);

  EXPECT_THROW(graph.Run(ws, 1), kpipeline::PipelineException);
}

// 测试独占节点互斥执行
TEST(GraphTest, ExclusiveNodeSerializesExecution)
{
  kpipeline::Graph graph;
  std::atomic<int> concurrent_count{0};
  std::atomic<int> max_concurrent{0};

  // 创建两个独占节点，它们共享同一个输入但互斥执行
  class MonitoredExclusiveNode : public kpipeline::Node
  {
  public:
    MonitoredExclusiveNode(std::string name, std::string input, std::string output,
                           std::atomic<int>& count, std::atomic<int>& max_count)
      : Node(std::move(name), {std::move(input)}, {std::move(output)}),
        count_(count), max_count_(max_count)
    {
      this->exclusive_ = true;
    }

    void Execute(kpipeline::Workspace& ws) const override
    {
      int cur = count_.fetch_add(1) + 1;
      // 更新最大并发数
      int prev_max = max_count_.load();
      while (cur > prev_max && !max_count_.compare_exchange_weak(prev_max, cur)) {}

      std::this_thread::sleep_for(std::chrono::milliseconds(50));
      count_.fetch_sub(1);
      ws.Set(outputs_.at(0), 1);
    }

  private:
    std::atomic<int>& count_;
    std::atomic<int>& max_count_;
  };

  graph.AddNode(std::make_shared<MonitoredExclusiveNode>("ExA", "input", "out_a", concurrent_count, max_concurrent));
  graph.AddNode(std::make_shared<MonitoredExclusiveNode>("ExB", "input", "out_b", concurrent_count, max_concurrent));

  kpipeline::Workspace ws;
  ws.Set("input", 0);

  // 使用多线程运行，但独占节点应该序列化执行
  graph.Run(ws, 4);

  // 独占节点的最大并发数应为 1
  EXPECT_EQ(max_concurrent.load(), 1);
}

// 测试图的打印功能不崩溃
TEST(GraphTest, PrintDoesNotCrash)
{
  kpipeline::Graph graph;
  graph.AddNode(std::make_shared<AddOneNode>("Node1", "in", "mid"));
  graph.AddNode(std::make_shared<MultiplyByTwoNode>("Node2", "mid", "out"));

  EXPECT_NO_THROW(graph.Print());
}

// 测试空图打印不崩溃
TEST(GraphTest, PrintEmptyGraphDoesNotCrash)
{
  kpipeline::Graph graph;
  EXPECT_NO_THROW(graph.Print());
}

// 测试 diamond 拓扑的图执行
TEST(GraphTest, DiamondTopologyExecution)
{
  // Diamond: Source -> (A, B) -> Merge
  // Source 产出 "data"
  // A 从 "data" 读取，产出 "result_a"
  // B 从 "data" 读取，产出 "result_b"
  // Merge 从 "result_a" 和 "result_b" 读取，产出 "final"

  class SourceNode : public kpipeline::Node
  {
  public:
    SourceNode() : Node("Source", {}, {"data"}) {}
    void Execute(kpipeline::Workspace& ws) const override
    {
      ws.Set(outputs_.at(0), 10);
    }
  };

  class DoubleNode : public kpipeline::Node
  {
  public:
    DoubleNode(std::string name, std::string input, std::string output)
      : Node(std::move(name), {std::move(input)}, {std::move(output)}) {}
    void Execute(kpipeline::Workspace& ws) const override
    {
      int val = ws.Get<int>(inputs_.at(0));
      ws.Set(outputs_.at(0), val * 2);
    }
  };

  class SumNode : public kpipeline::Node
  {
  public:
    SumNode() : Node("Sum", {"result_a", "result_b"}, {"final"}) {}
    void Execute(kpipeline::Workspace& ws) const override
    {
      int a = ws.Get<int>(inputs_.at(0));
      int b = ws.Get<int>(inputs_.at(1));
      ws.Set(outputs_.at(0), a + b);
    }
  };

  kpipeline::Graph graph;
  graph.AddNode(std::make_shared<SourceNode>());
  graph.AddNode(std::make_shared<DoubleNode>("A", "data", "result_a"));
  graph.AddNode(std::make_shared<DoubleNode>("B", "data", "result_b"));
  graph.AddNode(std::make_shared<SumNode>());

  kpipeline::Workspace ws;
  graph.Run(ws, 4);

  ASSERT_TRUE(ws.Has("final"));
  // data=10, A: 10*2=20, B: 10*2=20, Sum: 20+20=40
  EXPECT_EQ(ws.Get<int>("final"), 40);
}
