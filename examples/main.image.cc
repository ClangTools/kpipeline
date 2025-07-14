#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <memory>
#include <json/json.h>

#include "kpipeline/graph_builder.h"
#include "kpipeline/node_factory.h"

// --- 定义数据结构 ---

// 模拟一个加载到内存中的图片对象
struct ImageData
{
  std::string original_path;
  int width;
  int height;
  std::string format;
};

// --- 定义新的节点实现 ---

namespace image_processing_nodes
{
  // 1. 加载图片节点
  class LoadImageNode : public kpipeline::Node
  {
  public:
    explicit LoadImageNode(const Json::Value& config) : Node(kpipeline::NodeFactory::Build(config))
    {
    }

    void Execute(kpipeline::Workspace& ws) const override
    {
      auto path = ws.Get<std::string>(inputs_.at(0));
      std::cout << "    > Loading image from: " << path << std::endl;
      std::this_thread::sleep_for(std::chrono::milliseconds(50)); // 模拟IO延迟

      // 模拟图片数据
      ImageData data{path, 1920, 1080, "JPEG"};
      ws.Set(outputs_.at(0), data);
    }
  };

  // REGISTER_NODE(LoadImageNode);

  namespace
  {
    std::shared_ptr<kpipeline::Node> Creator_LoadImageNode(const Json::Value& config)
    {
      return std::make_shared<LoadImageNode>(config);
    }

    const bool registered_LoadImageNode =
      kpipeline::NodeFactory::Instance().Register("LoadImageNode", Creator_LoadImageNode);
  }

  // 2. 调整图片尺寸的通用节点
  class ResizeImageNode : public kpipeline::Node
  {
  public:
    explicit ResizeImageNode(const Json::Value& config) : Node(kpipeline::NodeFactory::Build(config))
    {
      // 从配置中读取本节点特有的参数
      const Json::Value& params = config["params"];
      target_width_ = params["width"].asInt();
      target_height_ = params["height"].asInt();
      suffix_ = params["suffix"].asString();
    }

    void Execute(kpipeline::Workspace& ws) const override
    {
      auto image_data = ws.Get<ImageData>(inputs_.at(0));
      std::cout << "    > Resizing '" << image_data.original_path
        << "' to " << target_width_ << "x" << target_height_
        << " for '" << suffix_ << "'" << std::endl;
      std::this_thread::sleep_for(std::chrono::milliseconds(150)); // 模拟CPU密集型工作

      // 生成新的路径
      std::string new_path = image_data.original_path + "_" + suffix_ + ".jpg";
      ws.Set(outputs_.at(0), new_path);
    }

  private:
    int target_width_;
    int target_height_;
    std::string suffix_;
  };

  REGISTER_NODE(ResizeImageNode);


  // 3. 提取元数据节点
  class ExtractMetadataNode : public kpipeline::Node
  {
  public:
    explicit ExtractMetadataNode(const Json::Value& config) : Node(kpipeline::NodeFactory::Build(config))
    {
    }

    void Execute(kpipeline::Workspace& ws) const override
    {
      auto image_data = ws.Get<ImageData>(inputs_.at(0));
      std::cout << "    > Extracting metadata from '" << image_data.original_path << "'" << std::endl;
      std::this_thread::sleep_for(std::chrono::milliseconds(80));

      // 创建一个 JSON 对象作为元数据
      Json::Value metadata;
      metadata["format"] = image_data.format;
      metadata["original_width"] = image_data.width;
      metadata["original_height"] = image_data.height;
      metadata["processed_by"] = "PipelineEngineV2";

      ws.Set(outputs_.at(0), metadata);
    }
  };

  REGISTER_NODE(ExtractMetadataNode);


  // 4. 生成最终报告的节点
  class GenerateReportNode : public kpipeline::Node
  {
  public:
    explicit GenerateReportNode(const Json::Value& config) : Node(kpipeline::NodeFactory::Build(config))
    {
    }

    void Execute(kpipeline::Workspace& ws) const override
    {
      // 从工作空间获取所有处理结果
      auto thumb_path = ws.Get<std::string>(inputs_.at(0));
      auto web_path = ws.Get<std::string>(inputs_.at(1));
      auto metadata = ws.Get<Json::Value>(inputs_.at(2)); // 直接获取 Json::Value

      std::cout << "    > Generating final report..." << std::endl;

      Json::Value final_report;
      final_report["thumbnail"] = thumb_path;
      final_report["web_version"] = web_path;
      final_report["metadata"] = metadata;

      // 将 JSON 对象转换为字符串
      Json::StreamWriterBuilder writer;
      std::string report_str = Json::writeString(writer, final_report);

      ws.Set(outputs_.at(0), report_str);
    }
  };

  REGISTER_NODE(GenerateReportNode);
} // namespace image_processing_nodes

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
    // 设置初始输入：图片路径
    ws.Set("image_path", std::string("/path/to/my/awesome_photo.jpg"));

    // 使用4个线程运行图
    graph->Run(ws, 4);

    const auto& final_report = ws.Get<std::string>("final_json_report");
    std::cout << "\nFinal JSON Report from Workspace:\n" << final_report << std::endl;
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
