#include "audio_query.h"
#include <iostream>

namespace on_audio_query_linux {

AudioQuery::AudioQuery(DatabaseManager* db_manager, const QueryParams& params)
    : BaseQuery(db_manager), params_(params) {}

AudioQuery::~AudioQuery() {}

FlValue* AudioQuery::Execute() {
  std::cout << "[AudioQuery] Querying songs from database..." << std::endl;

  /// Query from database
  auto songs = db_manager_->QuerySongs(params_);

  /// Create result list
  FlValue* result_list = fl_value_new_list();

  for (const auto& song : songs) {
    FlValue* song_map = SongToFlValue(song);
    fl_value_append_take(result_list, song_map);
  }

  std::cout << "[AudioQuery] Returning " << songs.size() << " songs" << std::endl;

  return result_list;
}

}  // namespace on_audio_query_linux
