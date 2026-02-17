#include "database_manager.h"
#include "../utils/string_utils.h"
#include "../utils/artist_separator.h"
#include <iostream>
#include <sstream>
#include <filesystem>
#include <cstring>
#include <sys/stat.h>

namespace on_audio_query_linux {

/// QueryParams helper implementations
std::string QueryParams::BuildWhereClause() const {
  std::vector<std::string> conditions;

  if (path_filter.has_value()) {
    conditions.push_back("file_path LIKE '" + path_filter.value() + "%'");
  }

  if (artist_filter.has_value()) {
    conditions.push_back("artist_id = " + std::to_string(artist_filter.value()));
  }

  if (album_filter.has_value()) {
    conditions.push_back("album_id = " + std::to_string(album_filter.value()));
  }

  if (genre_filter.has_value()) {
    conditions.push_back("genre_id = " + std::to_string(genre_filter.value()));
  }

  if (search_filter.has_value()) {
    std::string search = search_filter.value();
    conditions.push_back("(title LIKE '%" + search + "%' OR artist LIKE '%" + search + "%' OR album LIKE '%" + search + "%')");
  }

  if (conditions.empty()) {
    return "";
  }

  return StringUtils::Join(conditions, " AND ");
}

std::string QueryParams::BuildOrderByClause() const {
  std::string column;

  switch (sort_type) {
    case SortType::TITLE:
      column = "title";
      break;
    case SortType::ARTIST:
      column = "artist";
      break;
    case SortType::ALBUM:
      column = "album";
      break;
    case SortType::DURATION:
      column = "duration";
      break;
    case SortType::DATE_ADDED:
      column = "date_added";
      break;
    case SortType::SIZE:
      column = "file_size";
      break;
    case SortType::DISPLAY_NAME:
      column = "display_name";
      break;
  }

  //add COLLATE NOCASE for case-insensitive sorting on text columns
  if (ignore_case && (sort_type == SortType::TITLE || sort_type == SortType::ARTIST ||
                     sort_type == SortType::ALBUM || sort_type == SortType::DISPLAY_NAME)) {
    column += " COLLATE NOCASE";
  }

  column += (order_type == OrderType::ASC) ? " ASC" : " DESC";

  return column;
}

/// DatabaseManager implementation
DatabaseManager::DatabaseManager(const std::string& db_path)
    : db_(nullptr), db_path_(db_path) {}

DatabaseManager::~DatabaseManager() {
  Close();
}

bool DatabaseManager::Initialize() {
  std::lock_guard<std::mutex> lock(db_mutex_);

  //ensure directory exists
  std::filesystem::path db_file_path(db_path_);
  std::filesystem::create_directories(db_file_path.parent_path());

  bool is_first_run = !std::filesystem::exists(db_path_);

  int rc = sqlite3_open(db_path_.c_str(), &db_);
  if (rc != SQLITE_OK) {
    std::cerr << "[DatabaseManager] Cannot open database: " << sqlite3_errmsg(db_) << std::endl;
    return false;
  }

  //enable performance optimizations
  sqlite3_exec(db_, "PRAGMA journal_mode=WAL", nullptr, nullptr, nullptr);
  sqlite3_exec(db_, "PRAGMA synchronous=NORMAL", nullptr, nullptr, nullptr);
  sqlite3_exec(db_, "PRAGMA cache_size=10000", nullptr, nullptr, nullptr);
  sqlite3_exec(db_, "PRAGMA temp_store=MEMORY", nullptr, nullptr, nullptr);

  if (is_first_run) {
    std::cout << "[DatabaseManager] First run - creating database schema" << std::endl;
    if (!CreateTables() || !CreateIndexes()) {
      std::cerr << "[DatabaseManager] Failed to create schema" << std::endl;
      return false;
    }
  }

  return true;
}

bool DatabaseManager::CreateTables() {
  const char* songs_table = R"(
    CREATE TABLE IF NOT EXISTS songs (
      id INTEGER PRIMARY KEY,
      file_path TEXT NOT NULL UNIQUE,
      file_mtime INTEGER NOT NULL,
      file_size INTEGER NOT NULL,
      display_name TEXT NOT NULL,
      display_name_wo_ext TEXT NOT NULL,
      file_extension TEXT NOT NULL,
      uri TEXT NOT NULL,
      title TEXT,
      artist TEXT,
      album TEXT,
      genre TEXT,
      year INTEGER,
      track INTEGER,
      duration INTEGER,
      album_id INTEGER,
      artist_id INTEGER,
      genre_id INTEGER,
      date_added INTEGER,
      date_modified INTEGER,
      is_music INTEGER DEFAULT 1
    )
  )";

