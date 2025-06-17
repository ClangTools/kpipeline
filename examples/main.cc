#include <iostream>
#include <numeric>
#include <string>
#include <vector>
#include <chrono>
#include <thread>

#include "graph_builder.h"
#include "node_factory.h"

namespace nodes
{
  class LoadNumbersNode : public kpipeline::Node
  {
  public:
    explicit LoadNumbersNode(const Json::Value& config) : Node(config)
    {
    }

    void Execute(kpipeline::Workspace& ws) const override
    {
      const auto& initial_data = ws.Get<std::vector<int>>(inputs_.at(0));
      ws.Set(outputs_.at(0), initial_data);
    }
  };

  REGISTER_NODE(LoadNumbersNode);

  class SumNode : public kpipeline::Node
  {
  public:
    explicit SumNode(const Json::Value& config) : Node(config)
    {
    }

    void Execute(kpipeline::Workspace& ws) const override
    {
      const auto& numbers = ws.Get<std::vector<int>>(inputs_.at(0));
      int sum = std::accumulate(numbers.begin(), numbers.end(), 0);
      ws.Set(outputs_.at(0), sum);
    }
  };

  REGISTER_NODE(SumNode);

  class AverageNode : public kpipeline::Node
  {
  public:
    explicit AverageNode(const Json::Value& config) : Node(config)
    {
    }

    void Execute(kpipeline::Workspace& ws) const override
    {
      const auto& numbers = ws.Get<std::vector<int>>(inputs_.at(0));
      if (numbers.empty())
      {
        ws.Set(outputs_.at(0), 0.0);
        return;
      }
      double sum = std::accumulate(numbers.begin(), numbers.end(), 0.0);
      ws.Set(outputs_.at(0), sum / numbers.size());
    }
  };

  REGISTER_NODE(AverageNode);

  class DummyProcessNode : public kpipeline::Node {
  public:
    explicit DummyProcessNode(const Json::Value& config) : Node(config) {
      message_ = config.get("params", Json::Value(Json::objectValue))
                      .get("message", "Default message")
                      .asString();
    }
    void Execute(kpipeline::Workspace& ws) const override {
      // ======================== 修复开始 ========================
      // 我们不需要读取输入值来保证执行顺序，调度器已经保证了。
      // 所以我们可以直接开始模拟工作。
      std::this_thread::sleep_for(std::chrono::milliseconds(100));

      // 正确的报告内容
      std::string report = "Report from node '" + name_ + "': " + message_;
      ws.Set(outputs_.at(0), report);
      // ======================== 修复结束 ========================
    }
  private:
    std::string message_;
  };
  REGISTER_NODE(DummyProcessNode);

  class FinalReportNode : public kpipeline::Node
  {
  public:
    explicit FinalReportNode(const Json::Value& config) : Node(config)
    {
    }

    void Execute(kpipeline::Workspace& ws) const override
    {
      auto report1 = ws.Get<std::string>(inputs_.at(0));
      auto report2 = ws.Get<std::string>(inputs_.at(1));
      std::string final_report = "--- Final Consolidated Report ---\n"
        "1. " + report1 + "\n"
        "2. " + report2 + "\n";
      ws.Set(outputs_.at(0), final_report);
    }
  };

  REGISTER_NODE(FinalReportNode);
} // namespace my_nodes

int main(int argc, char* argv[])
{
  if (argc != 2)
  {
    std::cerr << "Usage: " << argv[0] << " <path_to_pipeline_config.json>" << std::endl;
    return 1;
  }

  try
  {
    auto graph = kpipeline::GraphBuilder::FromFile(argv[1]);

    kpipeline::Workspace ws;
    ws.Set("initial_input", std::vector<int>{10, 20, 30, 40, 50});

    graph->Run(ws);

    const auto& final_report = ws.Get<std::string>("final_report");
    std::cout << "\nFinal Output from Workspace:\n" << final_report << std::endl;
  }
  catch (const kpipeline::PipelineException& e)
  {
    std::cerr << "A pipeline error occurred: " << e.what() << std::endl;
    return 1;
  } catch (const std::exception& e)
  {
    std::cerr << "An unexpected error occurred: " << e.what() << std::endl;
    return 1;
  }

  return 0;
}
