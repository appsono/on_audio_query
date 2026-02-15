#include "base_query.h"

namespace on_audio_query_linux {

BaseQuery::BaseQuery(DatabaseManager* db_manager)
    : db_manager_(db_manager) {}

FlValue* BaseQuery::SongToFlValue(const SongMetadata& song) {
  FlValue* song_map = fl_value_new_map();

  fl_value_set_string_take(song_map, "_id",
                          fl_value_new_int(song.id));
  fl_value_set_string_take(song_map, "_data",
                          fl_value_new_string(song.data.c_str()));
  fl_value_set_string_take(song_map, "_uri",
                          fl_value_new_string(song.uri.c_str()));
  fl_value_set_string_take(song_map, "_display_name",
                          fl_value_new_string(song.display_name.c_str()));
  fl_value_set_string_take(song_map, "_display_name_wo_ext",
                          fl_value_new_string(song.display_name_wo_ext.c_str()));
  fl_value_set_string_take(song_map, "_size",
                          fl_value_new_int(song.size));
  fl_value_set_string_take(song_map, "album",
                          fl_value_new_string(song.album.c_str()));
  fl_value_set_string_take(song_map, "album_id",
                          fl_value_new_int(song.album_id));
  fl_value_set_string_take(song_map, "artist",
                          fl_value_new_string(song.artist.c_str()));
  fl_value_set_string_take(song_map, "artist_id",
                          fl_value_new_int(song.artist_id));
  fl_value_set_string_take(song_map, "date_added",
                          fl_value_new_int(song.date_added));
  fl_value_set_string_take(song_map, "date_modified",
                          fl_value_new_int(song.date_modified));
  fl_value_set_string_take(song_map, "duration",
                          fl_value_new_int(song.duration));
  fl_value_set_string_take(song_map, "title",
                          fl_value_new_string(song.title.c_str()));
  fl_value_set_string_take(song_map, "track",
                          fl_value_new_int(song.track));
  fl_value_set_string_take(song_map, "year",
                          fl_value_new_int(song.year));
  fl_value_set_string_take(song_map, "genre",
                          fl_value_new_string(song.genre.c_str()));
  fl_value_set_string_take(song_map, "genre_id",
                          fl_value_new_int(song.genre_id));
  fl_value_set_string_take(song_map, "file_extension",
                          fl_value_new_string(song.file_extension.c_str()));
  fl_value_set_string_take(song_map, "is_music",
                          fl_value_new_bool(song.is_music));

  return song_map;
}

FlValue* BaseQuery::AlbumToFlValue(const AlbumData& album) {
  FlValue* album_map = fl_value_new_map();

  fl_value_set_string_take(album_map, "_id",
                          fl_value_new_int(album.id));
  fl_value_set_string_take(album_map, "album",
                          fl_value_new_string(album.album.c_str()));
  fl_value_set_string_take(album_map, "artist",
                          fl_value_new_string(album.artist.c_str()));
  fl_value_set_string_take(album_map, "artist_id",
                          fl_value_new_int(album.artist_id));
  fl_value_set_string_take(album_map, "numsongs",
                          fl_value_new_int(album.num_of_songs));
  fl_value_set_string_take(album_map, "first_year",
                          fl_value_new_int(album.first_year));
  fl_value_set_string_take(album_map, "last_year",
                          fl_value_new_int(album.last_year));

  return album_map;
}

FlValue* BaseQuery::ArtistToFlValue(const ArtistData& artist) {
  FlValue* artist_map = fl_value_new_map();

  fl_value_set_string_take(artist_map, "_id",
                          fl_value_new_int(artist.id));
  fl_value_set_string_take(artist_map, "artist",
                          fl_value_new_string(artist.artist.c_str()));
  fl_value_set_string_take(artist_map, "number_of_albums",
                          fl_value_new_int(artist.number_of_albums));
  fl_value_set_string_take(artist_map, "number_of_tracks",
                          fl_value_new_int(artist.number_of_tracks));

  return artist_map;
}

FlValue* BaseQuery::GenreToFlValue(const GenreData& genre) {
  FlValue* genre_map = fl_value_new_map();

  fl_value_set_string_take(genre_map, "_id",
                          fl_value_new_int(genre.id));
  fl_value_set_string_take(genre_map, "genre",
                          fl_value_new_string(genre.name.c_str()));
  fl_value_set_string_take(genre_map, "num_of_songs",
                          fl_value_new_int(genre.num_of_songs));

  return genre_map;
}

FlValue* BaseQuery::PlaylistToFlValue(const PlaylistData& playlist) {
  FlValue* playlist_map = fl_value_new_map();

  fl_value_set_string_take(playlist_map, "_id",
                          fl_value_new_int(playlist.id));
  fl_value_set_string_take(playlist_map, "playlist",
                          fl_value_new_string(playlist.name.c_str()));
  fl_value_set_string_take(playlist_map, "data",
                          fl_value_new_string(playlist.data.c_str()));
  fl_value_set_string_take(playlist_map, "date_added",
                          fl_value_new_int(playlist.date_added));
  fl_value_set_string_take(playlist_map, "date_modified",
                          fl_value_new_int(playlist.date_modified));
  fl_value_set_string_take(playlist_map, "num_of_songs",
                          fl_value_new_int(playlist.num_of_songs));

  return playlist_map;
}

}  // namespace on_audio_query_linux