  const char* albums_table = R"(
    CREATE TABLE IF NOT EXISTS albums (
      id INTEGER PRIMARY KEY,
      album TEXT NOT NULL UNIQUE,
      artist TEXT,
      artist_id INTEGER,
      num_of_songs INTEGER DEFAULT 0,
      first_year INTEGER,
      last_year INTEGER
    )
  )";

  const char* artists_table = R"(
    CREATE TABLE IF NOT EXISTS artists (
      id INTEGER PRIMARY KEY,
      artist TEXT NOT NULL UNIQUE,
      number_of_albums INTEGER DEFAULT 0,
      number_of_tracks INTEGER DEFAULT 0
    )
  )";

  const char* genres_table = R"(
    CREATE TABLE IF NOT EXISTS genres (
      id INTEGER PRIMARY KEY,
      name TEXT NOT NULL UNIQUE,
      num_of_songs INTEGER DEFAULT 0
    )
  )";

  const char* playlists_table = R"(
    CREATE TABLE IF NOT EXISTS playlists (
      id INTEGER PRIMARY KEY AUTOINCREMENT,
      name TEXT NOT NULL UNIQUE,
      data TEXT,
      date_added INTEGER,
      date_modified INTEGER,
      num_of_songs INTEGER DEFAULT 0
    )
  )";

  const char* playlist_items_table = R"(
    CREATE TABLE IF NOT EXISTS playlist_items (
      id INTEGER PRIMARY KEY AUTOINCREMENT,
      playlist_id INTEGER NOT NULL,
      song_id INTEGER NOT NULL,
      position INTEGER NOT NULL,
      date_added INTEGER,
      FOREIGN KEY (playlist_id) REFERENCES playlists(id) ON DELETE CASCADE,
      FOREIGN KEY (song_id) REFERENCES songs(id) ON DELETE CASCADE,
      UNIQUE (playlist_id, song_id)
    )
  )";

  const char* artwork_cache_table = R"(
    CREATE TABLE IF NOT EXISTS artwork_cache (
      id INTEGER NOT NULL,
      type INTEGER NOT NULL,
      format TEXT NOT NULL,
      data BLOB,
      cached_at INTEGER,
      PRIMARY KEY (id, type, format)
    )
  )";

  char* err_msg = nullptr;

  if (sqlite3_exec(db_, songs_table, nullptr, nullptr, &err_msg) != SQLITE_OK ||
      sqlite3_exec(db_, albums_table, nullptr, nullptr, &err_msg) != SQLITE_OK ||
      sqlite3_exec(db_, artists_table, nullptr, nullptr, &err_msg) != SQLITE_OK ||
      sqlite3_exec(db_, genres_table, nullptr, nullptr, &err_msg) != SQLITE_OK ||
      sqlite3_exec(db_, playlists_table, nullptr, nullptr, &err_msg) != SQLITE_OK ||
      sqlite3_exec(db_, playlist_items_table, nullptr, nullptr, &err_msg) != SQLITE_OK ||
      sqlite3_exec(db_, artwork_cache_table, nullptr, nullptr, &err_msg) != SQLITE_OK) {
    std::cerr << "[DatabaseManager] Failed to create tables: " << err_msg << std::endl;
    sqlite3_free(err_msg);
    return false;
  }

  return true;
}

bool DatabaseManager::CreateIndexes() {
  const char* indexes[] = {
    "CREATE INDEX IF NOT EXISTS idx_songs_artist ON songs(artist_id)",
    "CREATE INDEX IF NOT EXISTS idx_songs_album ON songs(album_id)",
    "CREATE INDEX IF NOT EXISTS idx_songs_genre ON songs(genre_id)",
    "CREATE INDEX IF NOT EXISTS idx_songs_mtime ON songs(file_mtime)",
    "CREATE INDEX IF NOT EXISTS idx_songs_date_added ON songs(date_added)",
    "CREATE INDEX IF NOT EXISTS idx_songs_title ON songs(title COLLATE NOCASE)",
    "CREATE INDEX IF NOT EXISTS idx_songs_file_path ON songs(file_path)",
    "CREATE INDEX IF NOT EXISTS idx_playlist_items_playlist ON playlist_items(playlist_id, position)",
    "CREATE INDEX IF NOT EXISTS idx_playlist_items_song ON playlist_items(song_id)"
  };

  char* err_msg = nullptr;
  for (const char* index_sql : indexes) {
    if (sqlite3_exec(db_, index_sql, nullptr, nullptr, &err_msg) != SQLITE_OK) {
      std::cerr << "[DatabaseManager] Failed to create index: " << err_msg << std::endl;
      sqlite3_free(err_msg);
      return false;
    }
  }

  return true;
}

bool DatabaseManager::CheckIntegrity() {
  std::lock_guard<std::mutex> lock(db_mutex_);

  sqlite3_stmt* stmt;
  int rc = sqlite3_prepare_v2(db_, "PRAGMA integrity_check", -1, &stmt, nullptr);

  if (rc != SQLITE_OK) {
    return false;
  }

  bool is_ok = false;
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    const char* result = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
    is_ok = (strcmp(result, "ok") == 0);
  }

  sqlite3_finalize(stmt);
  return is_ok;
}

void DatabaseManager::Close() {
  std::lock_guard<std::mutex> lock(db_mutex_);

  ClearPreparedStatements();

  if (db_) {
    sqlite3_close(db_);
    db_ = nullptr;
  }
}

