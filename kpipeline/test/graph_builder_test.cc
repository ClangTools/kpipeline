#include "kpipeline/graph_builder.h"
#include "kpipeline/node_factory.h"
#include "kpipeline/workspace.h"
#include "gtest/gtest.h"
#include <json/json.h>
#include <cstdlib>

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

  REGISTER_NODE(TestBuilderNode);

  // 跨平台的测试数据文件路径解析
  // Bazel 在测试时设置 TEST_SRCDIR 和 TEST_WORKSPACE 环境变量
  // Linux/macOS: TEST_SRCDIR 指向 runfiles 目录
  // Windows:     同样通过环境变量定位 runfiles
  std::string GetTestFilePath(const std::string& filename)
  {
    const char* srcdir = std::getenv("TEST_SRCDIR");
    const char* workspace = std::getenv("TEST_WORKSPACE");
    if (srcdir && workspace)
    {
      return std::string(srcdir) + "/" + workspace + "/kpipeline/test/" + filename;
    }
    // Fallback: 使用相对路径（适用于从工作区根目录直接运行的场景）
    return "kpipeline/test/" + filename;
  }
} // namespace

// 测试文件不存在时抛出异常
TEST(GraphBuilderTest, FromFileThrowsOnMissingFile)
{
  EXPECT_THROW(
    kpipeline::GraphBuilder::FromFile("this_file_does_not_exist_12345.json"),
    kpipeline::PipelineException
  );
}

// 测试非法 JSON 时抛出异常
TEST(GraphBuilderTest, FromFileThrowsOnInvalidJson)
{
  std::string path = GetTestFilePath("test_invalid_json.json");
  EXPECT_THROW(
    kpipeline::GraphBuilder::FromFile(path),
    kpipeline::PipelineException
  );
}

// 测试缺少 nodes 字段时抛出异常
TEST(GraphBuilderTest, FromFileThrowsOnMissingNodesField)
{
  std::string path = GetTestFilePath("test_invalid_no_nodes.json");
  EXPECT_THROW(
    kpipeline::GraphBuilder::FromFile(path),
    kpipeline::PipelineException
  );
}

// 测试合法 JSON 构建可执行的 Graph
TEST(GraphBuilderTest, FromFileBuildsValidGraph)
{
  std::string path = GetTestFilePath("test_valid_graph.json");
  auto graph = kpipeline::GraphBuilder::FromFile(path);
  ASSERT_NE(graph, nullptr);

  kpipeline::Workspace ws;
  graph->Run(ws, 1);

  EXPECT_TRUE(ws.Has("result"));
  EXPECT_EQ(ws.Get<std::string>("result"), "default_value");
}

// 测试空 JSON 配置文件
TEST(GraphBuilderTest, FromFileThrowsOnEmptyJson)
{
  std::string path = GetTestFilePath("test_empty.json");
  EXPECT_THROW(
    kpipeline::GraphBuilder::FromFile(path),
    kpipeline::PipelineException
  );
}

// 测试 nodes 字段不是数组时抛出异常
TEST(GraphBuilderTest, FromFileThrowsOnNodesNotArray)
{
  std::string path = GetTestFilePath("test_nodes_not_array.json");
  EXPECT_THROW(
    kpipeline::GraphBuilder::FromFile(path),
    kpipeline::PipelineException
  );
}

// 验证 GetTestFilePath 解析出的路径指向真实存在的文件
TEST(GraphBuilderTest, TestFilePathResolvesToExistingFile)
{
  std::string path = GetTestFilePath("test_valid_graph.json");
  std::ifstream f(path);
  ASSERT_TRUE(f.good()) << "Cannot open test data file at resolved path: " << path
                         << "\nTEST_SRCDIR=" << (std::getenv("TEST_SRCDIR") ? std::getenv("TEST_SRCDIR") : "(null)")
                         << "\nTEST_WORKSPACE=" << (std::getenv("TEST_WORKSPACE") ? std::getenv("TEST_WORKSPACE") : "(null)");
}
