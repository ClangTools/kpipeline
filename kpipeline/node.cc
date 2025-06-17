#include "node.h"
#include "workspace.h"

namespace kpipeline {

  Node::Node(const Json::Value& config) {
    if (!config["name"].isString()) {
      throw PipelineException("Node config error: 'name' is missing or not a string.");
    }
    name_ = config["name"].asString();

    const Json::Value inputs_json = config.get("inputs", Json::Value(Json::arrayValue));
    if (!inputs_json.isArray()) {
      throw PipelineException("Node '" + name_ + "' config error: 'inputs' must be an array.");
    }
    for (const auto& input : inputs_json) {
      inputs_.push_back(input.asString());
    }

    const Json::Value& outputs_json = config["outputs"];
    if (!outputs_json.isArray()) {
      throw PipelineException("Node '" + name_ + "' config error: 'outputs' is missing or not an array.");
    }
    for (const auto& output : outputs_json) {
      outputs_.push_back(output.asString());
    }
  }

} // namespace kpipeline