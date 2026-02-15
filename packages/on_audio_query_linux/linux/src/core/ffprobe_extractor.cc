#include "ffprobe_extractor.h"
#include "../utils/string_utils.h"

#include <nlohmann/json.hpp>
#include <iostream>
#include <sstream>
#include <cstdlib>
#include <array>
#include <memory>
#include <sys/stat.h>
#include <functional>

using json = nlohmann::json;

namespace on_audio_query_linux {

FFprobeExtractor::FFprobeExtractor() : cache_(kCacheSize) {}

FFprobeExtractor::~FFprobeExtractor() {}

bool FFprobeExtractor::IsAvailable() {
  int exit_code = system("which ffprobe > /dev/null 2>&1");
  return exit_code == 0;
}

std::optional<SongMetadata> FFprobeExtractor::Extract(const std::string& file_path) {
  //check cache first
  auto cached = cache_.Get(file_path);
  if (cached.has_value()) {
    return cached.value();
  }

  //run ffprobe
  auto output = RunFFprobe(file_path);

  if (output.exit_code != 0 || output.json_output.empty()) {
    std::cerr << "[FFprobeExtractor] Failed to extract metadata from: " << file_path << std::endl;
    //return fallback metadata
    SongMetadata fallback = CreateFallbackMetadata(file_path);
    cache_.Put(file_path, fallback);
    return fallback;
  }

  try {
    SongMetadata metadata = ParseFFprobeOutput(output.json_output, file_path);
    cache_.Put(file_path, metadata);
    return metadata;
  } catch (const std::exception& e) {
    std::cerr << "[FFprobeExtractor] Parse error: " << e.what() << std::endl;
    SongMetadata fallback = CreateFallbackMetadata(file_path);
    cache_.Put(file_path, fallback);
    return fallback;
  }
}

std::optional<std::vector<uint8_t>> FFprobeExtractor::ExtractArtwork(
    const std::string& file_path,
    const std::string& format) {

  //extract embedded artwork (ffmpeg)
  std::string temp_file = "/tmp/artwork_" + std::to_string(time(nullptr)) + "_" +
                         std::to_string(rand()) + "." + format;

  std::ostringstream cmd;
  cmd << "ffmpeg -i " << StringUtils::EscapeShellArg(file_path) << " "
      << "-an -vcodec copy " << StringUtils::EscapeShellArg(temp_file)
      << " 2>/dev/null";

  int exit_code = system(cmd.str().c_str());

  if (exit_code != 0) {
    return std::nullopt;
  }

  //read extracted artwork
  FILE* file = fopen(temp_file.c_str(), "rb");
  if (!file) {
    std::remove(temp_file.c_str());
    return std::nullopt;
  }

  //get file size
  fseek(file, 0, SEEK_END);
  long size = ftell(file);
  fseek(file, 0, SEEK_SET);

  //read into buffer
  std::vector<uint8_t> buffer(size);
  size_t bytes_read = fread(buffer.data(), 1, size, file);
  fclose(file);

  //clean up temp file
  std::remove(temp_file.c_str());

  if (bytes_read != static_cast<size_t>(size)) {
    return std::nullopt;
  }

  return buffer;
}

std::vector<std::optional<SongMetadata>> FFprobeExtractor::ExtractBatch(
    const std::vector<std::string>& file_paths,
    std::function<void(int, int)> progress_callback) {

  std::vector<std::optional<SongMetadata>> results;
  results.reserve(file_paths.size());

  int processed = 0;
  int total = file_paths.size();

  for (const auto& path : file_paths) {
    results.push_back(Extract(path));
    processed++;

    if (progress_callback && processed % 10 == 0) {
      progress_callback(processed, total);
    }
  }

  if (progress_callback) {
    progress_callback(total, total);
  }

  return results;
}

FFprobeExtractor::FFprobeOutput FFprobeExtractor::RunFFprobe(
    const std::string& file_path,
    const std::vector<std::string>& extra_args) {

  //build ffprobe command
  std::ostringstream cmd;
  cmd << "ffprobe -v quiet -print_format json "
      << "-show_format -show_streams "
      << "-show_entries format=duration,size "
      << "-show_entries format_tags=artist,album,title,genre,date,track,disc,composer ";

  for (const auto& arg : extra_args) {
    cmd << arg << " ";
  }

  cmd << StringUtils::EscapeShellArg(file_path) << " 2>&1";

  //execute command and capture output
  std::array<char, 128> buffer;
  std::string result;
  std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.str().c_str(), "r"), pclose);

  if (!pipe) {
    return {"", -1};
  }

  while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
    result += buffer.data();
  }

  int exit_code = pclose(pipe.release()) >> 8;

  return {result, exit_code};
}

