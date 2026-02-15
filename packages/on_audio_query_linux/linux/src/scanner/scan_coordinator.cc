#include "scan_coordinator.h"
#include <iostream>
#include <thread>

namespace on_audio_query_linux {

ScanCoordinator::ScanCoordinator(DatabaseManager* db_manager,
                                 FFprobeExtractor* ffprobe,
                                 ThreadPool* thread_pool)
    : db_manager_(db_manager),
      ffprobe_(ffprobe),
      thread_pool_(thread_pool),
      incremental_scanner_(db_manager),
      cancel_requested_(false),
      scan_in_progress_(false) {}

ScanCoordinator::~ScanCoordinator() {
  CancelScan();
}

void ScanCoordinator::FullScan(const std::string& directory,
                               ProgressCallback callback) {
  std::lock_guard<std::mutex> lock(scan_mutex_);

  if (scan_in_progress_) {
    std::cerr << "[ScanCoordinator] Scan already in progress" << std::endl;
    return;
  }

  scan_in_progress_ = true;
  cancel_requested_ = false;

  std::cout << "[ScanCoordinator] Starting full scan of: " << directory << std::endl;

  /// Scan filesystem for all audio files
  auto files = file_scanner_.ScanDirectory(directory);

  ScanProgress progress;
  progress.total_files = files.size();
  progress.processed_files = 0;
  progress.new_files = 0;
  progress.updated_files = 0;
  progress.deleted_files = 0;
  progress.failed_files = 0;

  /// Process all files
  ProcessFiles(files, progress, callback);

  /// Update aggregated tables
  UpdateAggregatedTables();

  std::cout << "[ScanCoordinator] Full scan complete!" << std::endl;
  std::cout << "  New: " << progress.new_files << std::endl;
  std::cout << "  Updated: " << progress.updated_files << std::endl;
  std::cout << "  Failed: " << progress.failed_files << std::endl;

  scan_in_progress_ = false;
}

void ScanCoordinator::IncrementalScan(const std::string& directory,
                                      ProgressCallback callback) {
  std::lock_guard<std::mutex> lock(scan_mutex_);

  if (scan_in_progress_) {
    std::cerr << "[ScanCoordinator] Scan already in progress" << std::endl;
    return;
  }

  scan_in_progress_ = true;
  cancel_requested_ = false;

  std::cout << "[ScanCoordinator] Starting incremental scan of: " << directory << std::endl;

  /// Scan filesystem
  auto current_files = file_scanner_.ScanDirectory(directory);

  /// Detect changes
  auto delta = incremental_scanner_.DetectChanges(directory, current_files);

  ScanProgress progress;
  progress.total_files = delta.new_files.size() + delta.modified_files.size() +
                        delta.deleted_file_ids.size();
  progress.processed_files = 0;
  progress.new_files = 0;
  progress.updated_files = 0;
  progress.deleted_files = delta.deleted_file_ids.size();
  progress.failed_files = 0;

  /// Process new files
  if (!delta.new_files.empty()) {
    ProcessFiles(delta.new_files, progress, callback);
  }

  /// Process modified files
  if (!delta.modified_files.empty()) {
    ProcessFiles(delta.modified_files, progress, callback);
  }

  /// Delete removed files
  if (!delta.deleted_file_ids.empty()) {
    db_manager_->BeginTransaction();
    for (int64_t song_id : delta.deleted_file_ids) {
      db_manager_->DeleteSong(song_id);
    }
    db_manager_->CommitTransaction();

    progress.processed_files += delta.deleted_file_ids.size();
  }

  /// Update aggregated tables
  UpdateAggregatedTables();

  std::cout << "[ScanCoordinator] Incremental scan complete!" << std::endl;
  std::cout << "  New: " << progress.new_files << std::endl;
  std::cout << "  Modified: " << progress.updated_files << std::endl;
  std::cout << "  Deleted: " << progress.deleted_files << std::endl;
  std::cout << "  Failed: " << progress.failed_files << std::endl;

  /// Final callback
  if (callback) {
    callback(progress);
  }

  scan_in_progress_ = false;
}

void ScanCoordinator::AsyncScan(const std::string& directory,
                                bool incremental,
                                ProgressCallback callback) {
  //launch scan in background thread
  std::thread([this, directory, incremental, callback]() {
    if (incremental) {
      IncrementalScan(directory, callback);
    } else {
      FullScan(directory, callback);
    }
  }).detach();
}

void ScanCoordinator::CancelScan() {
  cancel_requested_ = true;
}

void ScanCoordinator::ProcessFiles(const std::vector<std::string>& files,
                                   ScanProgress& progress,
                                   ProgressCallback callback) {
  if (files.empty()) {
    return;
  }

  const size_t batch_size = 100;
  const size_t num_batches = (files.size() + batch_size - 1) / batch_size;

  std::mutex progress_mutex;

  /// Start transaction for batch inserts
  db_manager_->BeginTransaction();

  /// Process files in parallel batches
  std::vector<std::future<void>> futures;

  for (size_t batch_idx = 0; batch_idx < num_batches; ++batch_idx) {
    if (cancel_requested_) {
      break;
    }

    size_t start_idx = batch_idx * batch_size;
    size_t end_idx = std::min(start_idx + batch_size, files.size());

    std::vector<std::string> batch(
        files.begin() + start_idx,
        files.begin() + end_idx
    );

    //submit batch to thread pool
    auto future = thread_pool_->Submit([this, batch, &progress, &progress_mutex, callback]() {
      for (const auto& file_path : batch) {
        if (cancel_requested_) {
          return;
        }

        //extract metadata using FFprobe
        auto metadata_opt = ffprobe_->Extract(file_path);

        std::lock_guard<std::mutex> lock(progress_mutex);

        if (metadata_opt.has_value()) {
          //check if song exists in DB
          auto existing = db_manager_->GetSongByPath(file_path);

          if (existing.has_value()) {
            //update existing song
            db_manager_->UpdateSong(metadata_opt.value());
            progress.updated_files++;
          } else {
            //insert new song
            db_manager_->InsertSong(metadata_opt.value());
            progress.new_files++;
          }
        } else {
          progress.failed_files++;
        }

        progress.processed_files++;

        //call progress callback every 10 files
        if (callback && progress.processed_files % 10 == 0) {
          callback(progress);
        }
      }
    });

    futures.push_back(std::move(future));
  }

  /// Wait for all batches to complete
  for (auto& future : futures) {
    future.get();
  }

  /// Commit transaction
  db_manager_->CommitTransaction();

  /// Final callback
  if (callback) {
    callback(progress);
  }
}

void ScanCoordinator::UpdateAggregatedTables() {
  std::cout << "[ScanCoordinator] Updating aggregated tables..." << std::endl;
  db_manager_->UpdateAggregatedTables();
}

}  // namespace on_audio_query_linux
