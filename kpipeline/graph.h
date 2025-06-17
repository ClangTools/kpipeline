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
#include "node.h"
#include "workspace.h"
#include "thread_pool.h"

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

    void Run(Workspace& ws, size_t num_threads = std::thread::hardware_concurrency())
    {
      if (nodes_.empty()) return;

      auto [adj, in_degree] = BuildDependencies();

      ThreadPool pool(num_threads);
      std::atomic<size_t> completed_nodes_count(0);
      size_t total_nodes = nodes_.size();
      std::mutex completion_mutex;
      std::condition_variable cv_completion;

      std::function<void(const std::string&)> schedule_next;

      schedule_next = [this, &ws, &adj, &in_degree, &pool, &completed_nodes_count, total_nodes, &completion_mutex, &
          cv_completion, &schedule_next](const std::string& completed_node_name)
        {
          // 调度后继节点
          if (adj.count(completed_node_name))
          {
            for (const auto& successor_name : adj.at(completed_node_name))
            {
              if (--in_degree.at(successor_name) == 0)
              {
                pool.Enqueue([this, &ws, &schedule_next, successor_name]
                {
                  std::cout << "[Thread " << std::this_thread::get_id() << "] Executing Node: " << successor_name <<
                    std::endl;
                  try
                  {
                    nodes_.at(successor_name)->Execute(ws);
                  }
                  catch (const std::exception& e)
                  {
                    // 在线程中捕获异常，否则会直接导致程序终止
                    std::cerr << "Exception in node '" << successor_name << "': " << e.what() << std::endl;
                    // 可以考虑在这里设置一个全局的失败状态
                  }
                  schedule_next(successor_name);
                });
              }
            }
          }

          // ======================== 修复开始 ========================
          // 将计数器增加，并检查是否是最后一个完成的节点
          size_t count = completed_nodes_count.fetch_add(1) + 1;

          if (count == total_nodes)
          {
            // 当最后一个节点完成时，我们才需要通知主线程。
            // 为了安全，在通知之前获取锁。
            // 这是一个好习惯，虽然在这个特定场景下可能不是必须的，
            // 但能避免一些复杂的边缘情况。
            {
              std::lock_guard<std::mutex> lock(completion_mutex);
            }
            cv_completion.notify_one();
          }
          // ======================== 修复结束 ========================
        };

      std::cout << "--- Starting Parallel Graph Execution with " << num_threads << " threads ---" << std::endl;
      for (const auto& pair : in_degree)
      {
        if (pair.second == 0)
        {
          pool.Enqueue([this, &ws, &schedule_next, node_name = pair.first]
          {
            std::cout << "[Thread " << std::this_thread::get_id() << "] Executing Node: " << node_name << std::endl;
            try
            {
              nodes_.at(node_name)->Execute(ws);
            }
            catch (const std::exception& e)
            {
              std::cerr << "Exception in node '" << node_name << "': " << e.what() << std::endl;
            }
            schedule_next(node_name);
          });
        }
      }

      std::unique_lock<std::mutex> lock(completion_mutex);
      // wait的lambda现在只检查最终条件，不再需要依赖锁来保护计数器
      cv_completion.wait(lock, [&] { return completed_nodes_count == total_nodes; });

      std::cout << "--- Graph Execution Finished ---" << std::endl;
    }

  private:
    // BuildDependencies 函数保持不变...
    std::pair<std::map<std::string, std::vector<std::string>>,
              std::map<std::string, std::atomic<int>>>
    BuildDependencies()
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
        for (const auto& input : node->GetInputs())
        {
          if (data_producer.count(input))
          {
            const auto& producer_name = data_producer.at(input);
            adj[producer_name].push_back(name);
            in_degree.at(name)++;
          }
        }
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
        throw PipelineException("Graph error: Cycle detected or no entry-point nodes.");
      }

      return {adj, std::move(in_degree)};
    }

    std::map<std::string, std::shared_ptr<Node>> nodes_;
  };
} // namespace kpipeline
#endif // KPIPELINE_GRAPH_H_
