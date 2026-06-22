#include "kpipeline/graph_builder.h"
#include "kpipeline/node_factory.h"
#include "kpipeline/workspace.h"
#include "gtest/gtest.h"
#include <fstream>
#include <json/json.h>

namespace
{
  // 用于 GraphBuilder 测试的简单节点
  class TestBuilderNode : public kpipeline::Node
  {
  public:
    explicit TestBuilderNode(const Json::Value& config) : Node(kpipeline::NodeFactory::Build(config))
    {
    }

    void Execute(kpipeline::Workspace& ws) const override
    {
      // 如果有输入，复制到输出；如果没有输入，设置默认值
      for (const auto& output : outputs_)
      {
        if (!inputs_.empty() && ws.Has(inputs_.at(0)))
        {
          ws.SetAny(output, ws.GetAny(inputs_.at(0)));
        }
        else
        {
          ws.Set(output, std::string("default_value"));
        }
      }
    }
  };

  // 注册测试节点
  REGISTER_NODE(TestBuilderNode);

  // 获取测试数据文件路径的辅助函数
  // Bazel 测试运行时，data 文件位于 runfiles 中
  std::string GetTestFilePath(const std::string& filename)
  {
    // 在 Bazel 测试中，data 文件被放在运行目录中
    return "kpipeline/test/" + filename;
  }
} // namespace

// 测试文件不存在时抛出异常
TEST(GraphBuilderTest, FromFileThrowsOnMissingFile)
{
  EXPECT_THROW(
    kpipeline::GraphBuilder::FromFile("/nonexistent/path/to/config.json"),
    kpipeline::PipelineException
  );
}

// 测试非法 JSON 时抛出异常
TEST(GraphBuilderTest, FromFileThrowsOnInvalidJson)
{
  // 创建一个临时文件包含非法 JSON
  std::string temp_path = "/tmp/kpipeline_test_invalid.json";
  {
    std::ofstream ofs(temp_path);
    ofs << "{ this is not valid json }}}";
  }

  EXPECT_THROW(
    kpipeline::GraphBuilder::FromFile(temp_path),
    kpipeline::PipelineException
  );

  // 清理
  std::remove(temp_path.c_str());
}

// 测试缺少 nodes 字段时抛出异常
TEST(GraphBuilderTest, FromFileThrowsOnMissingNodesField)
{
  EXPECT_THROW(
    kpipeline::GraphBuilder::FromFile(GetTestFilePath("test_invalid_no_nodes.json")),
    kpipeline::PipelineException
  );
}

// 测试合法 JSON 构建可执行的 Graph
TEST(GraphBuilderTest, FromFileBuildsValidGraph)
{
  auto graph = kpipeline::GraphBuilder::FromFile(GetTestFilePath("test_valid_graph.json"));
  ASSERT_NE(graph, nullptr);

  // 运行图验证它确实可以执行
  kpipeline::Workspace ws;
  graph->Run(ws, 1);

  // 验证 sink 节点的输出存在
  EXPECT_TRUE(ws.Has("result"));
  EXPECT_EQ(ws.Get<std::string>("result"), "default_value");
}

// 测试空 JSON 配置文件
TEST(GraphBuilderTest, FromFileThrowsOnEmptyJson)
{
  std::string temp_path = "/tmp/kpipeline_test_empty.json";
  {
    std::ofstream ofs(temp_path);
    ofs << "{}";
  }

  EXPECT_THROW(
    kpipeline::GraphBuilder::FromFile(temp_path),
    kpipeline::PipelineException
  );

  std::remove(temp_path.c_str());
}

// 测试 nodes 字段不是数组时抛出异常
TEST(GraphBuilderTest, FromFileThrowsOnNodesNotArray)
{
  std::string temp_path = "/tmp/kpipeline_test_nodes_not_array.json";
  {
    std::ofstream ofs(temp_path);
    ofs << R"({"name": "bad", "nodes": "not_an_array"})";
  }

  EXPECT_THROW(
    kpipeline::GraphBuilder::FromFile(temp_path),
    kpipeline::PipelineException
  );

  std::remove(temp_path.c_str());
}
