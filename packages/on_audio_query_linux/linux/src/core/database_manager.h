#ifndef DATABASE_MANAGER_H_
#define DATABASE_MANAGER_H_

#include <string>
#include <vector>
#include <map>
#include <optional>
#include <mutex>
#include <sqlite3.h>
#include "../models/song_metadata.h"

namespace on_audio_query_linux {

/// Album data (aggregated from songs)
struct AlbumData {
  int64_t id;
  std::string album;
  std::string artist;
  int64_t artist_id;
  int num_of_songs;
  int first_year;
  int last_year;
};

/// Artist data (aggregated from songs)
struct ArtistData {
  int64_t id;
  std::string artist;
  int number_of_albums;
  int number_of_tracks;
};

/// Genre data (aggregated from songs)
struct GenreData {
  int64_t id;
  std::string name;
  int num_of_songs;
};

/// Playlist data
struct PlaylistData {
  int64_t id;
  std::string name;
  std::string data;
  int64_t date_added;
  int64_t date_modified;
  int num_of_songs;
};

/// Query parameters for sorting and filtering
struct QueryParams {
  enum class SortType {
    TITLE, ARTIST, ALBUM, DURATION, DATE_ADDED, SIZE, DISPLAY_NAME
  };

  enum class OrderType {
    ASC, DESC
  };

  SortType sort_type = SortType::TITLE;
  OrderType order_type = OrderType::ASC;
  bool ignore_case = true;

  /// Filters
  std::optional<std::string> path_filter;
  std::optional<int64_t> artist_filter;
  std::optional<int64_t> album_filter;
  std::optional<int64_t> genre_filter;
  std::optional<std::string> search_filter;

  /// Pagination
  std::optional<int> limit;
  std::optional<int> offset;

  std::string BuildWhereClause() const;
  std::string BuildOrderByClause() const;
};

class DatabaseManager {
 public:
  explicit DatabaseManager(const std::string& db_path);
  ~DatabaseManager();

  /// Database lifecycle
  bool Initialize();
  bool CheckIntegrity();
  void Close();

  /// Song operations
  bool InsertSong(const SongMetadata& song);
  bool UpdateSong(const SongMetadata& song);
  bool DeleteSong(int64_t song_id);
  bool DeleteSongByPath(const std::string& path);
  std::vector<SongMetadata> QuerySongs(const QueryParams& params = QueryParams{});
  std::optional<SongMetadata> GetSongById(int64_t id);
  std::optional<SongMetadata> GetSongByPath(const std::string& path);
  std::vector<std::string> GetAllSongPaths();

  /// Album operations
  std::vector<AlbumData> QueryAlbums(const QueryParams& params = QueryParams{});
  std::optional<AlbumData> GetAlbumById(int64_t id);

  /// Artist operations
  std::vector<ArtistData> QueryArtists(const QueryParams& params = QueryParams{});
  std::optional<ArtistData> GetArtistById(int64_t id);

  /// Genre operations
  std::vector<GenreData> QueryGenres(const QueryParams& params = QueryParams{});
  std::optional<GenreData> GetGenreById(int64_t id);

  /// Playlist operations
  int64_t CreatePlaylist(const std::string& name);
  bool DeletePlaylist(int64_t playlist_id);
  bool AddToPlaylist(int64_t playlist_id, int64_t song_id);
  bool RemoveFromPlaylist(int64_t playlist_id, int64_t song_id);
  bool MovePlaylistItem(int64_t playlist_id, int from_pos, int to_pos);
  bool RenamePlaylist(int64_t playlist_id, const std::string& new_name);
  std::vector<PlaylistData> QueryPlaylists();
  std::vector<SongMetadata> GetPlaylistSongs(int64_t playlist_id);

  /// Artwork cache
  bool CacheArtwork(int64_t id, int type, const std::string& format,
                    const std::vector<uint8_t>& data);
  std::optional<std::vector<uint8_t>> GetCachedArtwork(int64_t id, int type,
                                                        const std::string& format);

  /// Transaction support
  void BeginTransaction();
  void CommitTransaction();
  void RollbackTransaction();

  /// Aggregation updates
  void UpdateAggregatedTables();

  /// Utility
  bool IsDatabaseEmpty();
  int64_t GetSongCount();

 private:
  sqlite3* db_;
  std::string db_path_;
  mutable std::mutex db_mutex_;

  /// Prepared statements cache
  std::map<std::string, sqlite3_stmt*> prepared_stmts_;

  bool CreateTables();
  bool CreateIndexes();
  sqlite3_stmt* GetPreparedStatement(const std::string& query);
  void ClearPreparedStatements();

  /// Helper functions
  SongMetadata ExtractSongFromStatement(sqlite3_stmt* stmt);
  AlbumData ExtractAlbumFromStatement(sqlite3_stmt* stmt);
  ArtistData ExtractArtistFromStatement(sqlite3_stmt* stmt);
  GenreData ExtractGenreFromStatement(sqlite3_stmt* stmt);
  PlaylistData ExtractPlaylistFromStatement(sqlite3_stmt* stmt);

  /// Split artist query helpers
  std::vector<AlbumData> QueryAlbumsForSplitArtist(int64_t split_artist_id, const QueryParams& params);
  void UpdateArtistsWithSplitting();
};

}  // namespace on_audio_query_linux

#endif  // DATABASE_MANAGER_H_
