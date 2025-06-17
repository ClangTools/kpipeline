#include "node.h"
#include "workspace.h"

namespace kpipeline
{
  Node::Node(const Json::Value& config)
  {
    if (!config["name"].isString())
    {
      throw PipelineException("Node config error: 'name' is missing or not a string.");
    }
    name_ = config["name"].asString();

    // 处理数据输入 (可选)
    const Json::Value inputs_json = config.get("inputs", Json::Value(Json::arrayValue));
    if (!inputs_json.isArray())
    {
      throw PipelineException("Node '" + name_ + "' config error: 'inputs' must be an array.");
    }
    for (const auto& input : inputs_json)
    {
      inputs_.push_back(input.asString());
    }

    // 新增：处理控制输入 (可选)
    const Json::Value control_inputs_json = config.get("control_inputs", Json::Value(Json::arrayValue));
    if (!control_inputs_json.isArray())
    {
      throw PipelineException("Node '" + name_ + "' config error: 'control_inputs' must be an array.");
    }
    for (const auto& input : control_inputs_json)
    {
      control_inputs_.push_back(input.asString());
    }

    // 处理数据输出 (可选，但大多数节点都有)
    const Json::Value outputs_json = config.get("outputs", Json::Value(Json::arrayValue));
    if (!outputs_json.isArray())
    {
      throw PipelineException("Node '" + name_ + "' config error: 'outputs' must be an array.");
    }
    for (const auto& output : outputs_json)
    {
      outputs_.push_back(output.asString());
    }
  }
} // namespace kpipeline
