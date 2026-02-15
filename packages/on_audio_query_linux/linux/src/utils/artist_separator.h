#ifndef ARTIST_SEPARATOR_H_
#define ARTIST_SEPARATOR_H_

#include <string>
#include <vector>
#include <set>
#include <map>
#include <regex>

namespace on_audio_query_linux {

class ArtistSeparator {
 public:
  static ArtistSeparator& Instance();

  /// Split an artist string into individual artists
  std::vector<std::string> SplitArtistString(const std::string& artist_string);

  /// Generate a deterministic ID for a split artist (negative to distinguish from DB IDs)
  int64_t GenerateSplitArtistId(const std::string& artist_name);

  /// Add artist to index (tracks which split artists appear in which combined strings)
  void AddToIndex(const std::string& split_artist_name, const std::string& combined_artist_string);

  /// Get all combined artist strings that contain a given artist
  std::set<std::string> GetCombinedArtistsFor(const std::string& artist_name);

  /// Add ID-to-name mapping for split artists
  void AddIdMapping(int64_t artist_id, const std::string& artist_name);

  /// Get artist name by ID
  std::string GetArtistNameById(int64_t artist_id);

  /// Clear all index data
  void ClearIndex();

  /// Check if an artist ID represents a split artist (negative ID)
  bool IsSplitArtistId(int64_t artist_id);

 private:
  ArtistSeparator();

  /// Check if an artist name is an exception that should not be split
  bool IsExceptionArtist(const std::string& artist_string);

  /// Separators used to split artist strings
  static const std::vector<std::string> SEPARATORS;

  /// Exact-match exceptions (band names that shouldn't be split)
  static const std::set<std::string> EXACT_EXCEPTIONS;

  /// Index mapping split artist names to combined strings they appear in
  std::map<std::string, std::set<std::string>> split_artist_index_;

  /// Index mapping split artist IDs to their display names
  std::map<int64_t, std::string> id_to_name_map_;
};

}  // namespace on_audio_query_linux

#endif  // ARTIST_SEPARATOR_H_
