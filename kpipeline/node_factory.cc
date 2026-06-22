#include "node_factory.h"
#include "workspace.h"

namespace kpipeline
{
  NodeFactory& NodeFactory::Instance()
  {
    static NodeFactory factory;
    return factory;
  }

  // NodeConfig 是 Node 的具体子类，仅用于承载从 JSON 解析出的配置数据。
  // 它的 Execute() 为空，永远不会被调用——Build() 返回的 shared_ptr<Node>
  // 会通过 Node 的拷贝构造函数初始化具体节点类型的基类部分。
  class NodeConfig final : public kpipeline::Node
  {
  public:
    friend NodeFactory;
    NodeConfig(): Node() {}
    ~NodeConfig() override = default;
    void Execute(Workspace& ws) const override {}
  };

  bool NodeFactory::Register(const std::string& type, const NodeCreator& creator)
  {
    if (creators_.count(type))
    {
      return false;
    }
    creators_[type] = creator;
    return true;
  }

  std::shared_ptr<Node> NodeFactory::Create(const Json::Value& config)
  {
    if (!config["type"].isString())
    {
      throw PipelineException("NodeFactory error: Node config missing 'type' field.");
    }
    std::string type = config["type"].asString();
    if (creators_.find(type) == creators_.end())
    {
      throw PipelineException("NodeFactory error: Unknown node type '" + type + "'.");
    }
    return creators_.at(type)(config);
  }

  std::shared_ptr<Node> NodeFactory::Build(const Json::Value& config)
  {
    if (!config["type"].isString())
    {
      throw PipelineException("NodeFactory error: Node config missing 'type' field.");
    }

    auto node = std::make_shared<NodeConfig>();

    if (!config["name"].isString())
    {
      throw PipelineException("Node config error: 'name' is missing or not a string.");
    }
    node->name_ = config["name"].asString();

    // 处理数据输入 (可选)
    const Json::Value inputs_json = config.get("inputs", Json::Value(Json::arrayValue));
    if (!inputs_json.isArray())
    {
      throw PipelineException("Node '" + node->name_ + "' config error: 'inputs' must be an array.");
    }
    for (const auto& input : inputs_json)
    {
      node->inputs_.push_back(input.asString());
    }

    // 处理控制输入 (可选)
    const Json::Value control_inputs_json = config.get("control_inputs", Json::Value(Json::arrayValue));
    if (!control_inputs_json.isArray())
    {
      throw PipelineException("Node '" + node->name_ + "' config error: 'control_inputs' must be an array.");
    }
    for (const auto& input : control_inputs_json)
    {
      node->control_inputs_.push_back(input.asString());
    }

    // 处理数据输出 (可选，但大多数节点都有)
    const Json::Value outputs_json = config.get("outputs", Json::Value(Json::arrayValue));
    if (!outputs_json.isArray())
    {
      throw PipelineException("Node '" + node->name_ + "' config error: 'outputs' must be an array.");
    }
    for (const auto& output : outputs_json)
    {
      node->outputs_.push_back(output.asString());
    }

    return node;
  }
} // namespace kpipeline
