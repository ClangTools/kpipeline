#ifndef KPIPELINE_GRAPH_H_
#define KPIPELINE_GRAPH_H_

#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <map>
#include <queue>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <set>

#include "logger.h"
#include "node.h"
#include "workspace.h"
#include "thread_pool.h"
#include "profiler.h"

namespace kpipeline
{
  class Graph
  {
  public:
    void AddNode(std::shared_ptr<Node> node)
    {
      if (!node) return;
      nodes_[node->GetName()] = node;
    }

    // 新增 enable_profiling 参数
    void Run(Workspace& ws, size_t num_threads = std::thread::hardware_concurrency(), bool enable_profiling = false)
    {
      if (nodes_.empty()) return;

      // 创建 Profiler 实例
      Profiler profiler;

      auto [adj, in_degree] = BuildDependencies();

      ThreadPool pool(num_threads);
      std::atomic<size_t> finished_nodes_count(0);
      size_t total_graph_nodes = nodes_.size();

      std::mutex completion_mutex;
      std::condition_variable cv_completion;

      std::function<void(const std::string&)> schedule_next;
      std::function<void(const std::string&)> prune_branch;

      schedule_next = [this, &ws, &adj, &in_degree, &pool, &finished_nodes_count, total_graph_nodes, &cv_completion, &
          schedule_next, &prune_branch, &profiler, enable_profiling](const std::string& completed_node_name)
        {
          if (adj.count(completed_node_name))
          {
            for (const auto& successor_name : adj.at(completed_node_name))
            {
              if (--in_degree.at(successor_name) == 0)
              {
                bool can_run = true;
                const auto& successor_node = nodes_.at(successor_name);
                for (const auto& control_input : successor_node->GetControlInputs())
                {
                  if (!ws.Has(control_input))
                  {
                    can_run = false;
                    break;
                  }
                }

                if (can_run)
                {
                  pool.Enqueue([this, &ws, &schedule_next, &profiler, enable_profiling, successor_name]
                  {
                    LOG_INFO("[Thread {}] Executing Node: {}", std::this_thread::get_id(), successor_name);
                    auto start_time = std::chrono::high_resolution_clock::now();
                    try { nodes_.at(successor_name)->Execute(ws); }
                    catch (const std::exception& e)
                    {
                      LOG_ERROR("Exception in node '{}': {}", successor_name, e.what());
                    }
                    if (enable_profiling) profiler.End(successor_name, start_time);
                    schedule_next(successor_name);
                  });
                }
                else
                {
                  prune_branch(successor_name);
                }
              }
            }
          }

          if (finished_nodes_count.fetch_add(1) + 1 == total_graph_nodes)
          {
            cv_completion.notify_one();
          }
        };

      prune_branch = [&adj, &in_degree, &finished_nodes_count, total_graph_nodes, &cv_completion, &prune_branch](
        const std::string& pruned_node_name)
        {
          if (adj.count(pruned_node_name))
          {
            for (const auto& successor_name : adj.at(pruned_node_name))
            {
              if (--in_degree.at(successor_name) == 0) { prune_branch(successor_name); }
            }
          }
          if (finished_nodes_count.fetch_add(1) + 1 == total_graph_nodes)
          {
            cv_completion.notify_one();
          }
        };

      LOG_INFO("--- Starting Graph Execution with {} threads ---", num_threads);
      for (const auto& [name, node] : nodes_)
      {
        if (in_degree.at(name) == 0)
        {
          pool.Enqueue([this, &ws, &schedule_next, &profiler, enable_profiling, name]
          {
            LOG_INFO("[Thread {}] Executing Node: {}", std::this_thread::get_id(), name);
            auto start_time = std::chrono::high_resolution_clock::now();
            try { nodes_.at(name)->Execute(ws); }
            catch (const std::exception& e)
            {
              LOG_ERROR("Exception in node '{}': {}", name, e.what());
            }
            if (enable_profiling) profiler.End(name, start_time);
            schedule_next(name);
          });
        }
      }

      std::unique_lock<std::mutex> lock(completion_mutex);
      cv_completion.wait(lock, [&] { return finished_nodes_count.load() == total_graph_nodes; });

      LOG_INFO("--- Graph Execution Finished ---");

      // 在图执行完毕后打印报告
      if (enable_profiling)
      {
        profiler.PrintReport();
      }
    }

  private:
    std::pair<std::map<std::string, std::vector<std::string>>,
              std::map<std::string, std::atomic<int>>> BuildDependencies()
    {
      std::map<std::string, std::vector<std::string>> adj;
      std::map<std::string, std::atomic<int>> in_degree;
      std::map<std::string, std::string> data_producer;

      for (const auto& [name, node] : nodes_)
      {
        in_degree[name] = 0;
        for (const auto& output : node->GetOutputs())
        {
          if (data_producer.count(output))
          {
            throw PipelineException("Data '" + output + "' produced by multiple nodes.");
          }
          data_producer[output] = name;
        }
      }

      for (const auto& [name, node] : nodes_)
      {
        auto process_deps = [&](const std::vector<std::string>& inputs)
        {
          for (const auto& input : inputs)
          {
            if (data_producer.count(input))
            {
              const auto& producer_name = data_producer.at(input);
              adj[producer_name].push_back(name);
              in_degree.at(name)++;
            }
          }
        };
        process_deps(node->GetInputs());
        process_deps(node->GetControlInputs());
      }

      bool has_start_node = false;
      if (!nodes_.empty())
      {
        for (const auto& pair : in_degree)
        {
          if (pair.second == 0)
          {
            has_start_node = true;
            break;
          }
        }
      }
      else
      {
        has_start_node = true;
      }

      if (!has_start_node)
      {
        throw PipelineException("Graph error: A cycle was detected, or there are no entry-point nodes.");
      }

      return {adj, std::move(in_degree)};
    }

    std::map<std::string, std::shared_ptr<Node>> nodes_;
  };
} // namespace kpipeline
#endif // KPIPELINE_GRAPH_H_
