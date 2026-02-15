#include "album_query.h"
#include <iostream>

namespace on_audio_query_linux {

AlbumQuery::AlbumQuery(DatabaseManager* db_manager, const QueryParams& params)
    : BaseQuery(db_manager), params_(params) {}

AlbumQuery::~AlbumQuery() {}

FlValue* AlbumQuery::Execute() {
  std::cout << "[AlbumQuery] Querying albums from database..." << std::endl;

  auto albums = db_manager_->QueryAlbums(params_);

  FlValue* result_list = fl_value_new_list();

  for (const auto& album : albums) {
    FlValue* album_map = AlbumToFlValue(album);
    fl_value_append_take(result_list, album_map);
  }

  std::cout << "[AlbumQuery] Returning " << albums.size() << " albums" << std::endl;

  return result_list;
}

}  // namespace on_audio_query_linux
