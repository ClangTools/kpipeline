#ifndef KPIPELINE_WORKSPACE_H_
#define KPIPELINE_WORKSPACE_H_

#include <any>
#include <map>
#include <stdexcept>
#include <string>
#include <shared_mutex>
#include <mutex>

namespace kpipeline
{
  class PipelineException : public std::runtime_error
  {
  public:
    using std::runtime_error::runtime_error;
  };

  class Workspace
  {
  public:
    // 将数据存入工作空间
    template <typename T>
    void Set(const std::string& name, T&& data)
    {
      std::unique_lock<std::shared_mutex> lock(mutex_);
      data_[name] = std::forward<T>(data);
    }

    // 从工作空间获取数据
    // 注意：返回的是值的拷贝，而不是引用，以避免悬空引用问题。
    // 如果性能是极致要求，可以返回一个被锁保护的迭代器或智能指针，但拷贝更安全简单。
    template <typename T>
    T Get(const std::string& name) const
    {
      // 在读取前加锁
      std::shared_lock<std::shared_mutex> lock(mutex_);
      try
      {
        return std::any_cast<T>(data_.at(name));
      }
      catch (const std::out_of_range&)
      {
        throw PipelineException("Workspace error: Data with name '" + name + "' not found.");
      } catch (const std::bad_any_cast& e)
      {
        // 提供更详细的类型错误信息
        const std::any& val = data_.at(name);
        throw PipelineException("Workspace error: Type mismatch for data '" + name + "'. "
          "Requested: " + typeid(T).name() +
          ", Actual: " + val.type().name());
      }
    }

    // 检查数据是否存在
    bool Has(const std::string& name) const
    {
      std::shared_lock<std::shared_mutex> lock(mutex_);
      return data_.count(name) > 0;
    }

    std::any GetAny(const std::string& name) const
    {
      std::shared_lock<std::shared_mutex> lock(mutex_);
      try
      {
        return data_.at(name);
      }
      catch (const std::out_of_range&)
      {
        throw PipelineException("Workspace error: Data with name '" + name + "' not found.");
      }
    }

  private:
    // 使用 mutable 关键字，以便在 const 方法中也能锁定互斥锁
    mutable std::shared_mutex mutex_;
    std::map<std::string, std::any> data_;
  };
} // namespace kpipeline

#endif // KPIPELINE_WORKSPACE_H_
