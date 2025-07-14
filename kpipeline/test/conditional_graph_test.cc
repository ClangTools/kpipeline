#include "graph.h"
#include "gtest/gtest.h"

// --- 在测试文件中定义用于条件分支测试的节点 ---

namespace
{
  // 路由节点：如果输入 > 0，激活 "route_a"；否则激活 "route_b"
  class TestRouterNode : public kpipeline::Node
  {
  public:
    TestRouterNode() : Node("Router", {"input"}, {"route_a", "route_b"})
    {
    }

    void Execute(kpipeline::Workspace& ws) const override
    {
      if (ws.Get<int>("input") > 0)
      {
        ws.Set("route_a", kpipeline::ControlSignal{});
      }
      else
      {
        ws.Set("route_b", kpipeline::ControlSignal{});
      }
    }
  };

  // 分支处理节点：将输入字符串附加到自身
  class BranchProcessNode : public kpipeline::Node
  {
  public:
    BranchProcessNode(const std::string& name, const std::string& control_signal)
      : Node(name, {"branch_input"}, {"branch_output_" + name}, {control_signal})
    {
    }

    void Execute(kpipeline::Workspace& ws) const override
    {
      auto val = ws.Get<std::string>("branch_input");
      ws.Set(outputs_.at(0), val + "_" + name_);
    }
  };

  // 合并节点：收集存在的结果
  class MergeNode : public kpipeline::Node
  {
  public:
    MergeNode() : Node("Merge", {"branch_output_BranchA", "branch_output_BranchB"}, {"final_output"})
    {
    }

    void Execute(kpipeline::Workspace& ws) const override
    {
      std::string result = "none";
      if (ws.Has(inputs_.at(0)))
      {
        result = ws.Get<std::string>(inputs_.at(0));
      }
      else if (ws.Has(inputs_.at(1)))
      {
        result = ws.Get<std::string>(inputs_.at(1));
      }
      ws.Set(outputs_.at(0), result);
    }
  };
} // namespace

// 测试用例基类，用于设置一个标准的条件图
class ConditionalGraphTest : public ::testing::Test
{
protected:
  void SetUp() override
  {
    // 构建图:
    //               +---------+
    // (input)------>| Router  |---+----("route_a")----+
    //               +---------+   |                   |
    //                           |                   V
    //                           |            +-------------+
    //                           |            |   BranchA   |<----(branch_input)
    //                           |            +-------------+             ^
    //                           |                   |                    |
    //                           |           ("branch_output_BranchA")    |
    //                           |                   |                    |
    //                           +----("route_b")----+--------------+     |
    //                                               |              |     |
    //                                               V              V     |
    //                                        +-------------+    +-------+----+
    //                                        |   BranchB   |--->|   Merge    |--->("final_output")
    //                                        +-------------+    +------------+
    //                                               ^              ^
    //                                               |              |
    //                                    (branch_input) ("branch_output_BranchB")

    graph.AddNode(std::make_shared<TestRouterNode>());
    graph.AddNode(std::make_shared<BranchProcessNode>("BranchA", "route_a"));
    graph.AddNode(std::make_shared<BranchProcessNode>("BranchB", "route_b"));
    graph.AddNode(std::make_shared<MergeNode>());
  }

  kpipeline::Graph graph;
};

// 测试当路由走向分支A时，是否只有分支A被执行
TEST_F(ConditionalGraphTest, ExecutesBranchAWhenInputIsPositive)
{
  kpipeline::Workspace ws;
  ws.Set("input", 10);
  ws.Set("branch_input", std::string("data"));

  graph.Run(ws, 2);

  // 验证最终输出是否来自 BranchA
  ASSERT_TRUE(ws.Has("final_output"));
  EXPECT_EQ(ws.Get<std::string>("final_output"), "data_BranchA");

  // 验证 BranchA 的输出存在
  EXPECT_TRUE(ws.Has("branch_output_BranchA"));
  // 验证 BranchB 的输出不存在（因为被剪枝了）
  EXPECT_FALSE(ws.Has("branch_output_BranchB"));
}

// 测试当路由走向分支B时，是否只有分支B被执行
TEST_F(ConditionalGraphTest, ExecutesBranchBWhenInputIsNegative)
{
  kpipeline::Workspace ws;
  ws.Set("input", -10);
  ws.Set("branch_input", std::string("data"));

  graph.Run(ws, 2);

  // 验证最终输出是否来自 BranchB
  ASSERT_TRUE(ws.Has("final_output"));
  EXPECT_EQ(ws.Get<std::string>("final_output"), "data_BranchB");

  // 验证 BranchA 的输出不存在
  EXPECT_FALSE(ws.Has("branch_output_BranchA"));
  // 验证 BranchB 的输出存在
  EXPECT_TRUE(ws.Has("branch_output_BranchB"));
}
