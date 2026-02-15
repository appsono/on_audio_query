#ifndef SCAN_COORDINATOR_H_
#define SCAN_COORDINATOR_H_

#include <string>
#include <vector>
#include <atomic>
#include <mutex>
#include <functional>
#include "../core/database_manager.h"
#include "../core/ffprobe_extractor.h"
#include "../core/thread_pool.h"
#include "file_scanner.h"
#include "incremental_scanner.h"

namespace on_audio_query_linux {

class ScanCoordinator {
 public:
  ScanCoordinator(DatabaseManager* db_manager,
                  FFprobeExtractor* ffprobe,
                  ThreadPool* thread_pool);
  ~ScanCoordinator();

  struct ScanProgress {
    int total_files;
    int processed_files;
    int new_files;
    int updated_files;
    int deleted_files;
    int failed_files;
  };

  using ProgressCallback = std::function<void(const ScanProgress&)>;

  /// Full scan (initial or forced rescan)
  void FullScan(const std::string& directory,
                ProgressCallback callback = nullptr);

  /// Incremental scan (detect and process changes)
  void IncrementalScan(const std::string& directory,
                       ProgressCallback callback = nullptr);

  /// Async scan (returns immediately, notifies via callback)
  void AsyncScan(const std::string& directory,
                 bool incremental,
                 ProgressCallback callback);

  /// Cancel ongoing scan
  void CancelScan();

  /// Check if scan is in progress
  bool IsScanInProgress() const { return scan_in_progress_.load(); }

 private:
  DatabaseManager* db_manager_;
  FFprobeExtractor* ffprobe_;
  ThreadPool* thread_pool_;
  FileScanner file_scanner_;
  IncrementalScanner incremental_scanner_;

  std::atomic<bool> cancel_requested_;
  std::atomic<bool> scan_in_progress_;
  std::mutex scan_mutex_;

  /// Process a list of files in parallel
  void ProcessFiles(const std::vector<std::string>& files,
                    ScanProgress& progress,
                    ProgressCallback callback);

  /// Update aggregated tables after scan
  void UpdateAggregatedTables();
};

}  // namespace on_audio_query_linux

#endif  // SCAN_COORDINATOR_H_