/// Song operations
bool DatabaseManager::InsertSong(const SongMetadata& song) {
  std::lock_guard<std::mutex> lock(db_mutex_);

  const char* sql = R"(
    INSERT OR REPLACE INTO songs (
      id, file_path, file_mtime, file_size, display_name, display_name_wo_ext,
      file_extension, uri, title, artist, album, genre, year, track, duration,
      album_id, artist_id, genre_id, date_added, date_modified, is_music
    ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
  )";

  sqlite3_stmt* stmt = GetPreparedStatement(sql);
  if (!stmt) return false;

  sqlite3_bind_int64(stmt, 1, song.id);
  sqlite3_bind_text(stmt, 2, song.data.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int64(stmt, 3, song.file_mtime);
  sqlite3_bind_int64(stmt, 4, song.size);
  sqlite3_bind_text(stmt, 5, song.display_name.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 6, song.display_name_wo_ext.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 7, song.file_extension.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 8, song.uri.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 9, song.title.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 10, song.artist.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 11, song.album.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 12, song.genre.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(stmt, 13, song.year);
  sqlite3_bind_int(stmt, 14, song.track);
  sqlite3_bind_int64(stmt, 15, song.duration);
  sqlite3_bind_int64(stmt, 16, song.album_id);
  sqlite3_bind_int64(stmt, 17, song.artist_id);
  sqlite3_bind_int64(stmt, 18, song.genre_id);
  sqlite3_bind_int64(stmt, 19, song.date_added);
  sqlite3_bind_int64(stmt, 20, song.date_modified);
  sqlite3_bind_int(stmt, 21, song.is_music ? 1 : 0);

  int rc = sqlite3_step(stmt);
  sqlite3_reset(stmt);

  return rc == SQLITE_DONE;
}

bool DatabaseManager::UpdateSong(const SongMetadata& song) {
  return InsertSong(song);  //INSERT OR REPLACE handles update
}

bool DatabaseManager::DeleteSong(int64_t song_id) {
  std::lock_guard<std::mutex> lock(db_mutex_);

  const char* sql = "DELETE FROM songs WHERE id = ?";
  sqlite3_stmt* stmt = GetPreparedStatement(sql);
  if (!stmt) return false;

  sqlite3_bind_int64(stmt, 1, song_id);

  int rc = sqlite3_step(stmt);
  sqlite3_reset(stmt);

  return rc == SQLITE_DONE;
}

bool DatabaseManager::DeleteSongByPath(const std::string& path) {
  std::lock_guard<std::mutex> lock(db_mutex_);

  const char* sql = "DELETE FROM songs WHERE file_path = ?";
  sqlite3_stmt* stmt = GetPreparedStatement(sql);
  if (!stmt) return false;

  sqlite3_bind_text(stmt, 1, path.c_str(), -1, SQLITE_TRANSIENT);

  int rc = sqlite3_step(stmt);
  sqlite3_reset(stmt);

  return rc == SQLITE_DONE;
}

std::vector<SongMetadata> DatabaseManager::QuerySongs(const QueryParams& params) {
  std::lock_guard<std::mutex> lock(db_mutex_);

  std::ostringstream query;
  query << "SELECT * FROM songs";

  std::string where_clause = params.BuildWhereClause();
  if (!where_clause.empty()) {
    query << " WHERE " << where_clause;
  }

  query << " ORDER BY " << params.BuildOrderByClause();

  if (params.limit.has_value()) {
    query << " LIMIT " << params.limit.value();
    if (params.offset.has_value()) {
      query << " OFFSET " << params.offset.value();
    }
  }

  sqlite3_stmt* stmt;
  if (sqlite3_prepare_v2(db_, query.str().c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
    std::cerr << "[DatabaseManager] Failed to prepare query: " << sqlite3_errmsg(db_) << std::endl;
    return {};
  }

  std::vector<SongMetadata> results;
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    results.push_back(ExtractSongFromStatement(stmt));
  }

  sqlite3_finalize(stmt);
  return results;
}

std::optional<SongMetadata> DatabaseManager::GetSongById(int64_t id) {
  std::lock_guard<std::mutex> lock(db_mutex_);

  const char* sql = "SELECT * FROM songs WHERE id = ?";
  sqlite3_stmt* stmt = GetPreparedStatement(sql);
  if (!stmt) return std::nullopt;

  sqlite3_bind_int64(stmt, 1, id);

  std::optional<SongMetadata> result;
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    result = ExtractSongFromStatement(stmt);
  }

  sqlite3_reset(stmt);
  return result;
}

std::optional<SongMetadata> DatabaseManager::GetSongByPath(const std::string& path) {
  std::lock_guard<std::mutex> lock(db_mutex_);

  const char* sql = "SELECT * FROM songs WHERE file_path = ?";
  sqlite3_stmt* stmt = GetPreparedStatement(sql);
  if (!stmt) return std::nullopt;

  sqlite3_bind_text(stmt, 1, path.c_str(), -1, SQLITE_TRANSIENT);

  std::optional<SongMetadata> result;
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    result = ExtractSongFromStatement(stmt);
  }

  sqlite3_reset(stmt);
  return result;
}

std::vector<std::string> DatabaseManager::GetAllSongPaths() {
  std::lock_guard<std::mutex> lock(db_mutex_);

  const char* sql = "SELECT file_path FROM songs ORDER BY file_path";
  sqlite3_stmt* stmt = GetPreparedStatement(sql);
  if (!stmt) return {};

  std::vector<std::string> paths;
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    paths.push_back(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)));
  }

  sqlite3_reset(stmt);
  return paths;
}

/// Album operations
std::vector<AlbumData> DatabaseManager::QueryAlbums(const QueryParams& params) {
  std::lock_guard<std::mutex> lock(db_mutex_);

  //Check if this is a split artist filter (negative ID)
  if (params.artist_filter.has_value() && params.artist_filter.value() < 0) {
    return QueryAlbumsForSplitArtist(params.artist_filter.value(), params);
  }

  std::ostringstream query;
  query << "SELECT id, album, artist, artist_id, num_of_songs, first_year, last_year FROM albums";

  //Add WHERE clause for artist filter
  if (params.artist_filter.has_value()) {
    query << " WHERE artist_id = " << params.artist_filter.value();
  }

  query << " ORDER BY album COLLATE NOCASE ";
  query << (params.order_type == QueryParams::OrderType::ASC ? "ASC" : "DESC");

  sqlite3_stmt* stmt;
  if (sqlite3_prepare_v2(db_, query.str().c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
    return {};
  }

  std::vector<AlbumData> results;
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    results.push_back(ExtractAlbumFromStatement(stmt));
  }

  sqlite3_finalize(stmt);
  return results;
}

std::optional<AlbumData> DatabaseManager::GetAlbumById(int64_t id) {
  std::lock_guard<std::mutex> lock(db_mutex_);

  const char* sql = "SELECT id, album, artist, artist_id, num_of_songs, first_year, last_year FROM albums WHERE id = ?";
  sqlite3_stmt* stmt = GetPreparedStatement(sql);
  if (!stmt) return std::nullopt;

  sqlite3_bind_int64(stmt, 1, id);

  std::optional<AlbumData> result;
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    result = ExtractAlbumFromStatement(stmt);
  }

  sqlite3_reset(stmt);
  return result;
}

