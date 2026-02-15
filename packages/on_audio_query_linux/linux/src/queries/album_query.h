#ifndef ALBUM_QUERY_H_
#define ALBUM_QUERY_H_

#include "base_query.h"

namespace on_audio_query_linux {

class AlbumQuery : public BaseQuery {
 public:
  AlbumQuery(DatabaseManager* db_manager, const QueryParams& params = QueryParams{});
  ~AlbumQuery();

  FlValue* Execute() override;

 private:
  QueryParams params_;
};

}  // namespace on_audio_query_linux

#endif  // ALBUM_QUERY_H_
