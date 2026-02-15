#include "thread_pool.h"
#include <chrono>

namespace on_audio_query_linux {

ThreadPool::ThreadPool(size_t num_threads) : stop_(false), active_tasks_(0) {
  for (size_t i = 0; i < num_threads; ++i) {
    workers_.emplace_back([this] {
      while (true) {
        std::function<void()> task;

        {
          std::unique_lock<std::mutex> lock(queue_mutex_);
          condition_.wait(lock, [this] {
            return stop_ || !tasks_.empty();
          });

          if (stop_ && tasks_.empty()) {
            return;
          }

          task = std::move(tasks_.front());
          tasks_.pop();
        }

        active_tasks_++;
        try {
          task();
        } catch (...) {
          //catch all exceptions to prevent thread termination
        }
        active_tasks_--;
      }
    });
  }
}

ThreadPool::~ThreadPool() {
  {
    std::unique_lock<std::mutex> lock(queue_mutex_);
    stop_ = true;
  }

  condition_.notify_all();

  for (std::thread& worker : workers_) {
    if (worker.joinable()) {
      worker.join();
    }
  }
}

void ThreadPool::WaitAll() {
  while (active_tasks_ > 0 || !tasks_.empty()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
}

}  // namespace on_audio_query_linux