/// Artist operations
std::vector<ArtistData> DatabaseManager::QueryArtists(const QueryParams& params) {
  std::lock_guard<std::mutex> lock(db_mutex_);

  std::ostringstream query;
  query << "SELECT id, artist, number_of_albums, number_of_tracks FROM artists";
  query << " ORDER BY artist COLLATE NOCASE ";
  query << (params.order_type == QueryParams::OrderType::ASC ? "ASC" : "DESC");

  sqlite3_stmt* stmt;
  if (sqlite3_prepare_v2(db_, query.str().c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
    return {};
  }

  std::vector<ArtistData> results;
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    results.push_back(ExtractArtistFromStatement(stmt));
  }

  sqlite3_finalize(stmt);
  return results;
}

std::optional<ArtistData> DatabaseManager::GetArtistById(int64_t id) {
  std::lock_guard<std::mutex> lock(db_mutex_);

  const char* sql = "SELECT id, artist, number_of_albums, number_of_tracks FROM artists WHERE id = ?";
  sqlite3_stmt* stmt = GetPreparedStatement(sql);
  if (!stmt) return std::nullopt;

  sqlite3_bind_int64(stmt, 1, id);

  std::optional<ArtistData> result;
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    result = ExtractArtistFromStatement(stmt);
  }

  sqlite3_reset(stmt);
  return result;
}

/// Genre operations
std::vector<GenreData> DatabaseManager::QueryGenres(const QueryParams& params) {
  std::lock_guard<std::mutex> lock(db_mutex_);

  std::ostringstream query;
  query << "SELECT id, name, num_of_songs FROM genres";
  query << " ORDER BY name COLLATE NOCASE ";
  query << (params.order_type == QueryParams::OrderType::ASC ? "ASC" : "DESC");

  sqlite3_stmt* stmt;
  if (sqlite3_prepare_v2(db_, query.str().c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
    return {};
  }

  std::vector<GenreData> results;
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    results.push_back(ExtractGenreFromStatement(stmt));
  }

  sqlite3_finalize(stmt);
  return results;
}

std::optional<GenreData> DatabaseManager::GetGenreById(int64_t id) {
  std::lock_guard<std::mutex> lock(db_mutex_);

  const char* sql = "SELECT id, name, num_of_songs FROM genres WHERE id = ?";
  sqlite3_stmt* stmt = GetPreparedStatement(sql);
  if (!stmt) return std::nullopt;

  sqlite3_bind_int64(stmt, 1, id);

  std::optional<GenreData> result;
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    result = ExtractGenreFromStatement(stmt);
  }

  sqlite3_reset(stmt);
  return result;
}

/// Playlist operations
int64_t DatabaseManager::CreatePlaylist(const std::string& name) {
  std::lock_guard<std::mutex> lock(db_mutex_);

  const char* sql = "INSERT INTO playlists (name, date_added, date_modified, num_of_songs) VALUES (?, ?, ?, 0)";
  sqlite3_stmt* stmt = GetPreparedStatement(sql);
  if (!stmt) return -1;

  int64_t now = time(nullptr) * 1000;
  sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int64(stmt, 2, now);
  sqlite3_bind_int64(stmt, 3, now);

  int rc = sqlite3_step(stmt);
  sqlite3_reset(stmt);

  if (rc == SQLITE_DONE) {
    return sqlite3_last_insert_rowid(db_);
  }

  return -1;
}

bool DatabaseManager::DeletePlaylist(int64_t playlist_id) {
  std::lock_guard<std::mutex> lock(db_mutex_);

  const char* sql = "DELETE FROM playlists WHERE id = ?";
  sqlite3_stmt* stmt = GetPreparedStatement(sql);
  if (!stmt) return false;

  sqlite3_bind_int64(stmt, 1, playlist_id);

  int rc = sqlite3_step(stmt);
  sqlite3_reset(stmt);

  return rc == SQLITE_DONE;
}

bool DatabaseManager::AddToPlaylist(int64_t playlist_id, int64_t song_id) {
  std::lock_guard<std::mutex> lock(db_mutex_);

  //get next position
  const char* pos_sql = "SELECT COALESCE(MAX(position), -1) + 1 FROM playlist_items WHERE playlist_id = ?";
  sqlite3_stmt* pos_stmt = GetPreparedStatement(pos_sql);
  if (!pos_stmt) return false;

  sqlite3_bind_int64(pos_stmt, 1, playlist_id);

  int position = 0;
  if (sqlite3_step(pos_stmt) == SQLITE_ROW) {
    position = sqlite3_column_int(pos_stmt, 0);
  }
  sqlite3_reset(pos_stmt);

  //insert item
  const char* sql = "INSERT OR IGNORE INTO playlist_items (playlist_id, song_id, position, date_added) VALUES (?, ?, ?, ?)";
  sqlite3_stmt* stmt = GetPreparedStatement(sql);
  if (!stmt) return false;

  int64_t now = time(nullptr) * 1000;
  sqlite3_bind_int64(stmt, 1, playlist_id);
  sqlite3_bind_int64(stmt, 2, song_id);
  sqlite3_bind_int(stmt, 3, position);
  sqlite3_bind_int64(stmt, 4, now);

  int rc = sqlite3_step(stmt);
  sqlite3_reset(stmt);

  //update playlist count
  if (rc == SQLITE_DONE) {
    const char* update_sql = "UPDATE playlists SET num_of_songs = num_of_songs + 1, date_modified = ? WHERE id = ?";
    sqlite3_stmt* update_stmt = GetPreparedStatement(update_sql);
    if (update_stmt) {
      sqlite3_bind_int64(update_stmt, 1, now);
      sqlite3_bind_int64(update_stmt, 2, playlist_id);
      sqlite3_step(update_stmt);
      sqlite3_reset(update_stmt);
    }
  }

  return rc == SQLITE_DONE;
}

