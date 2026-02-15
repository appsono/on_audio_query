#include "playlist_query.h"
#include <iostream>

namespace on_audio_query_linux {

PlaylistQuery::PlaylistQuery(DatabaseManager* db_manager)
    : BaseQuery(db_manager) {}

PlaylistQuery::~PlaylistQuery() {}

FlValue* PlaylistQuery::Execute() {
  std::cout << "[PlaylistQuery] Querying playlists from database..." << std::endl;

  auto playlists = db_manager_->QueryPlaylists();

  FlValue* result_list = fl_value_new_list();

  for (const auto& playlist : playlists) {
    FlValue* playlist_map = PlaylistToFlValue(playlist);
    fl_value_append_take(result_list, playlist_map);
  }

  std::cout << "[PlaylistQuery] Returning " << playlists.size() << " playlists" << std::endl;

  return result_list;
}

}  // namespace on_audio_query_linux
