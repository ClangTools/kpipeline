#ifndef KPIPELINE_THREAD_POOL_H_
#define KPIPELINE_THREAD_POOL_H_

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>
#include <stdexcept>

namespace kpipeline
{
  class ThreadPool
  {
  public:
    ThreadPool(size_t num_threads);
    ~ThreadPool();

    template <class F, class... Args>
    auto Enqueue(F&& f, Args&&... args)
      -> std::future<typename std::result_of<F(Args...)>::type>;

    size_t GetThreadCount() const { return workers_.size(); }

  private:
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex queue_mutex_;
    std::condition_variable condition_;
    bool stop_;
  };

  inline ThreadPool::ThreadPool(size_t num_threads) : stop_(false)
  {
    for (size_t i = 0; i < num_threads; ++i)
    {
      workers_.emplace_back([this]
      {
        while (true)
        {
          std::function<void()> task;
          {
            std::unique_lock<std::mutex> lock(this->queue_mutex_);
            this->condition_.wait(lock, [this] { return this->stop_ || !this->tasks_.empty(); });
            if (this->stop_ && this->tasks_.empty()) return;
            task = std::move(this->tasks_.front());
            this->tasks_.pop();
          }
          task();
        }
      });
    }
  }

  inline ThreadPool::~ThreadPool()
  {
    {
      std::unique_lock<std::mutex> lock(queue_mutex_);
      stop_ = true;
    }
    condition_.notify_all();
    for (std::thread& worker : workers_)
    {
      worker.join();
    }
  }

  template <class F, class... Args>
  auto ThreadPool::Enqueue(F&& f, Args&&... args)
    -> std::future<std::result_of_t<F(Args...)>>
  {
    using return_type = std::result_of_t<F(Args...)>;

    auto task = std::make_shared<std::packaged_task<return_type()>>(
      std::bind(std::forward<F>(f), std::forward<Args>(args)...)
    );

    std::future<return_type> res = task->get_future();
    {
      std::unique_lock<std::mutex> lock(queue_mutex_);
      if (stop_) throw std::runtime_error("enqueue on stopped ThreadPool");
      tasks_.emplace([task]() { (*task)(); });
    }
    condition_.notify_one();
    return res;
  }
} // namespace kpipeline

#endif // KPIPELINE_THREAD_POOL_H_