bool DatabaseManager::RemoveFromPlaylist(int64_t playlist_id, int64_t song_id) {
  std::lock_guard<std::mutex> lock(db_mutex_);

  const char* sql = "DELETE FROM playlist_items WHERE playlist_id = ? AND song_id = ?";
  sqlite3_stmt* stmt = GetPreparedStatement(sql);
  if (!stmt) return false;

  sqlite3_bind_int64(stmt, 1, playlist_id);
  sqlite3_bind_int64(stmt, 2, song_id);

  int rc = sqlite3_step(stmt);
  sqlite3_reset(stmt);

  //update playlist count
  if (rc == SQLITE_DONE) {
    int64_t now = time(nullptr) * 1000;
    const char* update_sql = "UPDATE playlists SET num_of_songs = num_of_songs - 1, date_modified = ? WHERE id = ?";
    sqlite3_stmt* update_stmt = GetPreparedStatement(update_sql);
    if (update_stmt) {
      sqlite3_bind_int64(update_stmt, 1, now);
      sqlite3_bind_int64(update_stmt, 2, playlist_id);
      sqlite3_step(update_stmt);
      sqlite3_reset(update_stmt);
    }
  }

  return rc == SQLITE_DONE;
}

bool DatabaseManager::MovePlaylistItem(int64_t playlist_id, int from_pos, int to_pos) {
  std::lock_guard<std::mutex> lock(db_mutex_);

  //validate positions
  if (from_pos == to_pos || from_pos < 0 || to_pos < 0) {
    return false;
  }

  BeginTransaction();

  try {
    if (from_pos < to_pos) {
      //moving down: decrement positions between from and to
      const char* sql = R"(
        UPDATE playlist_items
        SET position = position - 1
        WHERE playlist_id = ?
          AND position > ?
          AND position <= ?
      )";
      sqlite3_stmt* stmt = GetPreparedStatement(sql);
      if (!stmt) {
        RollbackTransaction();
        return false;
      }
      sqlite3_bind_int64(stmt, 1, playlist_id);
      sqlite3_bind_int(stmt, 2, from_pos);
      sqlite3_bind_int(stmt, 3, to_pos);
      sqlite3_step(stmt);
      sqlite3_reset(stmt);
    } else {
      //moving up: increment positions between to and from
      const char* sql = R"(
        UPDATE playlist_items
        SET position = position + 1
        WHERE playlist_id = ?
          AND position >= ?
          AND position < ?
      )";
      sqlite3_stmt* stmt = GetPreparedStatement(sql);
      if (!stmt) {
        RollbackTransaction();
        return false;
      }
      sqlite3_bind_int64(stmt, 1, playlist_id);
      sqlite3_bind_int(stmt, 2, to_pos);
      sqlite3_bind_int(stmt, 3, from_pos);
      sqlite3_step(stmt);
      sqlite3_reset(stmt);
    }

    //move the item to its new position
    const char* move_sql = R"(
      UPDATE playlist_items
      SET position = ?
      WHERE playlist_id = ?
        AND position = ?
    )";
    sqlite3_stmt* move_stmt = GetPreparedStatement(move_sql);
    if (!move_stmt) {
      RollbackTransaction();
      return false;
    }
    sqlite3_bind_int(move_stmt, 1, to_pos);
    sqlite3_bind_int64(move_stmt, 2, playlist_id);
    sqlite3_bind_int(move_stmt, 3, from_pos < to_pos ? from_pos : from_pos + 1);

    int result = sqlite3_step(move_stmt);
    sqlite3_reset(move_stmt);

    if (result != SQLITE_DONE) {
      RollbackTransaction();
      return false;
    }

    //update playlist modification time
    const char* update_time_sql = R"(
      UPDATE playlists
      SET date_modified = ?
      WHERE id = ?
    )";
    sqlite3_stmt* time_stmt = GetPreparedStatement(update_time_sql);
    if (time_stmt) {
      sqlite3_bind_int64(time_stmt, 1, std::time(nullptr));
      sqlite3_bind_int64(time_stmt, 2, playlist_id);
      sqlite3_step(time_stmt);
      sqlite3_reset(time_stmt);
    }

    CommitTransaction();
    return true;
  } catch (...) {
    RollbackTransaction();
    return false;
  }
}

bool DatabaseManager::RenamePlaylist(int64_t playlist_id, const std::string& new_name) {
  std::lock_guard<std::mutex> lock(db_mutex_);

  const char* sql = "UPDATE playlists SET name = ?, date_modified = ? WHERE id = ?";
  sqlite3_stmt* stmt = GetPreparedStatement(sql);
  if (!stmt) return false;

  int64_t now = time(nullptr) * 1000;
  sqlite3_bind_text(stmt, 1, new_name.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int64(stmt, 2, now);
  sqlite3_bind_int64(stmt, 3, playlist_id);

  int rc = sqlite3_step(stmt);
  sqlite3_reset(stmt);

  return rc == SQLITE_DONE;
}

std::vector<PlaylistData> DatabaseManager::QueryPlaylists() {
  std::lock_guard<std::mutex> lock(db_mutex_);

  const char* sql = "SELECT id, name, data, date_added, date_modified, num_of_songs FROM playlists ORDER BY name";
  sqlite3_stmt* stmt = GetPreparedStatement(sql);
  if (!stmt) return {};

  std::vector<PlaylistData> results;
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    results.push_back(ExtractPlaylistFromStatement(stmt));
  }

  sqlite3_reset(stmt);
  return results;
}

