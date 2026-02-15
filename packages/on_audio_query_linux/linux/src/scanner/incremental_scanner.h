#ifndef INCREMENTAL_SCANNER_H_
#define INCREMENTAL_SCANNER_H_

#include <string>
#include <vector>
#include <set>
#include "../core/database_manager.h"

namespace on_audio_query_linux {

class IncrementalScanner {
 public:
  explicit IncrementalScanner(DatabaseManager* db_manager);
  ~IncrementalScanner();

  struct ScanDelta {
    std::vector<std::string> new_files;
    std::vector<std::string> modified_files;
    std::vector<int64_t> deleted_file_ids;
    std::vector<std::string> deleted_file_paths;
  };

  /// Detect changes since last scan
  ScanDelta DetectChanges(const std::string& directory,
                          const std::vector<std::string>& current_files);

  /// Check if file needs rescanning based on modification time
  bool NeedsRescan(const std::string& file_path, int64_t db_mtime);

 private:
  DatabaseManager* db_manager_;

  /// Get file modification time (seconds since epoch)
  int64_t GetFileModificationTime(const std::string& file_path);

  /// Check if file exists
  bool FileExists(const std::string& file_path);
};

}  // namespace on_audio_query_linux

#endif  // INCREMENTAL_SCANNER_H_
