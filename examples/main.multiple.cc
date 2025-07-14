#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <memory>
#include <sstream>

#include "graph.h"
#include "node.h"
#include "workspace.h"

// --- 定义数据结构 ---
struct SourceData
{
  int id;
  std::string content;
};

// --- 为这个例子定义不同的节点类型 ---
namespace multi_task_nodes
{
  // 1. 数据提供节点
  class DataProviderNode : public kpipeline::Node
  {
  public:
    DataProviderNode()
      : Node("DataProvider", {"source_id"}, {"source_data"})
    {
    }

    void Execute(kpipeline::Workspace& ws) const override
    {
      int id = ws.Get<int>(inputs_.at(0));
      std::cout << "    > Providing data for ID: " << id << "..." << std::endl;
      SourceData data{id, "This is the main content for the task."};
      ws.Set(outputs_.at(0), data);
    }
  };

  // 2. 并行任务 A: 内容分析 (耗时较长)
  class AnalyzeContentNode : public kpipeline::Node
  {
  public:
    AnalyzeContentNode()
      : Node("ContentAnalyzer", {"source_data"}, {"analysis_result"})
    {
    }

    void Execute(kpipeline::Workspace& ws) const override
    {
      auto data = ws.Get<SourceData>(inputs_.at(0));
      std::cout << "    > [Task A] Analyzing content... (long task)" << std::endl;
      std::this_thread::sleep_for(std::chrono::milliseconds(200));
      std::string result = "Content analysis complete: " +
        std::to_string(data.content.length()) + " chars found.";
      ws.Set(outputs_.at(0), result);
    }
  };

  // 3. 并行任务 B: 生成预览 (耗时中等)
  class GeneratePreviewNode : public kpipeline::Node
  {
  public:
    GeneratePreviewNode()
      : Node("PreviewGenerator", {"source_data"}, {"preview_result"})
    {
    }

    void Execute(kpipeline::Workspace& ws) const override
    {
      auto data = ws.Get<SourceData>(inputs_.at(0));
      std::cout << "    > [Task B] Generating preview... (medium task)" << std::endl;
      std::this_thread::sleep_for(std::chrono::milliseconds(120));
      std::string result = "Preview generated: '" + data.content.substr(0, 10) + "...'";
      ws.Set(outputs_.at(0), result);
    }
  };

  // 4. 并行任务 C: 归档数据 (耗时较短)
  class ArchiveDataNode : public kpipeline::Node
  {
  public:
    ArchiveDataNode()
      : Node("DataArchiver", {"source_data"}, {"archive_status"})
    {
    }

    void Execute(kpipeline::Workspace& ws) const override
    {
      auto data = ws.Get<SourceData>(inputs_.at(0));
      std::cout << "    > [Task C] Archiving data... (short task)" << std::endl;
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
      std::string result = "Data for ID " + std::to_string(data.id) + " archived.";
      ws.Set(outputs_.at(0), result);
    }
  };

  // 5. 扇入节点: 聚合报告
  class ReportAggregatorNode : public kpipeline::Node
  {
  public:
    ReportAggregatorNode()
      : Node("ReportAggregator",
             {"analysis_result", "preview_result", "archive_status"},
             {"final_report"})
    {
    }

    void Execute(kpipeline::Workspace& ws) const override
    {
      std::cout << "    > Aggregating results from all parallel tasks..." << std::endl;
      auto analysis_res = ws.Get<std::string>(inputs_.at(0));
      auto preview_res = ws.Get<std::string>(inputs_.at(1));
      auto archive_res = ws.Get<std::string>(inputs_.at(2));

      std::stringstream report;
      report << "--- Final Processing Report ---\n";
      report << "  - Analysis: " << analysis_res << "\n";
      report << "  - Preview:  " << preview_res << "\n";
      report << "  - Archive:  " << archive_res << "\n";
      ws.Set(outputs_.at(0), report.str());
    }
  };
} // namespace multi_task_nodes

int main()
{
  kpipeline::Logger::Get().SetLevel(kpipeline::LogLevel::DEBUG);
  using namespace multi_task_nodes;

  std::cout << "--- Running Multi-Task Parallel Processing Example ---\n";

  // 在代码中构建图
  kpipeline::Graph graph;

  // 1. 添加数据源节点
  graph.AddNode(std::make_shared<DataProviderNode>());

  // 2. 添加所有并行的处理节点 (扇出)
  graph.AddNode(std::make_shared<AnalyzeContentNode>());
  graph.AddNode(std::make_shared<GeneratePreviewNode>());
  graph.AddNode(std::make_shared<ArchiveDataNode>());

  // 3. 添加聚合节点 (扇入)
  graph.AddNode(std::make_shared<ReportAggregatorNode>());

  try
  {
    kpipeline::Workspace ws;
    ws.Set("source_id", 42);

    // 使用足够的线程来并行化所有任务
    graph.Run(ws, 4, true);

    const auto& final_report = ws.Get<std::string>("final_report");
    std::cout << "\n" << final_report << std::endl;
  }
  catch (const std::exception& e)
  {
    std::cerr << "An error occurred: " << e.what() << std::endl;
    return 1;
  }

  return 0;
}
