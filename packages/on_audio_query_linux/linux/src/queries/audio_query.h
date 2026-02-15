#ifndef AUDIO_QUERY_H_
#define AUDIO_QUERY_H_

#include "base_query.h"

namespace on_audio_query_linux {

class AudioQuery : public BaseQuery {
 public:
  AudioQuery(DatabaseManager* db_manager, const QueryParams& params = QueryParams{});
  ~AudioQuery();

  FlValue* Execute() override;

 private:
  QueryParams params_;
};

}  // namespace on_audio_query_linux

#endif  // AUDIO_QUERY_H_