std::vector<SongMetadata> DatabaseManager::GetPlaylistSongs(int64_t playlist_id) {
  std::lock_guard<std::mutex> lock(db_mutex_);

  const char* sql = R"(
    SELECT s.* FROM songs s
    JOIN playlist_items pi ON s.id = pi.song_id
    WHERE pi.playlist_id = ?
    ORDER BY pi.position
  )";

  sqlite3_stmt* stmt = GetPreparedStatement(sql);
  if (!stmt) return {};

  sqlite3_bind_int64(stmt, 1, playlist_id);

  std::vector<SongMetadata> results;
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    results.push_back(ExtractSongFromStatement(stmt));
  }

  sqlite3_reset(stmt);
  return results;
}

/// Artwork cache
bool DatabaseManager::CacheArtwork(int64_t id, int type, const std::string& format,
                                   const std::vector<uint8_t>& data) {
  std::lock_guard<std::mutex> lock(db_mutex_);

  const char* sql = "INSERT OR REPLACE INTO artwork_cache (id, type, format, data, cached_at) VALUES (?, ?, ?, ?, ?)";
  sqlite3_stmt* stmt = GetPreparedStatement(sql);
  if (!stmt) return false;

  int64_t now = time(nullptr);
  sqlite3_bind_int64(stmt, 1, id);
  sqlite3_bind_int(stmt, 2, type);
  sqlite3_bind_text(stmt, 3, format.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_blob(stmt, 4, data.data(), data.size(), SQLITE_TRANSIENT);
  sqlite3_bind_int64(stmt, 5, now);

  int rc = sqlite3_step(stmt);
  sqlite3_reset(stmt);

  return rc == SQLITE_DONE;
}

std::optional<std::vector<uint8_t>> DatabaseManager::GetCachedArtwork(int64_t id, int type,
                                                                       const std::string& format) {
  std::lock_guard<std::mutex> lock(db_mutex_);

  const char* sql = "SELECT data FROM artwork_cache WHERE id = ? AND type = ? AND format = ?";
  sqlite3_stmt* stmt = GetPreparedStatement(sql);
  if (!stmt) return std::nullopt;

  sqlite3_bind_int64(stmt, 1, id);
  sqlite3_bind_int(stmt, 2, type);
  sqlite3_bind_text(stmt, 3, format.c_str(), -1, SQLITE_TRANSIENT);

  std::optional<std::vector<uint8_t>> result;
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    const void* blob = sqlite3_column_blob(stmt, 0);
    int blob_size = sqlite3_column_bytes(stmt, 0);

    if (blob && blob_size > 0) {
      const uint8_t* data = static_cast<const uint8_t*>(blob);
      result = std::vector<uint8_t>(data, data + blob_size);
    }
  }

  sqlite3_reset(stmt);
  return result;
}

/// Transaction support
void DatabaseManager::BeginTransaction() {
  std::lock_guard<std::mutex> lock(db_mutex_);
  sqlite3_exec(db_, "BEGIN TRANSACTION", nullptr, nullptr, nullptr);
}

void DatabaseManager::CommitTransaction() {
  std::lock_guard<std::mutex> lock(db_mutex_);
  sqlite3_exec(db_, "COMMIT", nullptr, nullptr, nullptr);
}

void DatabaseManager::RollbackTransaction() {
  std::lock_guard<std::mutex> lock(db_mutex_);
  sqlite3_exec(db_, "ROLLBACK", nullptr, nullptr, nullptr);
}

/// Aggregation updates
void DatabaseManager::UpdateAggregatedTables() {
  std::lock_guard<std::mutex> lock(db_mutex_);

  //update albums
  const char* albums_sql = R"(
    INSERT OR REPLACE INTO albums (id, album, artist, artist_id, num_of_songs, first_year, last_year)
    SELECT
      album_id,
      album,
      artist,
      artist_id,
      COUNT(*) as num_of_songs,
      MIN(CASE WHEN year > 0 THEN year ELSE NULL END) as first_year,
      MAX(year) as last_year
    FROM songs
    GROUP BY album_id
  )";

  //update genres
  const char* genres_sql = R"(
    INSERT OR REPLACE INTO genres (id, name, num_of_songs)
    SELECT
      genre_id,
      genre,
      COUNT(*) as num_of_songs
    FROM songs
    GROUP BY genre_id
  )";

  sqlite3_exec(db_, albums_sql, nullptr, nullptr, nullptr);
  sqlite3_exec(db_, genres_sql, nullptr, nullptr, nullptr);

  //update artists with splitting
  UpdateArtistsWithSplitting();
}

/// Utility
bool DatabaseManager::IsDatabaseEmpty() {
  std::lock_guard<std::mutex> lock(db_mutex_);

  const char* sql = "SELECT COUNT(*) FROM songs";
  sqlite3_stmt* stmt = GetPreparedStatement(sql);
  if (!stmt) return true;

  int count = 0;
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    count = sqlite3_column_int(stmt, 0);
  }

  sqlite3_reset(stmt);
  return count == 0;
}

int64_t DatabaseManager::GetSongCount() {
  std::lock_guard<std::mutex> lock(db_mutex_);

  const char* sql = "SELECT COUNT(*) FROM songs";
  sqlite3_stmt* stmt = GetPreparedStatement(sql);
  if (!stmt) return 0;

  int64_t count = 0;
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    count = sqlite3_column_int64(stmt, 0);
  }

  sqlite3_reset(stmt);
  return count;
}

