#include "folder_query.h"
#include <iostream>

namespace on_audio_query_linux {

FolderQuery::FolderQuery(DatabaseManager* db_manager, const std::string& folder_path)
    : BaseQuery(db_manager), folder_path_(folder_path) {}

FolderQuery::~FolderQuery() {}

FlValue* FolderQuery::Execute() {
  std::cout << "[FolderQuery] Querying folder: " << folder_path_ << std::endl;

  QueryParams params;
  params.path_filter = folder_path_;

  auto songs = db_manager_->QuerySongs(params);

  FlValue* result_list = fl_value_new_list();

  for (const auto& song : songs) {
    FlValue* song_map = SongToFlValue(song);
    fl_value_append_take(result_list, song_map);
  }

  std::cout << "[FolderQuery] Found " << songs.size() << " songs in folder" << std::endl;

  return result_list;
}

}  // namespace on_audio_query_linux
