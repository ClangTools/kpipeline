#include "kpipeline/graph.h"
#include <utility>
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
