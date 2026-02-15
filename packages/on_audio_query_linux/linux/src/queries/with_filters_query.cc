#include "with_filters_query.h"
#include <iostream>

namespace on_audio_query_linux {

WithFiltersQuery::WithFiltersQuery(DatabaseManager* db_manager, const std::string& search_term)
    : BaseQuery(db_manager), search_term_(search_term) {}

WithFiltersQuery::~WithFiltersQuery() {}

FlValue* WithFiltersQuery::Execute() {
  std::cout << "[WithFiltersQuery] Searching for: " << search_term_ << std::endl;

  QueryParams params;
  params.search_filter = search_term_;

  auto songs = db_manager_->QuerySongs(params);

  FlValue* result_list = fl_value_new_list();

  for (const auto& song : songs) {
    FlValue* song_map = SongToFlValue(song);
    fl_value_append_take(result_list, song_map);
  }

  std::cout << "[WithFiltersQuery] Found " << songs.size() << " results" << std::endl;

  return result_list;
}

}  // namespace on_audio_query_linux
