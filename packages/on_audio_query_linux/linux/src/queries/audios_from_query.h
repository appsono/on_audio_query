#ifndef AUDIOS_FROM_QUERY_H_
#define AUDIOS_FROM_QUERY_H_

#include "base_query.h"

namespace on_audio_query_linux {

class AudiosFromQuery : public BaseQuery {
 public:
  AudiosFromQuery(DatabaseManager* db_manager, int64_t id, int type);
  ~AudiosFromQuery();

  FlValue* Execute() override;

 private:
  int64_t id_;
  int type_;  //artist, album, genre
};

}  // namespace on_audio_query_linux

#endif  // AUDIOS_FROM_QUERY_H_
