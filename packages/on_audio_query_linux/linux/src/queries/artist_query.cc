#include "artist_query.h"
#include "../utils/artist_separator.h"
#include <iostream>
#include <map>
#include <algorithm>

namespace on_audio_query_linux {

ArtistQuery::ArtistQuery(DatabaseManager* db_manager, const QueryParams& params)
    : BaseQuery(db_manager), params_(params) {}

ArtistQuery::~ArtistQuery() {}

FlValue* ArtistQuery::Execute() {
  std::cout << "[ArtistQuery] Querying artists from database..." << std::endl;

  auto& separator = ArtistSeparator::Instance();
  separator.ClearIndex();

  /// Get raw artists from database
  auto raw_artists = db_manager_->QueryArtists(params_);

  /// Build MediaStore ID lookup for single (non-combined) artists
  std::map<std::string, int64_t> mediastore_id_lookup;

  for (const auto& artist_data : raw_artists) {
    //check if this is a single artist (not combined)
    auto split_check = separator.SplitArtistString(artist_data.artist);
    if (split_check.size() == 1) {
      std::string artist_lower = artist_data.artist;
      std::transform(artist_lower.begin(), artist_lower.end(), artist_lower.begin(), ::tolower);
      mediastore_id_lookup[artist_lower] = artist_data.id;
    }
  }

  /// Map to track seen artists (key = lowercase artist name)
  std::map<std::string, ArtistData> seen_artists;

  /// Process each raw artist
  for (const auto& artist_data : raw_artists) {
    auto split_artists = separator.SplitArtistString(artist_data.artist);
    int64_t original_id = artist_data.id;
    int num_albums = artist_data.number_of_albums;
    int num_tracks = artist_data.number_of_tracks;

    //build split artist index if this is a combined artist
    if (split_artists.size() > 1) {
      for (const auto& artist_name : split_artists) {
        separator.AddToIndex(artist_name, artist_data.artist);
      }
    }

    /// Process each split artist
    for (const auto& artist_name : split_artists) {
      std::string artist_key = artist_name;
      std::transform(artist_key.begin(), artist_key.end(), artist_key.begin(), ::tolower);

      if (seen_artists.find(artist_key) != seen_artists.end()) {
        //artist already exists => merge counts
        auto& existing_artist = seen_artists[artist_key];
        existing_artist.number_of_albums += num_albums;
        existing_artist.number_of_tracks += num_tracks;
      } else {
        //new artist => create entry
        ArtistData split_data;
        split_data.artist = artist_name;

        //determine ID: use MediaStore ID if available, otherwise generate
        int64_t artist_id;
        if (mediastore_id_lookup.find(artist_key) != mediastore_id_lookup.end()) {
          artist_id = mediastore_id_lookup[artist_key];
        } else if (split_artists.size() == 1) {
          artist_id = original_id;
        } else {
          artist_id = separator.GenerateSplitArtistId(artist_name);
        }

        split_data.id = artist_id;

        //add ID-to-name mapping for split artists
        if (artist_id < 0) {
          separator.AddIdMapping(artist_id, artist_name);
        }

        split_data.number_of_albums = num_albums;
        split_data.number_of_tracks = num_tracks;

        seen_artists[artist_key] = split_data;
      }
    }
  }

  std::cout << "[ArtistQuery] Split artist count: " << seen_artists.size()
            << " (from " << raw_artists.size() << " raw entries)" << std::endl;

  // Convert map to list
  FlValue* result_list = fl_value_new_list();

  for (const auto& [key, artist] : seen_artists) {
    FlValue* artist_map = ArtistToFlValue(artist);
    fl_value_append_take(result_list, artist_map);
  }

  std::cout << "[ArtistQuery] Returning " << seen_artists.size() << " artists" << std::endl;

  return result_list;
}

}  // namespace on_audio_query_linux
