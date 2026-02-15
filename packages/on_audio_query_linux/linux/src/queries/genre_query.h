#ifndef GENRE_QUERY_H_
#define GENRE_QUERY_H_

#include "base_query.h"

namespace on_audio_query_linux {

class GenreQuery : public BaseQuery {
 public:
  GenreQuery(DatabaseManager* db_manager, const QueryParams& params);
  ~GenreQuery();

  FlValue* Execute() override;

 private:
  QueryParams params_;
};

}  // namespace on_audio_query_linux

#endif  // GENRE_QUERY_H_
