#include "file_scanner.h"

#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fstream>
#include <algorithm>
#include <iostream>
#include <cstring>

namespace on_audio_query_linux {

FileScanner::FileScanner() {}

FileScanner::~FileScanner() {}

std::vector<std::string> FileScanner::ScanDirectory(const std::string& path) {
  std::vector<std::string> files;

  std::string scan_path = path;
  if (scan_path.empty()) {
    scan_path = GetDefaultMusicDirectory();
  }

  std::cout << "[FileScanner] Scanning directory: " << scan_path << std::endl;
  ScanDirectoryRecursive(scan_path, files);
  std::cout << "[FileScanner] Found " << files.size() << " audio files" << std::endl;

  return files;
}

std::string FileScanner::GetDefaultMusicDirectory() {
  //try to read XDG_MUSIC_DIR from user-dirs.dirs
  const char* home = getenv("HOME");
  if (!home) {
    return "/home/Music"; //fallback
  }

  std::string home_str(home);
  std::string user_dirs_path = home_str + "/.config/user-dirs.dirs";

  std::ifstream user_dirs_file(user_dirs_path);
  if (user_dirs_file.is_open()) {
    std::string line;
    while (std::getline(user_dirs_file, line)) {
      //look for XDG_MUSIC_DIR="..."
      if (line.find("XDG_MUSIC_DIR") != std::string::npos) {
        size_t start = line.find('"');
        size_t end = line.rfind('"');
        if (start != std::string::npos && end != std::string::npos && end > start) {
          std::string music_dir = line.substr(start + 1, end - start - 1);
          //replace $HOME with actual home directory
          size_t home_pos = music_dir.find("$HOME");
          if (home_pos != std::string::npos) {
            music_dir.replace(home_pos, 5, home_str);
          }
          return music_dir;
        }
      }
    }
  }

  //fallback to ~/Music
  return home_str + "/Music";
}

bool FileScanner::IsAudioFile(const std::string& filename) {
  //convert to lowercase
  std::string lower_filename = filename;
  std::transform(lower_filename.begin(), lower_filename.end(),
                lower_filename.begin(), ::tolower);

  //check for audio file extensions
  const std::vector<std::string> audio_extensions = {
    ".mp3", ".flac", ".ogg", ".m4a", ".wav", ".aac",
    ".wma", ".opus", ".ape", ".wv", ".oga", ".mpc"
  };

  for (const auto& ext : audio_extensions) {
    if (lower_filename.length() >= ext.length() &&
        lower_filename.compare(lower_filename.length() - ext.length(),
                              ext.length(), ext) == 0) {
      return true;
    }
  }

  return false;
}

void FileScanner::ScanDirectoryRecursive(const std::string& path,
                                        std::vector<std::string>& files) {
  DIR* dir = opendir(path.c_str());
  if (!dir) {
    std::cerr << "[FileScanner] Cannot open directory: " << path << std::endl;
    return;
  }

  struct dirent* entry;
  while ((entry = readdir(dir)) != nullptr) {
    //skip . and ..
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
      continue;
    }

    std::string full_path = path + "/" + entry->d_name;

    struct stat statbuf;
    if (stat(full_path.c_str(), &statbuf) == -1) {
      continue; //skip files that cant be stat-ed
    }

    if (S_ISDIR(statbuf.st_mode)) {
      //recursively scan subdirectories
      ScanDirectoryRecursive(full_path, files);
    } else if (S_ISREG(statbuf.st_mode)) {
      //check if audio file
      if (IsAudioFile(entry->d_name)) {
        files.push_back(full_path);
      }
    }
  }

  closedir(dir);
}

}  // namespace on_audio_query_linux
