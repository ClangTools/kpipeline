#include "node_factory.h"
#include "workspace.h"

namespace kpipeline
{
  NodeFactory& NodeFactory::Instance()
  {
    static NodeFactory factory;
    return factory;
  }

  class NodeTmp final : public kpipeline::Node
  {
  public:
    friend NodeFactory;

    NodeTmp(): Node()
    {
    }

    ~NodeTmp() override = default;

    void Execute(Workspace& ws) const override
    {
    }
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
    // creators_.at(type)(config);
    auto creator = creators_[type](config);
    return creator;
  }

  std::shared_ptr<Node> NodeFactory::Build(const Json::Value& config)
  {
    if (!config["type"].isString())
    {
      throw PipelineException("NodeFactory error: Node config missing 'type' field.");
    }
    std::string type = config["type"].asString();

    // creators_.at(type)(config);
    auto creator = std::make_shared<NodeTmp>();
    ([creator, config]
    {
      if (!config["name"].isString())
      {
        throw PipelineException("Node config error: 'name' is missing or not a string.");
      }
      creator->name_ = config["name"].asString();

      // 处理数据输入 (可选)
      const Json::Value inputs_json = config.get("inputs", Json::Value(Json::arrayValue));
      if (!inputs_json.isArray())
      {
        throw PipelineException("Node '" + creator->name_ + "' config error: 'inputs' must be an array.");
      }
      for (const auto& input : inputs_json)
      {
        creator->inputs_.push_back(input.asString());
      }

      // 新增：处理控制输入 (可选)
      const Json::Value control_inputs_json = config.get("control_inputs", Json::Value(Json::arrayValue));
      if (!control_inputs_json.isArray())
      {
        throw PipelineException("Node '" + creator->name_ + "' config error: 'control_inputs' must be an array.");
      }
      for (const auto& input : control_inputs_json)
      {
        creator->control_inputs_.push_back(input.asString());
      }

      // 处理数据输出 (可选，但大多数节点都有)
      const Json::Value outputs_json = config.get("outputs", Json::Value(Json::arrayValue));
      if (!outputs_json.isArray())
      {
        throw PipelineException("Node '" + creator->name_ + "' config error: 'outputs' must be an array.");
      }
      for (const auto& output : outputs_json)
      {
        creator->outputs_.push_back(output.asString());
      }
    })();
    return creator;
  }
} // namespace kpipeline