/// Helper functions
sqlite3_stmt* DatabaseManager::GetPreparedStatement(const std::string& query) {
  auto it = prepared_stmts_.find(query);
  if (it != prepared_stmts_.end()) {
    sqlite3_reset(it->second);
    sqlite3_clear_bindings(it->second);
    return it->second;
  }

  sqlite3_stmt* stmt;
  int rc = sqlite3_prepare_v2(db_, query.c_str(), -1, &stmt, nullptr);
  if (rc != SQLITE_OK) {
    std::cerr << "[DatabaseManager] Failed to prepare statement: " << sqlite3_errmsg(db_) << std::endl;
    return nullptr;
  }

  prepared_stmts_[query] = stmt;
  return stmt;
}

void DatabaseManager::ClearPreparedStatements() {
  for (auto& pair : prepared_stmts_) {
    sqlite3_finalize(pair.second);
  }
  prepared_stmts_.clear();
}

SongMetadata DatabaseManager::ExtractSongFromStatement(sqlite3_stmt* stmt) {
  SongMetadata song;

  song.id = sqlite3_column_int64(stmt, 0);
  song.data = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
  song.file_mtime = sqlite3_column_int64(stmt, 2);
  song.size = sqlite3_column_int64(stmt, 3);
  song.display_name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
  song.display_name_wo_ext = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
  song.file_extension = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
  song.uri = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
  song.title = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 8));
  song.artist = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 9));
  song.album = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 10));
  song.genre = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 11));
  song.year = sqlite3_column_int(stmt, 12);
  song.track = sqlite3_column_int(stmt, 13);
  song.duration = sqlite3_column_int64(stmt, 14);
  song.album_id = sqlite3_column_int64(stmt, 15);
  song.artist_id = sqlite3_column_int64(stmt, 16);
  song.genre_id = sqlite3_column_int64(stmt, 17);
  song.date_added = sqlite3_column_int64(stmt, 18);
  song.date_modified = sqlite3_column_int64(stmt, 19);
  song.is_music = sqlite3_column_int(stmt, 20) != 0;

  return song;
}

AlbumData DatabaseManager::ExtractAlbumFromStatement(sqlite3_stmt* stmt) {
  AlbumData album;

  album.id = sqlite3_column_int64(stmt, 0);
  album.album = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));

  const char* artist_text = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
  album.artist = artist_text ? artist_text : "";

  album.artist_id = sqlite3_column_int64(stmt, 3);
  album.num_of_songs = sqlite3_column_int(stmt, 4);
  album.first_year = sqlite3_column_int(stmt, 5);
  album.last_year = sqlite3_column_int(stmt, 6);

  return album;
}

ArtistData DatabaseManager::ExtractArtistFromStatement(sqlite3_stmt* stmt) {
  ArtistData artist;

  artist.id = sqlite3_column_int64(stmt, 0);
  artist.artist = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
  artist.number_of_albums = sqlite3_column_int(stmt, 2);
  artist.number_of_tracks = sqlite3_column_int(stmt, 3);

  return artist;
}

GenreData DatabaseManager::ExtractGenreFromStatement(sqlite3_stmt* stmt) {
  GenreData genre;

  genre.id = sqlite3_column_int64(stmt, 0);
  genre.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
  genre.num_of_songs = sqlite3_column_int(stmt, 2);

  return genre;
}

PlaylistData DatabaseManager::ExtractPlaylistFromStatement(sqlite3_stmt* stmt) {
  PlaylistData playlist;

  playlist.id = sqlite3_column_int64(stmt, 0);
  playlist.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));

  const char* data_text = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
  playlist.data = data_text ? data_text : "";

  playlist.date_added = sqlite3_column_int64(stmt, 3);
  playlist.date_modified = sqlite3_column_int64(stmt, 4);
  playlist.num_of_songs = sqlite3_column_int(stmt, 5);

  return playlist;
}

std::vector<AlbumData> DatabaseManager::QueryAlbumsForSplitArtist(int64_t split_artist_id, const QueryParams& params) {
  //This method queries albums for a split artist (negative ID)
  //Strategy: Find all songs by this split artist, extract unique album_ids, then query those albums

  auto& separator = ArtistSeparator::Instance();

  //Get artist name from ID mapping
  std::string artist_name = separator.GetArtistNameById(split_artist_id);
  if (artist_name.empty()) {
    std::cerr << "[DatabaseManager] Could not find artist name for split artist ID: " << split_artist_id << std::endl;
    return {};
  }

  std::cout << "[DatabaseManager] Querying albums for split artist: " << artist_name
            << " (ID: " << split_artist_id << ")" << std::endl;

  //Get all combined artist strings that contain this artist
  auto combined_artists = separator.GetCombinedArtistsFor(artist_name);

  //Collect unique album IDs from songs by this artist
  std::set<int64_t> album_ids;

  if (combined_artists.empty()) {
    //Query songs where artist matches exactly
    const char* sql = "SELECT DISTINCT album_id FROM songs WHERE artist = ?";
    sqlite3_stmt* stmt = GetPreparedStatement(sql);
    if (stmt) {
      sqlite3_bind_text(stmt, 1, artist_name.c_str(), -1, SQLITE_TRANSIENT);
      while (sqlite3_step(stmt) == SQLITE_ROW) {
        album_ids.insert(sqlite3_column_int64(stmt, 0));
      }
      sqlite3_reset(stmt);
    }
  } else {
    //Query songs for each combined artist string
    const char* sql = "SELECT DISTINCT album_id FROM songs WHERE artist = ?";
    sqlite3_stmt* stmt = GetPreparedStatement(sql);
    if (stmt) {
      for (const auto& combined_artist : combined_artists) {
        sqlite3_bind_text(stmt, 1, combined_artist.c_str(), -1, SQLITE_TRANSIENT);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
          album_ids.insert(sqlite3_column_int64(stmt, 0));
        }
        sqlite3_reset(stmt);
      }
    }
  }

  if (album_ids.empty()) {
    return {};
  }

  //Query albums by collected album_ids
  std::vector<AlbumData> results;
  for (int64_t album_id : album_ids) {
    auto album = GetAlbumById(album_id);
    if (album.has_value()) {
      results.push_back(album.value());
    }
  }

  //Sort results
  std::sort(results.begin(), results.end(),
            [&params](const AlbumData& a, const AlbumData& b) {
              bool ascending = (params.order_type == QueryParams::OrderType::ASC);
              int cmp = strcasecmp(a.album.c_str(), b.album.c_str());
              return ascending ? (cmp < 0) : (cmp > 0);
            });

  std::cout << "[DatabaseManager] Found " << results.size() << " albums for split artist: " << artist_name << std::endl;

  return results;
}

