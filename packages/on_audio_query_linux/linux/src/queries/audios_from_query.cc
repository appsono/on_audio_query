#include "audios_from_query.h"
#include <iostream>

namespace on_audio_query_linux {

AudiosFromQuery::AudiosFromQuery(DatabaseManager* db_manager, int64_t id, int type)
    : BaseQuery(db_manager), id_(id), type_(type) {}

AudiosFromQuery::~AudiosFromQuery() {}

FlValue* AudiosFromQuery::Execute() {
  std::cout << "[AudiosFromQuery] Querying audios for id=" << id_ << ", type=" << type_ << std::endl;

  QueryParams params;

  //type: 0=album, 1=artist, 2=genre
  if (type_ == 0) {
    params.album_filter = id_;
  } else if (type_ == 1) {
    params.artist_filter = id_;
  } else if (type_ == 2) {
    params.genre_filter = id_;
  }

  auto songs = db_manager_->QuerySongs(params);

  FlValue* result_list = fl_value_new_list();

  for (const auto& song : songs) {
    FlValue* song_map = SongToFlValue(song);
    fl_value_append_take(result_list, song_map);
  }

  std::cout << "[AudiosFromQuery] Returning " << songs.size() << " songs" << std::endl;

  return result_list;
}

}  // namespace on_audio_query_linux
