#ifndef FILE_SCANNER_H_
#define FILE_SCANNER_H_

#include <string>
#include <vector>

namespace on_audio_query_linux {

class FileScanner {
 public:
  FileScanner();
  ~FileScanner();

  /// Scan a directory recursively for audio files
  std::vector<std::string> ScanDirectory(const std::string& path);

  /// Get default music directory (XDG_MUSIC_DIR or ~/Music)
  std::string GetDefaultMusicDirectory();

 private:
  bool IsAudioFile(const std::string& filename);
  void ScanDirectoryRecursive(const std::string& path,
                              std::vector<std::string>& files);
};

}  // namespace on_audio_query_linux

#endif  // FILE_SCANNER_H_
