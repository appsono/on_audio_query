#ifndef STRING_UTILS_H_
#define STRING_UTILS_H_

#include <string>
#include <vector>
#include <algorithm>
#include <cctype>

namespace on_audio_query_linux {

class StringUtils {
 public:
  // Convert string to lowercase
  static std::string ToLower(const std::string& str);

  // Case-insensitive string comparison
  static bool EqualsIgnoreCase(const std::string& a, const std::string& b);

  // Trim whitespace from both ends
  static std::string Trim(const std::string& str);

  // Split string by delimiter
  static std::vector<std::string> Split(const std::string& str, char delimiter);

  // Join strings with delimiter
  static std::string Join(const std::vector<std::string>& parts, const std::string& delimiter);

  // Escape shell arguments for safe command execution
  static std::string EscapeShellArg(const std::string& arg);

  // Get filename from path
  static std::string GetFilename(const std::string& path);

  // Get filename without extension
  static std::string GetFilenameWithoutExtension(const std::string& path);

  // Get file extension
  static std::string GetFileExtension(const std::string& path);

  // Parse track number from string (e.g., "5/12" -> 5)
  static int ParseTrackNumber(const std::string& track_str);

  // Parse disc number from string
  static int ParseDiscNumber(const std::string& disc_str);

  // Extract year from date string (e.g., "2023-05-12" -> 2023)
  static int ExtractYear(const std::string& date_str);
};

}  // namespace on_audio_query_linux

#endif  // STRING_UTILS_H_