void DatabaseManager::UpdateArtistsWithSplitting() {
  std::cout << "[DatabaseManager] Updating artists table with artist splitting..." << std::endl;

  auto& separator = ArtistSeparator::Instance();
  separator.ClearIndex();

  //Clear artists table
  sqlite3_exec(db_, "DELETE FROM artists", nullptr, nullptr, nullptr);

  //Get all unique artists from songs with their metadata
  const char* query_sql = R"(
    SELECT
      artist_id,
      artist,
      COUNT(DISTINCT album_id) as number_of_albums,
      COUNT(*) as number_of_tracks
    FROM songs
    GROUP BY artist_id
  )";

  sqlite3_stmt* stmt;
  if (sqlite3_prepare_v2(db_, query_sql, -1, &stmt, nullptr) != SQLITE_OK) {
    std::cerr << "[DatabaseManager] Failed to query artists: " << sqlite3_errmsg(db_) << std::endl;
    return;
  }

  //Build MediaStore ID lookup for single (non-combined) artists
  std::map<std::string, int64_t> mediastore_id_lookup;
  std::vector<ArtistData> raw_artists;

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    ArtistData artist;
    artist.id = sqlite3_column_int64(stmt, 0);
    artist.artist = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
    artist.number_of_albums = sqlite3_column_int(stmt, 2);
    artist.number_of_tracks = sqlite3_column_int(stmt, 3);

    raw_artists.push_back(artist);

    //Check if this is a single artist (not combined)
    auto split_check = separator.SplitArtistString(artist.artist);
    if (split_check.size() == 1) {
      std::string artist_lower = artist.artist;
      std::transform(artist_lower.begin(), artist_lower.end(), artist_lower.begin(), ::tolower);
      mediastore_id_lookup[artist_lower] = artist.id;
    }
  }

  sqlite3_finalize(stmt);

  //Map to track seen artists (key = lowercase artist name)
  std::map<std::string, ArtistData> seen_artists;

  //Process each raw artist
  for (const auto& artist_data : raw_artists) {
    auto split_artists = separator.SplitArtistString(artist_data.artist);
    int64_t original_id = artist_data.id;
    int num_albums = artist_data.number_of_albums;
    int num_tracks = artist_data.number_of_tracks;

    //Build split artist index if this is a combined artist
    if (split_artists.size() > 1) {
      for (const auto& artist_name : split_artists) {
        separator.AddToIndex(artist_name, artist_data.artist);
      }
    }

    //Process each split artist
    for (const auto& artist_name : split_artists) {
      std::string artist_key = artist_name;
      std::transform(artist_key.begin(), artist_key.end(), artist_key.begin(), ::tolower);

      if (seen_artists.find(artist_key) != seen_artists.end()) {
        //Artist already exists => merge counts
        auto& existing_artist = seen_artists[artist_key];
        existing_artist.number_of_albums += num_albums;
        existing_artist.number_of_tracks += num_tracks;
      } else {
        //New artist => create entry
        ArtistData split_data;
        split_data.artist = artist_name;

        //Determine ID: use MediaStore ID if available, otherwise generate
        int64_t artist_id;
        if (mediastore_id_lookup.find(artist_key) != mediastore_id_lookup.end()) {
          artist_id = mediastore_id_lookup[artist_key];
        } else if (split_artists.size() == 1) {
          artist_id = original_id;
        } else {
          artist_id = separator.GenerateSplitArtistId(artist_name);
        }

        split_data.id = artist_id;

        //Add ID-to-name mapping for split artists
        if (artist_id < 0) {
          separator.AddIdMapping(artist_id, artist_name);
          std::cout << "[DatabaseManager] Added ID mapping: " << artist_id << " -> " << artist_name << std::endl;
        }

        split_data.number_of_albums = num_albums;
        split_data.number_of_tracks = num_tracks;

        seen_artists[artist_key] = split_data;
      }
    }
  }

  std::cout << "[DatabaseManager] Split artist count: " << seen_artists.size()
            << " (from " << raw_artists.size() << " raw entries)" << std::endl;

  //Insert split artists into database
  const char* insert_sql = "INSERT INTO artists (id, artist, number_of_albums, number_of_tracks) VALUES (?, ?, ?, ?)";
  sqlite3_stmt* insert_stmt = GetPreparedStatement(insert_sql);

  if (insert_stmt) {
    for (const auto& [key, artist] : seen_artists) {
      sqlite3_bind_int64(insert_stmt, 1, artist.id);
      sqlite3_bind_text(insert_stmt, 2, artist.artist.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_int(insert_stmt, 3, artist.number_of_albums);
      sqlite3_bind_int(insert_stmt, 4, artist.number_of_tracks);

      if (sqlite3_step(insert_stmt) != SQLITE_DONE) {
        std::cerr << "[DatabaseManager] Failed to insert artist: " << sqlite3_errmsg(db_) << std::endl;
      }

      sqlite3_reset(insert_stmt);
    }
  }

  std::cout << "[DatabaseManager] Artists table updated with " << seen_artists.size() << " artists" << std::endl;
}

}  // namespace on_audio_query_linux
