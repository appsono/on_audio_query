#include "artwork_query.h"
#include <iostream>

namespace on_audio_query_linux {

ArtworkQuery::ArtworkQuery(DatabaseManager* db_manager, FFprobeExtractor* ffprobe,
                           int64_t id, int type, const std::string& format)
    : BaseQuery(db_manager), ffprobe_(ffprobe), id_(id), type_(type), format_(format) {}

ArtworkQuery::~ArtworkQuery() {}

FlValue* ArtworkQuery::Execute() {
  std::cout << "[ArtworkQuery] Query started - ID: " << id_ << ", Type: " << type_ << ", Format: " << format_ << std::endl;

  /// Check database cache first
  auto cached = db_manager_->GetCachedArtwork(id_, type_, format_);
  if (cached.has_value()) {
    std::cout << "[ArtworkQuery] Using cached artwork (" << cached->size() << " bytes)" << std::endl;
    return fl_value_new_uint8_list(cached->data(), cached->size());
  }

  /// Find the file path from database
  std::string file_path;
  if (type_ == 0) {  // AUDIO type => query by song ID
    auto song = db_manager_->GetSongById(id_);
    if (!song.has_value()) {
      std::cerr << "[ArtworkQuery] Song not found for ID: " << id_ << std::endl;
      return nullptr;
    }
    file_path = song->data;  //data field contains file path
  } else if (type_ == 1) {  //ALBUM type => query by album ID
    //get first song from album
    QueryParams params;
    params.album_filter = id_;
    auto songs = db_manager_->QuerySongs(params);
    if (songs.empty()) {
      std::cerr << "[ArtworkQuery] No songs found for album ID: " << id_ << std::endl;
      return nullptr;
    }
    file_path = songs[0].data;  //data field contains file path
  } else {
    std::cerr << "[ArtworkQuery] Unknown type: " << type_ << std::endl;
    return nullptr;
  }

  std::cout << "[ArtworkQuery] Extracting artwork from: " << file_path << std::endl;

  /// Extract artwork using FFprobe
  auto artwork = ffprobe_->ExtractArtwork(file_path, format_);

  if (!artwork.has_value() || artwork->empty()) {
    std::cout << "[ArtworkQuery] No artwork found" << std::endl;
    return nullptr;
  }

  /// Cache extracted artwork in database
  db_manager_->CacheArtwork(id_, type_, format_, artwork.value());

  std::cout << "[ArtworkQuery] Found artwork: " << artwork->size() << " bytes (cached)" << std::endl;

  return fl_value_new_uint8_list(artwork->data(), artwork->size());
}

}  // namespace on_audio_query_linux
