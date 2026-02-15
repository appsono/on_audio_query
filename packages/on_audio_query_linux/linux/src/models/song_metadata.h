#ifndef SONG_METADATA_H_
#define SONG_METADATA_H_

#include <string>

namespace on_audio_query_linux {

struct SongMetadata {
  int64_t id;
  std::string data;  //file path
  std::string uri;
  std::string display_name;
  std::string display_name_wo_ext;
  int64_t size;
  int64_t file_mtime;  //file modification time (seconds since epoch)
  std::string album;
  int64_t album_id;
  std::string artist;
  int64_t artist_id;
  int64_t date_added;
  int64_t date_modified;
  int64_t duration;  //milliseconds
  std::string title;
  int track;
  int year;
  std::string genre;
  int64_t genre_id;
  std::string file_extension;
  bool is_music;
};

}  // namespace on_audio_query_linux

#endif  // SONG_METADATA_H_
