#ifndef FFPROBE_EXTRACTOR_H_
#define FFPROBE_EXTRACTOR_H_

#include <string>
#include <vector>
#include <optional>
#include <functional>
#include "../models/song_metadata.h"
#include "../utils/lru_cache.h"

namespace on_audio_query_linux {

class FFprobeExtractor {
 public:
  FFprobeExtractor();
  ~FFprobeExtractor();

  /// Extract metadata (ffprobe)
  std::optional<SongMetadata> Extract(const std::string& file_path);

  /// Extract artwork (returns raw image bytes)
  std::optional<std::vector<uint8_t>> ExtractArtwork(const std::string& file_path,
                                                       const std::string& format = "jpeg");

  /// Batch extraction for parallel processing
  std::vector<std::optional<SongMetadata>> ExtractBatch(
      const std::vector<std::string>& file_paths,
      std::function<void(int, int)> progress_callback = nullptr
  );

  /// Check if ffprobe is available on the system
  static bool IsAvailable();

 private:
  struct FFprobeOutput {
    std::string json_output;
    int exit_code;
  };

  /// Run ffprobe command and capture output
  FFprobeOutput RunFFprobe(const std::string& file_path,
                           const std::vector<std::string>& extra_args = {});

  /// Parse FFprobe JSON output into SongMetadata
  SongMetadata ParseFFprobeOutput(const std::string& json_output,
                                   const std::string& file_path);

  /// Create fallback metadata when FFprobe fails
  SongMetadata CreateFallbackMetadata(const std::string& file_path);

  /// Generate ID from string (hash function)
  int64_t GenerateId(const std::string& input);

  /// LRU cache for recently extracted files (avoid re-extraction)
  LRUCache<std::string, SongMetadata> cache_;
  static constexpr size_t kCacheSize = 100;
};

}  // namespace on_audio_query_linux

#endif  // FFPROBE_EXTRACTOR_H_
