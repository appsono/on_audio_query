#ifndef THREAD_POOL_H_
#define THREAD_POOL_H_

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>
#include <atomic>
#include <stdexcept>

namespace on_audio_query_linux {

class ThreadPool {
 public:
  explicit ThreadPool(size_t num_threads);
  ~ThreadPool();

  /// Submit a task and get a future for the result
  template<typename F, typename... Args>
  auto Submit(F&& f, Args&&... args)
      -> std::future<std::invoke_result_t<F, Args...>>;

  /// Wait for all tasks to complete
  void WaitAll();

  /// Get number of threads
  size_t GetThreadCount() const { return workers_.size(); }

  /// Get number of active tasks
  int GetActiveTaskCount() const { return active_tasks_.load(); }

 private:
  std::vector<std::thread> workers_;
  std::queue<std::function<void()>> tasks_;

  std::mutex queue_mutex_;
  std::condition_variable condition_;
  std::atomic<bool> stop_;
  std::atomic<int> active_tasks_;
};

/// Template implementation must be in header
template<typename F, typename... Args>
auto ThreadPool::Submit(F&& f, Args&&... args)
    -> std::future<std::invoke_result_t<F, Args...>> {

  using return_type = std::invoke_result_t<F, Args...>;

  auto task = std::make_shared<std::packaged_task<return_type()>>(
      std::bind(std::forward<F>(f), std::forward<Args>(args)...)
  );

  std::future<return_type> res = task->get_future();

  {
    std::unique_lock<std::mutex> lock(queue_mutex_);

    if (stop_) {
      throw std::runtime_error("Cannot submit task to stopped ThreadPool");
    }

    tasks_.emplace([task]() { (*task)(); });
  }

  condition_.notify_one();
  return res;
}

}  // namespace on_audio_query_linux

#endif  // THREAD_POOL_H_
