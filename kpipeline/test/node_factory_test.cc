#include "kpipeline/node_factory.h"
#include "kpipeline/workspace.h"
#include "gtest/gtest.h"
#include <json/json.h>

namespace
{
  // 一个用于测试的简单节点实现
  class TestNode : public kpipeline::Node
  {
  public:
    explicit TestNode(const Json::Value& config) : Node(kpipeline::NodeFactory::Build(config))
    {
    }

    void Execute(kpipeline::Workspace& ws) const override
    {
      ws.Set(outputs_.at(0), std::string("executed"));
    }
  };

  // 用于注册测试的辅助函数
  std::shared_ptr<kpipeline::Node> CreateTestNode(const Json::Value& config)
  {
    return std::make_shared<TestNode>(config);
  }
} // namespace

// 测试首次注册返回 true
TEST(NodeFactoryTest, RegisterReturnsTrueOnFirstCall)
{
  // 使用唯一的类型名以避免与其他测试冲突
  std::string unique_type = "TestNode_Register_" + std::to_string(reinterpret_cast<uintptr_t>(this));
  // NodeFactory 是单例，使用固定名称测试
  bool result = kpipeline::NodeFactory::Instance().Register("TestNode_Reg_1", CreateTestNode);
  EXPECT_TRUE(result);
}

// 测试重复注册返回 false
TEST(NodeFactoryTest, RegisterReturnsFalseOnDuplicate)
{
  std::string type = "TestNode_Dup";
  kpipeline::NodeFactory::Instance().Register(type, CreateTestNode);
  bool result = kpipeline::NodeFactory::Instance().Register(type, CreateTestNode);
  EXPECT_FALSE(result);
}

// 测试未知类型抛出异常
TEST(NodeFactoryTest, CreateThrowsOnUnknownType)
{
  Json::Value config;
  config["type"] = "NonExistentNodeType_XYZ";
  EXPECT_THROW(
    kpipeline::NodeFactory::Instance().Create(config),
    kpipeline::PipelineException
  );
}

// 测试缺少 type 字段时抛出异常
TEST(NodeFactoryTest, CreateThrowsOnMissingTypeField)
{
  Json::Value config;
  config["name"] = "some_node";
  EXPECT_THROW(
    kpipeline::NodeFactory::Instance().Create(config),
    kpipeline::PipelineException
  );
}

// 测试正常注册后可正确创建节点
TEST(NodeFactoryTest, CreateReturnsValidNode)
{
  std::string type = "TestNode_Valid";
  kpipeline::NodeFactory::Instance().Register(type, CreateTestNode);

  Json::Value config;
  config["type"] = type;
  config["name"] = "my_test_node";
  config["inputs"] = Json::Value(Json::arrayValue);
  config["outputs"] = Json::Value(Json::arrayValue);

  auto node = kpipeline::NodeFactory::Instance().Create(config);
  ASSERT_NE(node, nullptr);
  EXPECT_EQ(node->GetName(), "my_test_node");
}

// 测试 Build() 正确设置 name/inputs/outputs
TEST(NodeFactoryTest, BuildSetsNameInputsOutputs)
{
  Json::Value config;
  config["type"] = "AnyType";
  config["name"] = "build_test_node";
  config["inputs"].append("input_a");
  config["inputs"].append("input_b");
  config["outputs"].append("output_x");

  auto node = kpipeline::NodeFactory::Build(config);
  ASSERT_NE(node, nullptr);
  EXPECT_EQ(node->GetName(), "build_test_node");
  ASSERT_EQ(node->GetInputs().size(), 2u);
  EXPECT_EQ(node->GetInputs()[0], "input_a");
  EXPECT_EQ(node->GetInputs()[1], "input_b");
  ASSERT_EQ(node->GetOutputs().size(), 1u);
  EXPECT_EQ(node->GetOutputs()[0], "output_x");
}

// 测试 Build() 缺少 name 字段时抛出异常
TEST(NodeFactoryTest, BuildThrowsOnMissingName)
{
  Json::Value config;
  config["type"] = "AnyType";
  // 故意不提供 name

  EXPECT_THROW(
    kpipeline::NodeFactory::Build(config),
    kpipeline::PipelineException
  );
}

// 测试 Build() 正确处理 control_inputs
TEST(NodeFactoryTest, BuildHandlesControlInputs)
{
  Json::Value config;
  config["type"] = "AnyType";
  config["name"] = "ctrl_node";
  config["control_inputs"].append("signal_a");
  config["control_inputs"].append("signal_b");

  auto node = kpipeline::NodeFactory::Build(config);
  ASSERT_NE(node, nullptr);
  ASSERT_EQ(node->GetControlInputs().size(), 2u);
  EXPECT_EQ(node->GetControlInputs()[0], "signal_a");
  EXPECT_EQ(node->GetControlInputs()[1], "signal_b");
}

// 测试 Build() 缺少 type 字段时抛出异常
TEST(NodeFactoryTest, BuildThrowsOnMissingType)
{
  Json::Value config;
  config["name"] = "no_type_node";

  EXPECT_THROW(
    kpipeline::NodeFactory::Build(config),
    kpipeline::PipelineException
  );
}

// 测试 Build() 当 inputs 不是数组时抛出异常
TEST(NodeFactoryTest, BuildThrowsOnNonArrayInputs)
{
  Json::Value config;
  config["type"] = "AnyType";
  config["name"] = "bad_inputs_node";
  config["inputs"] = "not_an_array"; // 错误：应该是数组

  EXPECT_THROW(
    kpipeline::NodeFactory::Build(config),
    kpipeline::PipelineException
  );
}

// 测试 Build() 当 outputs 不是数组时抛出异常
TEST(NodeFactoryTest, BuildThrowsOnNonArrayOutputs)
{
  Json::Value config;
  config["type"] = "AnyType";
  config["name"] = "bad_outputs_node";
  config["outputs"] = 12345; // 错误：应该是数组

  EXPECT_THROW(
    kpipeline::NodeFactory::Build(config),
    kpipeline::PipelineException
  );
}

// 测试 Build() 当 control_inputs 不是数组时抛出异常
TEST(NodeFactoryTest, BuildThrowsOnNonArrayControlInputs)
{
  Json::Value config;
  config["type"] = "AnyType";
  config["name"] = "bad_ctrl_node";
  config["control_inputs"] = true; // 错误：应该是数组

  EXPECT_THROW(
    kpipeline::NodeFactory::Build(config),
    kpipeline::PipelineException
  );
}

// 测试 Build() 提供的可选字段省略时默认处理
TEST(NodeFactoryTest, BuildHandlesOptionalFieldsOmitted)
{
  Json::Value config;
  config["type"] = "AnyType";
  config["name"] = "minimal_node";
  // 不提供 inputs, outputs, control_inputs

  auto node = kpipeline::NodeFactory::Build(config);
  ASSERT_NE(node, nullptr);
  EXPECT_TRUE(node->GetInputs().empty());
  EXPECT_TRUE(node->GetOutputs().empty());
  EXPECT_TRUE(node->GetControlInputs().empty());
}
