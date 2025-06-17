#ifndef KPIPELINE_GRAPH_BUILDER_H_
#define KPIPELINE_GRAPH_BUILDER_H_

#include <fstream>
#include <memory>
#include <string>
#include <json/json.h>
#include "graph.h"
#include "node_factory.h"

namespace kpipeline {

  class GraphBuilder {
  public:
    static std::unique_ptr<Graph> FromFile(const std::string& config_path) {
      std::ifstream config_file(config_path);
      if (!config_file.is_open()) {
        throw PipelineException("GraphBuilder error: Cannot open file '" + config_path + "'.");
      }

      Json::Value root;
      Json::CharReaderBuilder reader_builder;
      std::string errs;

      if (!Json::parseFromStream(reader_builder, config_file, &root, &errs)) {
        throw PipelineException("GraphBuilder error: Failed to parse JSON config. " + errs);
      }

      auto graph = std::make_unique<Graph>();

      std::cout << "Building graph '" << root.get("name", "Untitled").asString()
                << "' from config..." << std::endl;

      const Json::Value& nodes_config = root["nodes"];
      if (!nodes_config.isArray()) {
        throw PipelineException("GraphBuilder error: 'nodes' field is missing or not an array.");
      }

      for (const auto& node_config : nodes_config) {
        auto node = NodeFactory::Instance().Create(node_config);
        graph->AddNode(node);
      }

      std::cout << "Graph built successfully." << std::endl;
      return graph;
    }
  };

} // namespace kpipeline

#endif // KPIPELINE_GRAPH_BUILDER_H_