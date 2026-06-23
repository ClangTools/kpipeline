#include "kpipeline/graph_builder.h"
#include "kpipeline/node_factory.h"
#include "kpipeline/workspace.h"
#include "gtest/gtest.h"
#include <json/json.h>
#include <fstream>
#include <cstdlib>
#include <cstdio>

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

  // 跨平台临时文件辅助类：构造时创建，析构时自动删除
  // 不依赖 <filesystem>，兼容 macOS 10.15 以下和 Windows
  class TempJsonFile
  {
  public:
    TempJsonFile(const std::string& content)
    {
      // 使用 std::tmpnam 生成唯一文件名
      char name_buf[L_tmpnam];
      if (std::tmpnam(name_buf) != nullptr)
      {
        path_ = std::string(name_buf) + ".json";
      }
      else
      {
        // fallback: 使用计数器
        path_ = "kpipeline_test_" + std::to_string(counter_++) + ".json";
      }
      std::ofstream ofs(path_);
      ofs << content;
    }

    ~TempJsonFile()
    {
      std::remove(path_.c_str());
    }

    const std::string& GetPath() const { return path_; }

  private:
    std::string path_;
    static int counter_;
  };

  int TempJsonFile::counter_ = 0;

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
  TempJsonFile tmp("{ this is not valid json }}}");
  EXPECT_THROW(
    kpipeline::GraphBuilder::FromFile(tmp.GetPath()),
    kpipeline::PipelineException
  );
}

// 测试缺少 nodes 字段时抛出异常
TEST(GraphBuilderTest, FromFileThrowsOnMissingNodesField)
{
  TempJsonFile tmp(R"({"name": "test", "description": "missing nodes"})");
  EXPECT_THROW(
    kpipeline::GraphBuilder::FromFile(tmp.GetPath()),
    kpipeline::PipelineException
  );
}

// 测试合法 JSON 构建可执行的 Graph
TEST(GraphBuilderTest, FromFileBuildsValidGraph)
{
  TempJsonFile tmp(R"({
    "name": "test_graph",
    "nodes": [
      {
        "type": "TestBuilderNode",
        "name": "source",
        "outputs": ["data"]
      },
      {
        "type": "TestBuilderNode",
        "name": "sink",
        "inputs": ["data"],
        "outputs": ["result"]
      }
    ]
  })");

  auto graph = kpipeline::GraphBuilder::FromFile(tmp.GetPath());
  ASSERT_NE(graph, nullptr);

  kpipeline::Workspace ws;
  graph->Run(ws, 1);

  EXPECT_TRUE(ws.Has("result"));
  EXPECT_EQ(ws.Get<std::string>("result"), "default_value");
}

// 测试空 JSON 配置文件
TEST(GraphBuilderTest, FromFileThrowsOnEmptyJson)
{
  TempJsonFile tmp("{}");
  EXPECT_THROW(
    kpipeline::GraphBuilder::FromFile(tmp.GetPath()),
    kpipeline::PipelineException
  );
}

// 测试 nodes 字段不是数组时抛出异常
TEST(GraphBuilderTest, FromFileThrowsOnNodesNotArray)
{
  TempJsonFile tmp(R"({"name": "bad", "nodes": "not_an_array"})");
  EXPECT_THROW(
    kpipeline::GraphBuilder::FromFile(tmp.GetPath()),
    kpipeline::PipelineException
  );
}
