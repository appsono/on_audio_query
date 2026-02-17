#include "artist_query.h"
#include <iostream>

namespace on_audio_query_linux {

ArtistQuery::ArtistQuery(DatabaseManager* db_manager, const QueryParams& params)
    : BaseQuery(db_manager), params_(params) {}

ArtistQuery::~ArtistQuery() {}

FlValue* ArtistQuery::Execute() {
  std::cout << "[ArtistQuery] Querying artists from database..." << std::endl;

  //Artists are split and stored in database by UpdateArtistsWithSplitting()
  auto artists = db_manager_->QueryArtists(params_);

  FlValue* result_list = fl_value_new_list();

  for (const auto& artist : artists) {
    FlValue* artist_map = ArtistToFlValue(artist);
    fl_value_append_take(result_list, artist_map);
  }

  std::cout << "[ArtistQuery] Returning " << artists.size() << " artists" << std::endl;

  return result_list;
}

}  // namespace on_audio_query_linux
