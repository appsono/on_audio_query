#ifndef ARTWORK_QUERY_H_
#define ARTWORK_QUERY_H_

#include "base_query.h"
#include "../core/ffprobe_extractor.h"

namespace on_audio_query_linux {

class ArtworkQuery : public BaseQuery {
 public:
  ArtworkQuery(DatabaseManager* db_manager, FFprobeExtractor* ffprobe,
               int64_t id, int type, const std::string& format);
  ~ArtworkQuery();

  FlValue* Execute() override;

 private:
  FFprobeExtractor* ffprobe_;
  int64_t id_;
  int type_;  //0 = AUDIO, 1 = ALBUM
  std::string format_;
};

}  // namespace on_audio_query_linux

#endif  // ARTWORK_QUERY_H_