SongMetadata FFprobeExtractor::ParseFFprobeOutput(const std::string& json_output,
                                                   const std::string& file_path) {
  SongMetadata metadata = {};

  auto j = json::parse(json_output);

  /// Extract format data
  if (j.contains("format")) {
    auto& format = j["format"];

    /// Duration (millisecs)
    if (format.contains("duration")) {
      std::string duration_str = format["duration"].get<std::string>();
      double duration_secs = std::stod(duration_str);
      metadata.duration = static_cast<int64_t>(duration_secs * 1000);
    }

    /// File size
    if (format.contains("size")) {
      std::string size_str = format["size"].get<std::string>();
      metadata.size = std::stoll(size_str);
    }

    /// Tags
    if (format.contains("tags")) {
      auto& tags = format["tags"];

      //helper lambda to get tag value
      auto getTag = [&tags](const std::string& key, const std::string& default_val) -> std::string {
        if (tags.contains(key)) {
          return tags[key].get<std::string>();
        }
        //try uppercase key
        std::string upper_key = StringUtils::ToLower(key);
        for (auto it = tags.begin(); it != tags.end(); ++it) {
          if (StringUtils::EqualsIgnoreCase(it.key(), key)) {
            return it.value().get<std::string>();
          }
        }
        return default_val;
      };

      metadata.title = getTag("title", StringUtils::GetFilenameWithoutExtension(file_path));
      metadata.artist = getTag("artist", "Unknown Artist");
      metadata.album = getTag("album", "Unknown Album");
      metadata.genre = getTag("genre", "Unknown");

      //year (date tag9
      std::string date_str = getTag("date", "");
      if (!date_str.empty()) {
        metadata.year = StringUtils::ExtractYear(date_str);
      }

      //Track number
      std::string track_str = getTag("track", "0");
      metadata.track = StringUtils::ParseTrackNumber(track_str);
    } else {
      //no tags => use fallback values
      metadata.title = StringUtils::GetFilenameWithoutExtension(file_path);
      metadata.artist = "Unknown Artist";
      metadata.album = "Unknown Album";
      metadata.genre = "Unknown";
      metadata.year = 0;
      metadata.track = 0;
    }
  }

  /// Generate IDs
  metadata.id = GenerateId(file_path);
  metadata.album_id = GenerateId(metadata.album);
  metadata.artist_id = GenerateId(metadata.artist);
  metadata.genre_id = GenerateId(metadata.genre);

  /// File info
  metadata.data = file_path;
  metadata.uri = "file://" + file_path;
  metadata.display_name = StringUtils::GetFilename(file_path);
  metadata.display_name_wo_ext = StringUtils::GetFilenameWithoutExtension(file_path);
  metadata.file_extension = StringUtils::GetFileExtension(file_path);

  /// File timestamps
  struct stat st;
  if (stat(file_path.c_str(), &st) == 0) {
    metadata.date_added = st.st_ctime * 1000;
    metadata.date_modified = st.st_mtime * 1000;
  }

  metadata.is_music = true;

  return metadata;
}

SongMetadata FFprobeExtractor::CreateFallbackMetadata(const std::string& file_path) {
  SongMetadata metadata = {};

  /// Basic file info
  metadata.id = GenerateId(file_path);
  metadata.data = file_path;
  metadata.uri = "file://" + file_path;
  metadata.display_name = StringUtils::GetFilename(file_path);
  metadata.display_name_wo_ext = StringUtils::GetFilenameWithoutExtension(file_path);
  metadata.file_extension = StringUtils::GetFileExtension(file_path);

  /// Default metadata
  metadata.title = metadata.display_name_wo_ext;
  metadata.artist = "Unknown Artist";
  metadata.album = "Unknown Album";
  metadata.genre = "Unknown";
  metadata.album_id = GenerateId(metadata.album);
  metadata.artist_id = GenerateId(metadata.artist);
  metadata.genre_id = GenerateId(metadata.genre);

  /// File stats
  struct stat st;
  if (stat(file_path.c_str(), &st) == 0) {
    metadata.size = st.st_size;
    metadata.date_added = st.st_ctime * 1000;
    metadata.date_modified = st.st_mtime * 1000;
  }

  metadata.year = 0;
  metadata.track = 0;
  metadata.duration = 0;
  metadata.is_music = true;

  return metadata;
}

int64_t FFprobeExtractor::GenerateId(const std::string& input) {
  std::hash<std::string> hasher;
  return static_cast<int64_t>(hasher(input));
}

}  // namespace on_audio_query_linux
