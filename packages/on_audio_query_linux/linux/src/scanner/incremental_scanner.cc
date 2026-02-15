#include "incremental_scanner.h"
#include <sys/stat.h>
#include <unistd.h>
#include <iostream>
#include <map>

namespace on_audio_query_linux {

IncrementalScanner::IncrementalScanner(DatabaseManager* db_manager)
    : db_manager_(db_manager) {}

IncrementalScanner::~IncrementalScanner() {}

IncrementalScanner::ScanDelta IncrementalScanner::DetectChanges(
    const std::string& directory,
    const std::vector<std::string>& current_files) {

  ScanDelta delta;

  /// Get all songs from database for this directory
  std::map<std::string, SongMetadata> db_songs_map;
  auto db_songs = db_manager_->QuerySongs();

  for (const auto& song : db_songs) {
    //check if song is in the scanned directory
    if (song.data.find(directory) == 0) {
      db_songs_map[song.data] = song;
    }
  }

  /// Create set of current files for quick lookup
  std::set<std::string> current_files_set(current_files.begin(), current_files.end());

  /// Check current files against database
  for (const auto& file_path : current_files) {
    auto it = db_songs_map.find(file_path);

    if (it == db_songs_map.end()) {
      //file not in database = new
      delta.new_files.push_back(file_path);
    } else {
      //file exists in database = check if modified
      int64_t current_mtime = GetFileModificationTime(file_path);
      int64_t db_mtime = it->second.file_mtime;

      if (current_mtime > db_mtime) {
        //file has been modified
        delta.modified_files.push_back(file_path);
      }
    }
  }

  /// Find deleted files (in database but not in current scan)
  for (const auto& pair : db_songs_map) {
    const std::string& db_path = pair.first;
    const SongMetadata& song = pair.second;

    if (current_files_set.find(db_path) == current_files_set.end()) {
      //file no longer exists
      delta.deleted_file_ids.push_back(song.id);
      delta.deleted_file_paths.push_back(db_path);
    }
  }

  std::cout << "[IncrementalScanner] Delta: "
            << delta.new_files.size() << " new, "
            << delta.modified_files.size() << " modified, "
            << delta.deleted_file_ids.size() << " deleted"
            << std::endl;

  return delta;
}

bool IncrementalScanner::NeedsRescan(const std::string& file_path, int64_t db_mtime) {
  int64_t current_mtime = GetFileModificationTime(file_path);
  return current_mtime > db_mtime;
}

int64_t IncrementalScanner::GetFileModificationTime(const std::string& file_path) {
  struct stat st;
  if (stat(file_path.c_str(), &st) == 0) {
    return st.st_mtime;  //seconds since epoch
  }
  return 0;
}

bool IncrementalScanner::FileExists(const std::string& file_path) {
  return access(file_path.c_str(), F_OK) == 0;
}

}  // namespace on_audio_query_linux
