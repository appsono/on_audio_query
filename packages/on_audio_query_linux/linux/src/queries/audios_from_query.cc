#include "audios_from_query.h"
#include "../utils/artist_separator.h"
#include <iostream>
#include <set>

namespace on_audio_query_linux {

AudiosFromQuery::AudiosFromQuery(DatabaseManager* db_manager, int64_t id, int type)
    : BaseQuery(db_manager), id_(id), type_(type) {}

AudiosFromQuery::~AudiosFromQuery() {}

FlValue* AudiosFromQuery::Execute() {
  std::cout << "[AudiosFromQuery] Querying audios for id=" << id_ << ", type=" << type_ << std::endl;

  //AudiosFromType enum: 0=ALBUM, 1=ALBUM_ID, 2=ARTIST, 3=ARTIST_ID, 4=GENRE, 5=GENRE_ID
  //Check if this is a split artist query (type=2 or 3 and negative ID)
  if ((type_ == 2 || type_ == 3) && id_ < 0) {
    return ExecuteSplitArtistQuery();
  }

  QueryParams params;

  if (type_ == 0 || type_ == 1) {
    //ALBUM or ALBUM_ID => filter by album_id
    params.album_filter = id_;
  } else if (type_ == 2 || type_ == 3) {
    //ARTIST or ARTIST_ID => filter by artist_id
    params.artist_filter = id_;
  } else if (type_ == 4 || type_ == 5) {
    //GENRE or GENRE_ID => filter by genre_id
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

FlValue* AudiosFromQuery::ExecuteSplitArtistQuery() {
  auto& separator = ArtistSeparator::Instance();

  //Get artist name from ID mapping
  std::string artist_name = separator.GetArtistNameById(id_);
  if (artist_name.empty()) {
    std::cerr << "[AudiosFromQuery] Could not find artist name for split artist ID: " << id_ << std::endl;
    return fl_value_new_list();
  }

  std::cout << "[AudiosFromQuery] Loading songs for split artist: " << artist_name
            << " (ID: " << id_ << ")" << std::endl;

  //Get all combined artist strings that contain this artist
  auto combined_artists = separator.GetCombinedArtistsFor(artist_name);

  FlValue* result_list = fl_value_new_list();
  std::set<int64_t> seen_song_ids;

  if (combined_artists.empty()) {
    //This artist exists as a standalone entry => query by exact artist name match
    auto all_songs = db_manager_->QuerySongs();

    for (const auto& song : all_songs) {
      if (song.artist == artist_name && seen_song_ids.find(song.id) == seen_song_ids.end()) {
        seen_song_ids.insert(song.id);
        FlValue* song_map = SongToFlValue(song);
        fl_value_append_take(result_list, song_map);
      }
    }
  } else {
    //Query songs for each combined artist string
    auto all_songs = db_manager_->QuerySongs();

    for (const auto& song : all_songs) {
      //Check if songs artist matches any of the combined artist strings
      if (combined_artists.find(song.artist) != combined_artists.end() &&
          seen_song_ids.find(song.id) == seen_song_ids.end()) {
        seen_song_ids.insert(song.id);
        FlValue* song_map = SongToFlValue(song);
        fl_value_append_take(result_list, song_map);
      }
    }
  }

  std::cout << "[AudiosFromQuery] Found " << seen_song_ids.size()
            << " songs for split artist: " << artist_name << std::endl;

  return result_list;
}

}  // namespace on_audio_query_linux
