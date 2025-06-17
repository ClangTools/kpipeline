#ifndef KPIPELINE_NODE_H_
#define KPIPELINE_NODE_H_

#include <string>
#include <vector>
#include <json/json.h>
#include "workspace.h"

namespace kpipeline {

  class Node {
  public:
    explicit Node(const Json::Value& config);
    virtual ~Node() = default;

    virtual void Execute(Workspace& ws) const = 0;

    const std::string& GetName() const { return name_; }
    const std::vector<std::string>& GetInputs() const { return inputs_; }
    const std::vector<std::string>& GetOutputs() const { return outputs_; }

  protected:
    std::string name_;
    std::vector<std::string> inputs_;
    std::vector<std::string> outputs_;
  };

} // namespace kpipeline

#endif // KPIPELINE_NODE_H_