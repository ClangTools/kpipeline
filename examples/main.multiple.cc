#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <memory>
#include <sstream>
#include <random>

#include "graph.h"
#include "node.h"
#include "workspace.h"

// --- 为这个例子定义节点 ---
namespace fan_out_in_nodes
{
  // 1. 扇出节点 (Splitter)
  class SplitBatchNode : public kpipeline::Node
  {
  public:
    // 调用新的、无JSON的基类构造函数
    SplitBatchNode()
      : Node("Splitter", {"initial_batch"}, {"split_complete_signal"})
    {
    }

    void Execute(kpipeline::Workspace& ws) const override
    {
      auto batch = ws.Get<std::vector<int>>(inputs_.at(0));
      std::cout << "    > Splitting batch of " << batch.size() << " items..." << std::endl;
      for (int item_id : batch)
      {
        ws.Set("task_" + std::to_string(item_id), item_id);
      }
      ws.Set(outputs_.at(0), kpipeline::ControlSignal{});
    }
  };

  // 2. 并行处理节点 (Processor)
  class ProcessItemNode : public kpipeline::Node
  {
  public:
    // 调用新的、无JSON的基类构造函数
    ProcessItemNode(int task_id)
      : Node("Processor_" + std::to_string(task_id),
             {"task_" + std::to_string(task_id)},
             {"result_" + std::to_string(task_id)},
             {"split_complete_signal"})
    {
    }

    void Execute(kpipeline::Workspace& ws) const override
    {
      int item_id = ws.Get<int>(inputs_.at(0));

      std::random_device rd;
      std::mt19937 gen(rd());
      std::uniform_int_distribution<> distrib(50, 200);
      int sleep_ms = distrib(gen);

      std::cout << "    > Processing item " << item_id << " (will take " << sleep_ms << "ms)..." << std::endl;
      std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));

      std::string result = "Item " + std::to_string(item_id) + " processed successfully.";
      ws.Set(outputs_.at(0), result);
    }
  };

  // 3. 扇入节点 (Aggregator)
  class AggregateResultsNode : public kpipeline::Node
  {
  public:
    // 调用新的、无JSON的基类构造函数
    AggregateResultsNode(const std::vector<std::string>& result_names)
      : Node("Aggregator", result_names, {"final_summary"})
    {
    }

    void Execute(kpipeline::Workspace& ws) const override
    {
      std::cout << "    > Aggregating all results..." << std::endl;
      std::stringstream summary;
      summary << "--- Aggregation Summary ---\n";
      for (const auto& input_name : inputs_)
      {
        if (ws.Has(input_name))
        {
          summary << " - " << ws.Get<std::string>(input_name) << "\n";
        }
      }
      ws.Set(outputs_.at(0), summary.str());
    }
  };
} // namespace fan_out_in_nodes

int main()
{
  using namespace fan_out_in_nodes;

  std::cout << "--- Running Fan-out/Fan-in Example (Pure Code-defined Graph, No JSON) ---\n";

  std::vector<int> task_ids = {101, 102, 103, 104, 105};

  kpipeline::Graph graph;

  graph.AddNode(std::make_shared<SplitBatchNode>());

  std::vector<std::string> result_names;
  for (int id : task_ids)
  {
    graph.AddNode(std::make_shared<ProcessItemNode>(id));
    result_names.push_back("result_" + std::to_string(id));
  }

  graph.AddNode(std::make_shared<AggregateResultsNode>(result_names));

  try
  {
    kpipeline::Workspace ws;
    ws.Set("initial_batch", task_ids);

    graph.Run(ws, 4, true);

    const auto& final_summary = ws.Get<std::string>("final_summary");
    std::cout << "\n" << final_summary << std::endl;
  }
  catch (const std::exception& e)
  {
    std::cerr << "An error occurred: " << e.what() << std::endl;
    return 1;
  }

  return 0;
}
