#ifndef WITH_FILTERS_QUERY_H_
#define WITH_FILTERS_QUERY_H_

#include "base_query.h"

namespace on_audio_query_linux {

class WithFiltersQuery : public BaseQuery {
 public:
  WithFiltersQuery(DatabaseManager* db_manager, const std::string& search_term);
  ~WithFiltersQuery();

  FlValue* Execute() override;

 private:
  std::string search_term_;
};

}  // namespace on_audio_query_linux

#endif  // WITH_FILTERS_QUERY_H_
