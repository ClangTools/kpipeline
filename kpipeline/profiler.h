#ifndef KPIPELINE_PROFILER_H_
#define KPIPELINE_PROFILER_H_

#include <iostream>
#include <string>
#include <chrono>
#include <vector>
#include <mutex>
#include <iomanip>
#include <numeric>
#include <algorithm>

namespace kpipeline
{
  // 存储单个节点的计时结果
  struct ProfileResult
  {
    std::string node_name;
    std::chrono::duration<double, std::milli> duration;
  };

  // 线程安全的性能分析器
  class Profiler
  {
  public:
    Profiler() = default;

    // 开始对一个节点计时
    void Start(const std::string& node_name)
    {
      // 在 C++17 中，我们可以使用 if-constexpr 来避免在非 Windows 平台上链接不必要的库
      // 这里我们简单地使用 std::chrono
    }

    // 结束一个节点的计时，并记录结果
    void End(const std::string& node_name,
             std::chrono::time_point<std::chrono::high_resolution_clock> start_time)
    {
      auto end_time = std::chrono::high_resolution_clock::now();
      std::chrono::duration<double, std::milli> duration = end_time - start_time;

      std::lock_guard<std::mutex> lock(mutex_);
      results_.push_back({node_name, duration});
    }

    // 打印格式化的性能报告
    void PrintReport() const
    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (results_.empty())
      {
        std::cout << "\n--- Profiling Report (No nodes executed) ---\n";
        return;
      }

      auto results_copy = results_;
      // 按耗时降序排序
      std::sort(results_copy.begin(), results_copy.end(),
                [](const ProfileResult& a, const ProfileResult& b)
                {
                  return a.duration > b.duration;
                });

      double total_duration = std::accumulate(results_copy.begin(), results_copy.end(), 0.0,
                                              [](double sum, const ProfileResult& r)
                                              {
                                                return sum + r.duration.count();
                                              });

      std::cout << "\n--- Profiling Report ---\n";
      std::cout << std::left << std::setw(30) << "Node Name"
        << std::right << std::setw(15) << "Duration (ms)"
        << std::setw(10) << "% of Total" << std::endl;
      std::cout << std::string(55, '-') << std::endl;

      for (const auto& result : results_copy)
      {
        double percentage = (total_duration > 0) ? (result.duration.count() / total_duration * 100.0) : 0.0;
        std::cout << std::left << std::setw(30) << result.node_name
          << std::right << std::setw(15) << std::fixed << std::setprecision(3) << result.duration.count()
          << std::setw(9) << std::fixed << std::setprecision(1) << percentage << "%"
          << std::endl;
      }

      std::cout << std::string(55, '-') << std::endl;
      std::cout << std::left << std::setw(30) << "Total (Sum of durations)"
        << std::right << std::setw(15) << std::fixed << std::setprecision(3) << total_duration
        << std::endl;
    }

  private:
    mutable std::mutex mutex_;
    std::vector<ProfileResult> results_;
  };
} // namespace kpipeline

#endif // KPIPELINE_PROFILER_H_
