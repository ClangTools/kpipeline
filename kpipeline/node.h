#ifndef KPIPELINE_NODE_H_
#define KPIPELINE_NODE_H_

#include <string>
#include <vector>
#include <json/json.h>
#include "workspace.h"

namespace kpipeline
{
  // 一个空的结构体，用作控制流的 "激活信号"
  struct ControlSignal
  {
  };

  class Node
  {
  public:
    explicit Node(const Json::Value& config);
    virtual ~Node() = default;

    virtual void Execute(Workspace& ws) const = 0;

    const std::string& GetName() const { return name_; }
    const std::vector<std::string>& GetInputs() const { return inputs_; }
    const std::vector<std::string>& GetOutputs() const { return outputs_; }
    // 新增：获取控制输入的接口
    const std::vector<std::string>& GetControlInputs() const { return control_inputs_; }

  protected:
    std::string name_;
    std::vector<std::string> inputs_;
    std::vector<std::string> outputs_;
    // 新增：控制依赖
    std::vector<std::string> control_inputs_;
  };
} // namespace kpipeline

#endif // KPIPELINE_NODE_H_
