#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <chrono>
#include <thread>
#include <memory>
#include <json/json.h>

#include "graph_builder.h"
#include "node_factory.h"

// --- 定义数据结构 ---
// 模拟用户档案
struct UserProfile
{
  int user_id;
  std::string user_name;
  std::vector<std::string> photo_paths;
};

// --- 实现所有节点 ---
namespace user_analysis_nodes
{
  // --- 父图节点 ---
  class LoadUserProfileNode : public kpipeline::Node
  {
  public:
    explicit LoadUserProfileNode(const Json::Value& config) : Node(kpipeline::NodeFactory::Build(config))
    {
    }

    void Execute(kpipeline::Workspace& ws) const override
    {
      int id = ws.Get<int>(inputs_.at(0));
      std::cout << "    > Loading profile for user_id: " << id << std::endl;
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
      UserProfile profile{id, "TestUser", {"/photos/pic1.jpg", "/photos/pic2.jpg", "/photos/pic3.jpg"}};
      ws.Set(outputs_.at(0), profile);
    }
  };

  REGISTER_NODE(LoadUserProfileNode);

  class GenerateFinalUserReportNode : public kpipeline::Node
  {
  public:
    explicit GenerateFinalUserReportNode(const Json::Value& config) : Node(kpipeline::NodeFactory::Build(config))
    {
    }

    void Execute(kpipeline::Workspace& ws) const override
    {
      auto profile = ws.Get<UserProfile>(inputs_.at(0));
      auto photo_report = ws.Get<Json::Value>(inputs_.at(1));

      Json::Value final_report;
      final_report["user_name"] = profile.user_name;
      final_report["photo_analysis"] = photo_report;
      final_report["report_status"] = "COMPLETE";
      ws.Set(outputs_.at(0), Json::writeString(Json::StreamWriterBuilder(), final_report));
    }
  };

  REGISTER_NODE(GenerateFinalUserReportNode);


  // --- 子图节点 ---
  class ExtractPhotoPathsNode : public kpipeline::Node
  {
  public:
    explicit ExtractPhotoPathsNode(const Json::Value& config) : Node(kpipeline::NodeFactory::Build(config))
    {
    }

    void Execute(kpipeline::Workspace& ws) const override
    {
      auto profile = ws.Get<UserProfile>(inputs_.at(0));
      ws.Set(outputs_.at(0), profile.photo_paths);
    }
  };

  REGISTER_NODE(ExtractPhotoPathsNode);

  class AnalyzeColorsNode : public kpipeline::Node
  {
  public:
    explicit AnalyzeColorsNode(const Json::Value& config) : Node(kpipeline::NodeFactory::Build(config))
    {
    }

    void Execute(kpipeline::Workspace& ws) const override
    {
      auto photos = ws.Get<std::vector<std::string>>(inputs_.at(0));
      std::cout << "    > [SubGraph] Analyzing colors for " << photos.size() << " photos..." << std::endl;
      std::this_thread::sleep_for(std::chrono::milliseconds(200));
      Json::Value stats;
      stats["dominant_color"] = "blue";
      stats["saturation"] = 0.75;
      ws.Set(outputs_.at(0), stats);
    }
  };

  REGISTER_NODE(AnalyzeColorsNode);

  class CountObjectsNode : public kpipeline::Node
  {
  public:
    explicit CountObjectsNode(const Json::Value& config) : Node(kpipeline::NodeFactory::Build(config))
    {
    }

    void Execute(kpipeline::Workspace& ws) const override
    {
      auto photos = ws.Get<std::vector<std::string>>(inputs_.at(0));
      std::cout << "    > [SubGraph] Counting objects for " << photos.size() << " photos..." << std::endl;
      std::this_thread::sleep_for(std::chrono::milliseconds(150));
      Json::Value stats;
      stats["cats"] = 2;
      stats["dogs"] = 1;
      ws.Set(outputs_.at(0), stats);
    }
  };

  REGISTER_NODE(CountObjectsNode);

  class CompilePhotoReportNode : public kpipeline::Node
  {
  public:
    explicit CompilePhotoReportNode(const Json::Value& config) : Node(kpipeline::NodeFactory::Build(config))
    {
    }

    void Execute(kpipeline::Workspace& ws) const override
    {
      auto color_stats = ws.Get<Json::Value>(inputs_.at(0));
      auto object_stats = ws.Get<Json::Value>(inputs_.at(1));
      Json::Value report;
      report["color_analysis"] = color_stats;
      report["object_detection"] = object_stats;
      ws.Set(outputs_.at(0), report);
    }
  };

  REGISTER_NODE(CompilePhotoReportNode);


  // --- 核心：SubGraphNode 的实现 ---
  class SubGraphNode : public kpipeline::Node
  {
  public:
    explicit SubGraphNode(const Json::Value& config) : Node(kpipeline::NodeFactory::Build(config))
    {
      subgraph_config_path_ = config["params"]["config_path"].asString();
      // 可以在这里获取其他参数，比如线程数
      num_threads_ = config["params"].get("num_threads", 0).asInt();
      if (num_threads_ == 0)
      {
        num_threads_ = std::thread::hardware_concurrency();
      }
    }

    void Execute(kpipeline::Workspace& parent_ws) const override
    {
      std::cout << "    > Entering SubGraph from config: " << subgraph_config_path_ << std::endl;

      // 1. 创建子图的隔离工作空间
      kpipeline::Workspace sub_ws;

      // 2. 将父图的输入映射到子图的工作空间
      //    使用新的 GetAny 方法来正确复制数据
      for (const auto& input_name : inputs_)
      {
        sub_ws.Set(input_name, parent_ws.GetAny(input_name));
      }

      // 3. 构建并运行子图
      auto subgraph = kpipeline::GraphBuilder::FromFile(subgraph_config_path_);

      // ======================== 修复开始 ========================
      // Graph::Run 是一个阻塞调用，它会等待子图完全结束后才返回。
      // 我们需要确保这一点是正确的。
      // 之前的实现就是阻塞的，所以这里调用方式不变，但我们要理解其行为。
      subgraph->Run(sub_ws, num_threads_, false); // 子图内部不进行性能分析，以避免报告嵌套
      // ======================== 修复结束 ========================

      // 4. 将子图的输出映射回父图的工作空间
      for (const auto& output_name : outputs_)
      {
        // 同样使用 GetAny
        parent_ws.Set(output_name, sub_ws.GetAny(output_name));
      }

      std::cout << "    > Exiting SubGraph." << std::endl;
    }

  private:
    std::string subgraph_config_path_;
    int num_threads_;
  };

  REGISTER_NODE(SubGraphNode);
} // namespace user_analysis_nodes


int main(int argc, char* argv[])
{
  std::cout << "--- Running User Profile Analysis Pipeline ---" << std::endl;
  try
  {
    auto graph = kpipeline::GraphBuilder::FromFile("examples/main_pipeline.json");

    kpipeline::Workspace ws;
    ws.Set("user_id", 12345);

    graph->Run(ws, 4, true); // 启用性能分析

    const auto& final_report = ws.Get<std::string>("final_user_report");
    std::cout << "\n--- Final User Report ---\n" << final_report << std::endl;
  }
  catch (const std::exception& e)
  {
    std::cerr << "An error occurred: " << e.what() << std::endl;
    return 1;
  }
  return 0;
}
