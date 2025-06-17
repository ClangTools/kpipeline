#include "node_factory.h"
#include "workspace.h"

namespace kpipeline {

  NodeFactory& NodeFactory::Instance() {
    static NodeFactory factory;
    return factory;
  }

  bool NodeFactory::Register(const std::string& type, NodeCreator creator) {
    if (creators_.count(type)) {
      return false;
    }
    creators_[type] = std::move(creator);
    return true;
  }

  std::shared_ptr<Node> NodeFactory::Create(const Json::Value& config) {
    if (!config["type"].isString()) {
      throw PipelineException("NodeFactory error: Node config missing 'type' field.");
    }
    std::string type = config["type"].asString();
    if (creators_.find(type) == creators_.end()) {
      throw PipelineException("NodeFactory error: Unknown node type '" + type + "'.");
    }
    return creators_.at(type)(config);
  }

} // namespace kpipeline