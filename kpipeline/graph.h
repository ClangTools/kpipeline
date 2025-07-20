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
#include <exception>
#include <algorithm> // For std::max
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

    void Run(Workspace& ws, size_t num_threads = std::thread::hardware_concurrency(), bool enable_profiling = false)
    {
      if (nodes_.empty()) return;

      graph_failed_ = false;
      first_exception_ = nullptr;
      Profiler profiler;

      auto [adj, in_degree] = BuildDependencies();

      ThreadPool pool(std::max(num_threads, static_cast<size_t>(1)));
      std::atomic<size_t> finished_nodes_count(0);
      size_t total_graph_nodes = nodes_.size();

      std::mutex completion_mutex;
      std::condition_variable cv_completion;

      // ======================== 统一调度逻辑 ========================

      // 使用 shared_ptr 来管理 lambda 的生命周期，防止悬空引用
      auto on_node_finished_ptr = std::make_shared<std::function<void(const std::string&)>>();
      auto task_wrapper_ptr = std::make_shared<std::function<void(const std::string&,
                                                                  const std::function<void(const std::string&)>&)>>();

      // 定义任务执行的核心逻辑
      *task_wrapper_ptr = [this, &ws, &profiler, enable_profiling, &cv_completion]
      (const std::string& node_name, const std::function<void(const std::string&)>& on_finish)
        {
          if (graph_failed_.load())
          {
            on_finish(node_name);
            return;
          }

          try
          {
            LOG_INFO("[Thread {}] Executing Node: {}", thread_id_to_string(std::this_thread::get_id()), node_name);
            auto start_time = std::chrono::high_resolution_clock::now();
            nodes_.at(node_name)->Execute(ws);
            if (enable_profiling) profiler.End(node_name, start_time);
          }
          catch (const std::exception& e)
          {
            std::lock_guard<std::mutex> lock(exception_mutex_);
            if (!first_exception_)
            {
              first_exception_ = std::current_exception();
              graph_failed_ = true;
              LOG_INFO("!!! Graph execution failed in node '{}'. Halting all operations. Error:{}", node_name,
                       e.what());
              // 失败时也需要唤醒主线程
              cv_completion.notify_one();
            }
          }
          on_finish(node_name);
        };

      // 定义完成处理函数
      *on_node_finished_ptr =
        [this, &ws, &adj, &in_degree, &pool, &finished_nodes_count, total_graph_nodes, &cv_completion, task_wrapper_ptr,
          on_node_finished_ptr]
      (const std::string& finished_node_name)
        {
          if (adj.count(finished_node_name))
          {
            for (const auto& successor_name : adj.at(finished_node_name))
            {
              if (--in_degree.at(successor_name) == 0)
              {
                bool can_run = true;
                if (graph_failed_.load())
                {
                  can_run = false;
                }
                else
                {
                  const auto& successor_node = nodes_.at(successor_name);
                  for (const auto& control_input : successor_node->GetControlInputs())
                  {
                    if (!ws.Has(control_input))
                    {
                      can_run = false;
                      break;
                    }
                  }
                }

                // 捕获 shared_ptr 的拷贝，而不是引用
                pool.Enqueue([this, can_run, successor_name, task_wrapper_ptr, on_node_finished_ptr]()
                {
                  if (can_run)
                  {
                    (*task_wrapper_ptr)(successor_name, *on_node_finished_ptr);
                  }
                  else
                  {
                    LOG_INFO("    > Pruning branch at node: {}", successor_name);
                    (*on_node_finished_ptr)(successor_name);
                  }
                });
              }
            }
          }

          if (finished_nodes_count.fetch_add(1) + 1 == total_graph_nodes)
          {
            cv_completion.notify_one();
          }
        };

      // ======================== 结束 ========================

      LOG_INFO("--- Starting Graph Execution with {} threads ---", num_threads);
      bool has_started = false;
      for (const auto& [name, node] : nodes_)
      {
        if (in_degree.at(name) == 0)
        {
          has_started = true;
          // 启动初始节点
          pool.Enqueue([task_wrapper_ptr, name, on_node_finished_ptr]
          {
            (*task_wrapper_ptr)(name, *on_node_finished_ptr);
          });
        }
      }

      if (!has_started && total_graph_nodes > 0)
      {
        throw PipelineException("Graph has no entry points, but is not empty.");
      }

      std::unique_lock<std::mutex> lock(completion_mutex);
      cv_completion.wait(lock, [&]
      {
        return finished_nodes_count.load() >= total_graph_nodes || graph_failed_.load();
      });

      if (graph_failed_.load())
      {
        LOG_INFO("--- Graph Execution Halted Due to Error ---");
      }
      else
      {
        LOG_INFO("--- Graph Execution Finished Successfully ---");
      }

      if (enable_profiling)
      {
        profiler.PrintReport();
      }

      if (first_exception_)
      {
        std::rethrow_exception(first_exception_);
      }
    }


    void Print()
    {
      std::stringstream ss;
      ss << "\n--- Graph Structure ---\n";

      if (nodes_.empty())
      {
        ss << "(Empty Graph)\n";
      }
      else
      {
        auto [adj, in_degree] = BuildDependencies();
        std::vector<std::string> entry_points;
        for (const auto& [name, node] : nodes_)
        {
          if (in_degree.at(name) == 0)
          {
            entry_points.push_back(name);
          }
        }

        std::set<std::string> visited_nodes;
        for (const auto& entry_point : entry_points)
        {
          PrintNodeTree(ss, entry_point, "", true, visited_nodes, adj);
        }
      }
      ss << "-----------------------\n";
      // 一次性打印整个字符串块
      LOG_INFO("{}", ss.str());
    }

  private:
    std::string VectorToString(const std::vector<std::string>& vec)
    {
      if (vec.empty()) return "{}";
      std::stringstream ss_vec;
      for (size_t i = 0; i < vec.size(); ++i)
      {
        ss_vec << (i == 0 ? "" : ", ") << vec[i];
      }
      return ss_vec.str();
    }

    void PrintNodeTree(std::stringstream& ss, const std::string& node_name, const std::string& prefix, bool is_last,
                       std::set<std::string>& visited_nodes,
                       const std::map<std::string, std::vector<std::string>>& adj)
    {
      const auto& node = nodes_.at(node_name);
      ss << prefix << (is_last ? "└── " : "├── ") << node->GetName() << "\n";

      std::string child_prefix = prefix + (is_last ? "    " : "│   ");

      // 检查是否已经访问过
      if (visited_nodes.count(node_name))
      {
        ss << child_prefix << "(...)\n";
        return;
      }
      visited_nodes.insert(node_name);

      // 打印详细信息
      ss << child_prefix << "  [Inputs: " << VectorToString(node->GetInputs()) << "]\n";
      if (!node->GetControlInputs().empty())
      {
        ss << child_prefix << "  [Control: " << VectorToString(node->GetControlInputs()) << "]\n";
      }
      ss << child_prefix << "  [Outputs: " << VectorToString(node->GetOutputs()) << "]\n";

      // 递归打印后继节点
      if (adj.count(node_name) && !adj.at(node_name).empty())
      {
        const auto& successors = adj.at(node_name);
        for (size_t i = 0; i < successors.size(); ++i)
        {
          PrintNodeTree(ss, successors[i], child_prefix, (i == successors.size() - 1), visited_nodes, adj);
        }
      }
    }

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
    std::atomic<bool> graph_failed_;
    std::mutex exception_mutex_;
    std::exception_ptr first_exception_;
  };
} // namespace kpipeline
#endif // KPIPELINE_GRAPH_H_
