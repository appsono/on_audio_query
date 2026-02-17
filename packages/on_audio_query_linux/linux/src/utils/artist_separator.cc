#include "artist_separator.h"
#include <algorithm>
#include <cctype>
#include <functional>
#include <iostream>
#include <sstream>

namespace on_audio_query_linux {

/// Define separators
const std::vector<std::string> ArtistSeparator::SEPARATORS = {
  " feat. ",
  " ft. ",
  " featuring ",
  " / ",
  "/",
  ", ",
  " & ",
  "&",
  " and ",
  " x ",
  " X ",
};

/// Define exception artists
const std::set<std::string> ArtistSeparator::EXACT_EXCEPTIONS = {
  "simon & garfunkel",
  "hall & oates",
  "earth, wind & fire",
  "emerson, lake & palmer",
  "crosby, stills, nash & young",
  "peter, paul and mary",
  "blood, sweat & tears",
  "up, bustle and out",
  "me first and the gimme gimmes",
  "hootie & the blowfish",
  "katrina and the waves",
  "kc and the sunshine band",
  "martha and the vandellas",
  "gladys knight & the pips",
  "bob seger & the silver bullet band",
  "huey lewis and the news",
  "echo & the bunnymen",
  "tom petty and the heartbreakers",
  "bob marley & the wailers",
  "sly & the family stone",
  "bruce springsteen & the e street band",
  "diana ross & the supremes",
  "smokey robinson & the miracles",
  "joan jett & the blackhearts",
  "prince & the revolution",
  "derek & the dominos",
  "sergio mendes & brasil '66",
  "tyler, the creator",
  "panic! at the disco",
  "florence + the machine",
  "florence and the machine",
};

ArtistSeparator::ArtistSeparator() {}

ArtistSeparator& ArtistSeparator::Instance() {
  static ArtistSeparator instance;
  return instance;
}

/// Helper function to convert string to lowercase
static std::string ToLower(const std::string& str) {
  std::string result = str;
  std::transform(result.begin(), result.end(), result.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  return result;
}

/// Helper function to trim whitespace
static std::string Trim(const std::string& str) {
  size_t start = str.find_first_not_of(" \t\n\r");
  if (start == std::string::npos) return "";
  size_t end = str.find_last_not_of(" \t\n\r");
  return str.substr(start, end - start + 1);
}

bool ArtistSeparator::IsExceptionArtist(const std::string& artist_string) {
  std::string lower = ToLower(artist_string);
  return EXACT_EXCEPTIONS.find(lower) != EXACT_EXCEPTIONS.end();
}

std::vector<std::string> ArtistSeparator::SplitArtistString(const std::string& artist_string) {
  /// Check if entire string is an exception first
  if (IsExceptionArtist(artist_string)) {
    return {artist_string};
  }

  /// Smart splitting: protect exception artists within combined strings
  std::string working_string = artist_string;
  std::map<std::string, std::string> placeholders;
  int placeholder_index = 0;

  /// Sort exceptions by length (longest first) to avoid partial matches
  std::vector<std::string> sorted_exceptions(EXACT_EXCEPTIONS.begin(), EXACT_EXCEPTIONS.end());
  std::sort(sorted_exceptions.begin(), sorted_exceptions.end(),
            [](const std::string& a, const std::string& b) { return a.length() > b.length(); });

  /// Replace exception artists with placeholders
  for (const auto& exception : sorted_exceptions) {
    //case-insensitive search
    std::string lower_working = ToLower(working_string);
    size_t pos = lower_working.find(exception);

    if (pos != std::string::npos) {
      std::string placeholder = "___ARTIST_PLACEHOLDER_" + std::to_string(placeholder_index++) + "___";
      std::string original = working_string.substr(pos, exception.length());
      placeholders[placeholder] = original;
      working_string = working_string.substr(0, pos) + placeholder +
                      working_string.substr(pos + exception.length());
    }
  }

  /// Build regex pattern from all separators
  std::string pattern;
  for (size_t i = 0; i < SEPARATORS.size(); ++i) {
    if (i > 0) pattern += "|";
    //escape special regex characters
    std::string escaped;
    for (char c : SEPARATORS[i]) {
      if (c == '/' || c == '&' || c == '+') {
        escaped += '\\';
      }
      escaped += c;
    }
    pattern += "(" + escaped + ")";
  }

  /// Split by separators
  std::regex separator_regex(pattern, std::regex::icase);
  std::vector<std::string> parts;
  std::string current_part;

  std::sregex_token_iterator iter(working_string.begin(), working_string.end(), separator_regex, -1);
  std::sregex_token_iterator end;

  for (; iter != end; ++iter) {
    std::string part = Trim(iter->str());
    if (!part.empty()) {
      parts.push_back(part);
    }
  }

  //restore placeholders with original exception artist names
  for (auto& part : parts) {
    for (const auto& [placeholder, original] : placeholders) {
      size_t pos = part.find(placeholder);
      if (pos != std::string::npos) {
        part = part.substr(0, pos) + original + part.substr(pos + placeholder.length());
      }
    }
    part = Trim(part);
  }

  /// Only return split if we actually got multiple parts
  if (parts.size() > 1) {
    //remove empty parts
    parts.erase(std::remove_if(parts.begin(), parts.end(),
                               [](const std::string& s) { return s.empty(); }),
                parts.end());
    return parts;
  }

  return {artist_string};
}

int64_t ArtistSeparator::GenerateSplitArtistId(const std::string& artist_name) {
  // Normalize to lowercase and trim for consistency
  std::string normalized = ToLower(Trim(artist_name));

  /// Simple hash function
  std::hash<std::string> hasher;
  int64_t hash = static_cast<int64_t>(hasher(normalized));

  /// Ensure negative to distinguish from database IDs
  /// Use bitwise AND to keep within valid range
  return -(hash & 0x7FFFFFFF);
}

void ArtistSeparator::AddToIndex(const std::string& split_artist_name,
                                  const std::string& combined_artist_string) {
  std::string key = ToLower(split_artist_name);
  split_artist_index_[key].insert(combined_artist_string);
}

std::set<std::string> ArtistSeparator::GetCombinedArtistsFor(const std::string& artist_name) {
  std::string key = ToLower(artist_name);
  auto it = split_artist_index_.find(key);
  if (it != split_artist_index_.end()) {
    return it->second;
  }
  return {};
}

void ArtistSeparator::AddIdMapping(int64_t artist_id, const std::string& artist_name) {
  id_to_name_map_[artist_id] = artist_name;
}

std::string ArtistSeparator::GetArtistNameById(int64_t artist_id) {
  std::cout << "[ArtistSeparator] Looking up ID: " << artist_id
            << " (map size: " << id_to_name_map_.size() << ")" << std::endl;
  auto it = id_to_name_map_.find(artist_id);
  if (it != id_to_name_map_.end()) {
    std::cout << "[ArtistSeparator] Found: " << it->second << std::endl;
    return it->second;
  }
  std::cout << "[ArtistSeparator] Not found in map" << std::endl;
  return "";
}

void ArtistSeparator::ClearIndex() {
  split_artist_index_.clear();
  //Note: id_to_name_map_ is NOT cleared here because:
  //1. The mappings are deterministic (same name always generates same ID)
  //2. We need the mappings to persist across different query types
  //3. The mappings will be rebuilt when processing artists anyway
}

bool ArtistSeparator::IsSplitArtistId(int64_t artist_id) {
  return artist_id < 0;
}

}  // namespace on_audio_query_linux
