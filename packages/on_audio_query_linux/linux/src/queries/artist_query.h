#ifndef ARTIST_QUERY_H_
#define ARTIST_QUERY_H_

#include "base_query.h"

namespace on_audio_query_linux {

class ArtistQuery : public BaseQuery {
 public:
  ArtistQuery(DatabaseManager* db_manager, const QueryParams& params = QueryParams{});
  ~ArtistQuery();

  FlValue* Execute() override;

 private:
  QueryParams params_;
};

}  // namespace on_audio_query_linux

#endif  // ARTIST_QUERY_H_
