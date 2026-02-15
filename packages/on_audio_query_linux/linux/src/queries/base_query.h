#ifndef BASE_QUERY_H_
#define BASE_QUERY_H_

#include <flutter_linux/flutter_linux.h>
#include "../core/database_manager.h"

namespace on_audio_query_linux {

class BaseQuery {
 public:
  explicit BaseQuery(DatabaseManager* db_manager);
  virtual ~BaseQuery() = default;

  virtual FlValue* Execute() = 0;

 protected:
  DatabaseManager* db_manager_;

  /// Helper methods to convert C++ types to FlValue
  FlValue* SongToFlValue(const SongMetadata& song);
  FlValue* AlbumToFlValue(const AlbumData& album);
  FlValue* ArtistToFlValue(const ArtistData& artist);
  FlValue* GenreToFlValue(const GenreData& genre);
  FlValue* PlaylistToFlValue(const PlaylistData& playlist);
};

}  // namespace on_audio_query_linux

#endif  // BASE_QUERY_H_
