#include "string_utils.h"
#include <sstream>
#include <regex>

namespace on_audio_query_linux {

std::string StringUtils::ToLower(const std::string& str) {
  std::string result = str;
  std::transform(result.begin(), result.end(), result.begin(),
                [](unsigned char c) { return std::tolower(c); });
  return result;
}

bool StringUtils::EqualsIgnoreCase(const std::string& a, const std::string& b) {
  return ToLower(a) == ToLower(b);
}

std::string StringUtils::Trim(const std::string& str) {
  size_t first = str.find_first_not_of(" \t\n\r");
  if (first == std::string::npos) {
    return "";
  }

  size_t last = str.find_last_not_of(" \t\n\r");
  return str.substr(first, last - first + 1);
}

std::vector<std::string> StringUtils::Split(const std::string& str, char delimiter) {
  std::vector<std::string> result;
  std::stringstream ss(str);
  std::string item;

  while (std::getline(ss, item, delimiter)) {
    result.push_back(item);
  }

  return result;
}

std::string StringUtils::Join(const std::vector<std::string>& parts, const std::string& delimiter) {
  if (parts.empty()) {
    return "";
  }

  std::ostringstream oss;
  oss << parts[0];
  for (size_t i = 1; i < parts.size(); ++i) {
    oss << delimiter << parts[i];
  }

  return oss.str();
}

std::string StringUtils::EscapeShellArg(const std::string& arg) {
  // Use single quotes which handle all special characters except single quotes
  // For single quotes, close the quoted string, add escaped quote, reopen
  std::string escaped = "'";
  for (char c : arg) {
    if (c == '\'') {
      escaped += "'\\''";  // Close quote, add escaped quote, open quote
    } else {
      escaped += c;
    }
  }
  escaped += "'";
  return escaped;
}

std::string StringUtils::GetFilename(const std::string& path) {
  size_t slash_pos = path.find_last_of('/');
  if (slash_pos != std::string::npos) {
    return path.substr(slash_pos + 1);
  }
  return path;
}

std::string StringUtils::GetFilenameWithoutExtension(const std::string& path) {
  std::string filename = GetFilename(path);
  size_t dot_pos = filename.find_last_of('.');
  if (dot_pos != std::string::npos) {
    return filename.substr(0, dot_pos);
  }
  return filename;
}

std::string StringUtils::GetFileExtension(const std::string& path) {
  size_t dot_pos = path.find_last_of('.');
  if (dot_pos != std::string::npos) {
    return path.substr(dot_pos + 1);
  }
  return "";
}

int StringUtils::ParseTrackNumber(const std::string& track_str) {
  if (track_str.empty()) {
    return 0;
  }

  // Handle "5/12" format - extract first number
  std::string trimmed = Trim(track_str);
  size_t slash_pos = trimmed.find('/');
  if (slash_pos != std::string::npos) {
    trimmed = trimmed.substr(0, slash_pos);
  }

  try {
    return std::stoi(trimmed);
  } catch (...) {
    return 0;
  }
}

int StringUtils::ParseDiscNumber(const std::string& disc_str) {
  return ParseTrackNumber(disc_str);  // Same format
}

int StringUtils::ExtractYear(const std::string& date_str) {
  if (date_str.empty()) {
    return 0;
  }

  // Try to extract 4-digit year from date string
  std::regex year_regex(R"(\b(\d{4})\b)");
  std::smatch match;

  if (std::regex_search(date_str, match, year_regex)) {
    try {
      return std::stoi(match[1].str());
    } catch (...) {
      return 0;
    }
  }

  return 0;
}

}  // namespace on_audio_query_linux
