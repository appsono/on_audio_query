#ifndef FOLDER_QUERY_H_
#define FOLDER_QUERY_H_

#include "base_query.h"

namespace on_audio_query_linux {

class FolderQuery : public BaseQuery {
 public:
  FolderQuery(DatabaseManager* db_manager, const std::string& folder_path);
  ~FolderQuery();

  FlValue* Execute() override;

 private:
  std::string folder_path_;
};

}  // namespace on_audio_query_linux

#endif  // FOLDER_QUERY_H_
