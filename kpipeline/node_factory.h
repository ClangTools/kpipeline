#ifndef KPIPELINE_NODE_FACTORY_H_
#define KPIPELINE_NODE_FACTORY_H_

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <json/json.h>
#include "node.h"

namespace kpipeline {

  using NodeCreator = std::function<std::shared_ptr<Node>(const Json::Value&)>;

  class NodeFactory {
  public:
    static NodeFactory& Instance();

    bool Register(const std::string& type, NodeCreator creator);
    std::shared_ptr<Node> Create(const Json::Value& config);

  private:
    NodeFactory() = default;
    ~NodeFactory() = default;
    NodeFactory(const NodeFactory&) = delete;
    NodeFactory& operator=(const NodeFactory&) = delete;

    std::map<std::string, NodeCreator> creators_;
  };

#define REGISTER_NODE(NodeType)                                                    \
namespace {                                                                    \
std::shared_ptr<kpipeline::Node> Creator_##NodeType(const Json::Value& config) { \
return std::make_shared<NodeType>(config);                                 \
}                                                                              \
const bool registered_##NodeType =                                             \
kpipeline::NodeFactory::Instance().Register(#NodeType, Creator_##NodeType); \
}

} // namespace kpipeline

#endif // KPIPELINE_NODE_FACTORY_H_