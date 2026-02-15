#ifndef PLAYLIST_QUERY_H_
#define PLAYLIST_QUERY_H_

#include "base_query.h"

namespace on_audio_query_linux {

class PlaylistQuery : public BaseQuery {
 public:
  PlaylistQuery(DatabaseManager* db_manager);
  ~PlaylistQuery();

  FlValue* Execute() override;
};

}  // namespace on_audio_query_linux

#endif  // PLAYLIST_QUERY_H_
