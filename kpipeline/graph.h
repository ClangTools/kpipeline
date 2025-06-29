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

      ThreadPool pool(num_threads);
      std::atomic<size_t> finished_nodes_count(0);
      size_t total_graph_nodes = nodes_.size();

      std::mutex completion_mutex;
      std::condition_variable cv_completion;

      std::function<void(const std::string&)> schedule_next;
      std::function<void(const std::string&)> prune_branch;

      auto task_wrapper = [this, &ws, &schedule_next, &profiler, enable_profiling, &cv_completion
        ](const std::string& node_name)
      {
        // 如果图已经失败，则不执行任何新任务
        if (graph_failed_.load()) { return; }

        try
        {
          LOG_INFO("[Thread {}] Executing Node: {}", std::this_thread::get_id(), node_name);
          auto start_time = std::chrono::high_resolution_clock::now();

          nodes_.at(node_name)->Execute(ws);

          if (enable_profiling) profiler.End(node_name, start_time);
          schedule_next(node_name);
        }
        catch (const std::exception& e)
        {
          std::lock_guard<std::mutex> lock(exception_mutex_);
          // 只记录第一个发生的异常
          if (!first_exception_)
          {
            first_exception_ = std::current_exception();
            graph_failed_ = true;
            LOG_INFO("!!! Graph execution failed in node '{}'. Halting all operations. Error:{}", node_name, e.what());
          }
          // 唤醒主线程，让它提前结束并重新抛出异常
          cv_completion.notify_one();
        }
      };

      schedule_next = [this, &ws, &adj, &in_degree, &pool, &finished_nodes_count, total_graph_nodes, &cv_completion, &
          task_wrapper, &prune_branch](const std::string& completed_node_name)
        {
          if (graph_failed_.load())
          {
            // 如果图已失败，我们仍需增加计数器以最终结束等待，但不再调度
            if (finished_nodes_count.fetch_add(1) + 1 == total_graph_nodes) { cv_completion.notify_one(); }
            return;
          }

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
                  pool.Enqueue(task_wrapper, successor_name);
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

      prune_branch = [this, &adj, &in_degree, &finished_nodes_count, total_graph_nodes, &cv_completion, &prune_branch](
        const std::string& pruned_node_name)
        {
          if (graph_failed_.load())
          {
            if (finished_nodes_count.fetch_add(1) + 1 == total_graph_nodes) { cv_completion.notify_one(); }
            return;
          }

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
          pool.Enqueue(task_wrapper, name);
        }
      }

      std::unique_lock<std::mutex> lock(completion_mutex);
      // 等待条件：所有节点完成 或 图执行失败
      cv_completion.wait(lock, [&]
      {
        return finished_nodes_count.load() == total_graph_nodes || graph_failed_.load();
      });

      if (graph_failed_.load())
      {
        LOG_WARN("--- Graph Execution Halted Due to Error ---");
        // 等待线程池中的现有任务完成或退出，以避免悬空引用
      }
      else
      {
        LOG_INFO("--- Graph Execution Finished Successfully ---");
      }

      if (enable_profiling)
      {
        profiler.PrintReport();
      }

      // 如果有异常，就在主线程中重新抛出它
      if (first_exception_)
      {
        std::rethrow_exception(first_exception_);
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

    // --- 用于快速失败的成员 ---
    std::atomic<bool> graph_failed_;
    std::mutex exception_mutex_;
    std::exception_ptr first_exception_;
  };
} // namespace kpipeline
#endif // KPIPELINE_GRAPH_H_
