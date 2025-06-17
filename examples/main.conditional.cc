#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <memory>
#include <json/json.h>

#include "graph_builder.h"
#include "node_factory.h"

namespace conditional_nodes
{
  class LoadNumberNode : public kpipeline::Node
  {
  public:
    explicit LoadNumberNode(const Json::Value& config) : Node(config)
    {
    }

    void Execute(kpipeline::Workspace& ws) const override
    {
      ws.Set(outputs_.at(0), ws.Get<int>(inputs_.at(0)));
    }
  };

  REGISTER_NODE(LoadNumberNode);

  class RouterNode : public kpipeline::Node
  {
  public:
    explicit RouterNode(const Json::Value& config) : Node(config)
    {
    }

    void Execute(kpipeline::Workspace& ws) const override
    {
      int value = ws.Get<int>(inputs_.at(0));
      if (value > 0)
      {
        std::cout << "    > Decision: Routing to positive branch." << std::endl;
        ws.Set(outputs_.at(0), kpipeline::ControlSignal{});
      }
      else
      {
        std::cout << "    > Decision: Routing to negative branch." << std::endl;
        ws.Set(outputs_.at(1), kpipeline::ControlSignal{});
      }
    }
  };

  REGISTER_NODE(RouterNode);

  class ProcessBranchNode : public kpipeline::Node
  {
  public:
    explicit ProcessBranchNode(const Json::Value& config) : Node(config)
    {
      message_ = config["params"]["message"].asString();
    }

    void Execute(kpipeline::Workspace& ws) const override
    {
      int value = ws.Get<int>(inputs_.at(0));
      std::string result = message_ + " Value was: " + std::to_string(value);
      ws.Set(outputs_.at(0), result);
    }

  private:
    std::string message_;
  };

  REGISTER_NODE(ProcessBranchNode);

  // 结果收集节点
  class CollectResultNode : public kpipeline::Node
  {
  public:
    explicit CollectResultNode(const Json::Value& config) : Node(config)
    {
    }

    void Execute(kpipeline::Workspace& ws) const override
    {
      // ======================== 修复开始 ========================
      // 正确且健壮地处理可选输入

      std::string result_str = "Error: No result found from any branch.";

      // JSON 中定义的第一个输入是 "positive_result"
      const std::string& positive_input_name = inputs_.at(0);
      if (ws.Has(positive_input_name))
      {
        result_str = ws.Get<std::string>(positive_input_name);
      }
      // JSON 中定义的第二个输入是 "negative_result"
      else
      {
        const std::string& negative_input_name = inputs_.at(1);
        if (ws.Has(negative_input_name))
        {
          result_str = ws.Get<std::string>(negative_input_name);
        }
      }

      ws.Set(outputs_.at(0), "Final Report: " + result_str);
      // ======================== 修复结束 ========================
    }
  };

  REGISTER_NODE(CollectResultNode);
} // namespace conditional_nodes

void run_test_case(int initial_value)
{
  std::cout << "\n--- Running Conditional Test with Input: " << initial_value << " ---\n";
  try
  {
    auto graph = kpipeline::GraphBuilder::FromFile("examples/conditional_pipeline.json");

    kpipeline::Workspace ws;
    ws.Set("initial_value", initial_value);

    graph->Run(ws, 2);

    const auto& final_report = ws.Get<std::string>("final_result");
    std::cout << "\n" << final_report << std::endl;
  }
  catch (const std::exception& e)
  {
    std::cerr << "An error occurred: " << e.what() << std::endl;
  }
}

int main(int argc, char* argv[])
{
  if (argc > 1)
  {
    try
    {
      int value = std::stoi(argv[1]);
      run_test_case(value);
    }
    catch (const std::invalid_argument& e)
    {
      std::cerr << "Invalid argument: Please provide an integer." << std::endl;
      return 1;
    }
  }
  else
  {
    run_test_case(10);
    run_test_case(-5);
  }

  return 0;
}
