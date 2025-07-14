#ifndef KPIPELINE_NODE_H_
#define KPIPELINE_NODE_H_

#include <string>
#include <vector>
#include "workspace.h"
#include <memory>

namespace kpipeline
{
  // 一个空的结构体，用作控制流的 "激活信号"
  struct ControlSignal
  {
  };

  class Node
  {
  public:
    friend class NodeFactory;
    // explicit Node(const Json::Value& config);
    explicit Node() = default;

    virtual ~Node() = default;

    virtual void Execute(Workspace& ws) const = 0;

    const std::string& GetName() const { return name_; }
    const std::vector<std::string>& GetInputs() const { return inputs_; }
    const std::vector<std::string>& GetOutputs() const { return outputs_; }
    // 获取控制输入的接口
    const std::vector<std::string>& GetControlInputs() const { return control_inputs_; }

    // ====== 显式声明默认的拷贝赋值运算符 ======
    Node& operator=(const Node& other) = default;

    // ====== 显式声明默认的移动赋值运算符（C++11 及更高版本） ======
    // 这可以提高使用临时对象或右值赋值时的性能。
    Node& operator=(Node&& other) noexcept = default;

  protected:
    // 不依赖 JSON 的构造函数，供纯代码定义图时使用
    Node(std::string name,
         std::vector<std::string> inputs,
         std::vector<std::string> outputs,
         std::vector<std::string> control_inputs = {})
      : name_(std::move(name)),
        inputs_(std::move(inputs)),
        outputs_(std::move(outputs)),
        control_inputs_(std::move(control_inputs))
    {
    }

    // ====== 从 std::shared_ptr<Node> 拷贝构造 ======
    // 这个构造函数允许你用一个 shared_ptr 所指向的 Node 的内容来初始化一个新的 Node 对象。
    // 如果 shared_ptr 为空，则 Node 会被默认构造（空名称，空向量）。
    explicit Node(const std::shared_ptr<Node>& source_ptr)
      : name_(), inputs_(), outputs_(), control_inputs_() // 先默认构造所有成员
    {
      if (source_ptr)
      {
        // 如果 shared_ptr 非空，则拷贝其指向的 Node 的内容
        name_ = source_ptr->name_;
        inputs_ = source_ptr->inputs_;
        outputs_ = source_ptr->outputs_;
        control_inputs_ = source_ptr->control_inputs_;
      }
      // 如果 source_ptr 为空，则成员保持默认构造状态 (空字符串和空向量)
    }

  protected:
    std::string name_;
    std::vector<std::string> inputs_;
    std::vector<std::string> outputs_;
    // 新增：控制依赖
    std::vector<std::string> control_inputs_;
  };
} // namespace kpipeline

#endif // KPIPELINE_NODE_H_
