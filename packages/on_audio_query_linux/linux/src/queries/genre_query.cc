#include "genre_query.h"
#include <iostream>

namespace on_audio_query_linux {

GenreQuery::GenreQuery(DatabaseManager* db_manager, const QueryParams& params)
    : BaseQuery(db_manager), params_(params) {}

GenreQuery::~GenreQuery() {}

FlValue* GenreQuery::Execute() {
  std::cout << "[GenreQuery] Starting query..." << std::endl;

  auto genres = db_manager_->QueryGenres(params_);

  FlValue* result_list = fl_value_new_list();

  for (const auto& genre : genres) {
    FlValue* genre_map = GenreToFlValue(genre);
    fl_value_append_take(result_list, genre_map);
  }

  std::cout << "[GenreQuery] Query complete, returning " << genres.size() << " genres" << std::endl;

  return result_list;
}

}  // namespace on_audio_query_linux
