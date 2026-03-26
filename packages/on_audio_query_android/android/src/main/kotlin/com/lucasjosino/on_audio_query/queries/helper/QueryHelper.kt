package com.lucasjosino.on_audio_query.queries.helper

import android.content.ContentResolver
import android.content.ContentUris
import android.database.Cursor
import android.net.Uri
import android.os.Build
import android.provider.MediaStore
import android.util.Log
import java.io.File
import java.nio.charset.Charset

class QueryHelper {

    companion object {
        private val CP1252: Charset = Charset.forName("windows-1252")

        /**
         * Fixes UTF-8 mojibake in metadata strings.
         *
         * Some ID3 tag writers store UTF-8 bytes but mark the encoding as
         * Latin-1/CP1252. Android's MediaStore reads the bytes using that
         * declared encoding, producing garbled text like "Donâ€™t" instead
         * of "Don't".
         *
         * This re-encodes the string back to CP1252 bytes and decodes as
         * UTF-8. If that produces a valid, shorter string it was mojibake.
         * Otherwise the original is returned unchanged.
         */
        fun fixMojibake(input: String?): String? {
            if (input == null) return null

            return try {
                val bytes = input.toByteArray(CP1252)
                val decoded = String(bytes, Charsets.UTF_8)

                // Mojibake always expands: multi-byte UTF-8 sequences become
                // multiple single-byte CP1252 chars. A valid fix will be
                // shorter and contain no replacement characters.
                if (!decoded.contains('\uFFFD') && decoded.length < input.length) {
                    decoded
                } else {
                    input
                }
            } catch (_: Exception) {
                input
            }
        }
    }
    //This method will load some extra information about audio/song
    fun loadSongExtraInfo(
        uri: Uri,
        songData: MutableMap<String, Any?>
    ): MutableMap<String, Any?> {
        val file = File(songData["_data"].toString())

        //Getting displayName without [Extension].
        songData["_display_name_wo_ext"] = file.nameWithoutExtension
        //Adding only the extension
        songData["file_extension"] = file.extension

        //A different type of "data"
        val tempUri = ContentUris.withAppendedId(uri, songData["_id"].toString().toLong())
        songData["_uri"] = tempUri.toString()

        return songData
    }

    //This method will separate [String] from [Int]
    fun loadSongItem(itemProperty: String, cursor: Cursor): Any? {
        return when (itemProperty) {
            // Int
            "_id",
            "album_id",
            "artist_id" -> {
                // The [id] from Android >= 30/R is a [Long] instead of [Int].
                if (Build.VERSION.SDK_INT >= 30) {
                    cursor.getLong(cursor.getColumnIndex(itemProperty))
                } else {
                    cursor.getInt(cursor.getColumnIndex(itemProperty))
                }
            }
            "_size",
            "bookmark",
            "date_added",
            "date_modified",
            "duration",
            "track" -> cursor.getInt(cursor.getColumnIndex(itemProperty))
            // Boolean
            "is_alarm",
            "is_audiobook",
            "is_music",
            "is_notification",
            "is_podcast",
            "is_ringtone" -> {
                val value = cursor.getString(cursor.getColumnIndex(itemProperty))
                if (value == "0") return false
                return true
            }
            // String (fix mojibake from misencoded ID3 tags)
            else -> fixMojibake(cursor.getString(cursor.getColumnIndex(itemProperty)))
        }
    }

    //This method will separate [String] from [Int]
    fun loadAlbumItem(itemProperty: String, cursor: Cursor): Any? {
        return when (itemProperty) {
            "_id",
            "artist_id" -> {
                // The [album] id from Android >= 30/R is a [Long] instead of [Int].
                if (Build.VERSION.SDK_INT >= 30) {
                    cursor.getLong(cursor.getColumnIndex(itemProperty))
                } else {
                    cursor.getInt(cursor.getColumnIndex(itemProperty))
                }
            }
            "numsongs" -> cursor.getInt(cursor.getColumnIndex(itemProperty))
            else -> fixMojibake(cursor.getString(cursor.getColumnIndex(itemProperty)))
        }
    }

    //This method will separate [String] from [Int]
    fun loadPlaylistItem(itemProperty: String, cursor: Cursor): Any? {
        return when (itemProperty) {
            "_id",
            "date_added",
            "date_modified" -> cursor.getLong(cursor.getColumnIndex(itemProperty))
            else -> fixMojibake(cursor.getString(cursor.getColumnIndex(itemProperty)))
        }
    }

    //This method will separate [String] from [Int]
    fun loadArtistItem(itemProperty: String, cursor: Cursor): Any? {
        return when (itemProperty) {
            "_id" -> {
                // The [artist] id from Android >= 30/R is a [Long] instead of [Int].
                if (Build.VERSION.SDK_INT >= 30) {
                    cursor.getLong(cursor.getColumnIndex(itemProperty))
                } else {
                    cursor.getInt(cursor.getColumnIndex(itemProperty))
                }
            }
            "number_of_albums",
            "number_of_tracks" -> cursor.getInt(cursor.getColumnIndex(itemProperty))
            else -> fixMojibake(cursor.getString(cursor.getColumnIndex(itemProperty)))
        }
    }

    //This method will separate [String] from [Int]
    fun loadGenreItem(itemProperty: String, cursor: Cursor): Any? {
        return when (itemProperty) {
            "_id" -> {
                // The [genre] id from Android >= 30/R is a [Long] instead of [Int].
                if (Build.VERSION.SDK_INT >= 30) {
                    cursor.getLong(cursor.getColumnIndex(itemProperty))
                } else {
                    cursor.getInt(cursor.getColumnIndex(itemProperty))
                }
            }
            else -> fixMojibake(cursor.getString(cursor.getColumnIndex(itemProperty)))
        }
    }

    fun getMediaCount(type: Int, arg: String, resolver: ContentResolver): Int {
        val uri: Uri = if (type == 0) {
            MediaStore.Audio.Genres.Members.getContentUri("external", arg.toLong())
        } else {
            MediaStore.Audio.Playlists.Members.getContentUri("external", arg.toLong())
        }
        val cursor = resolver.query(uri, null, null, null, null)
        val count = cursor?.count ?: -1
        cursor?.close()
        return count
    }

    // Ignore the [Data] deprecation because this plugin support older versions.
    @Suppress("DEPRECATION")
    fun loadFirstItem(type: Int, id: Number, resolver: ContentResolver): String? {

        // We use almost the same method to 'query' the first item from Song/Album/Artist and we
        // need to use a different uri when 'querying' from playlist.
        // If [type] is something different, return null.
        val selection: String? = when (type) {
            0 -> MediaStore.Audio.Media._ID + "=?"
            1 -> MediaStore.Audio.Media.ALBUM_ID + "=?"
            2 -> null
            3 -> MediaStore.Audio.Media.ARTIST_ID + "=?"
            4 -> null
            else -> return null
        }

        var dataOrId: String? = null
        var cursor: Cursor? = null
        try {
            // Type 2 or 4 we use a different uri.
            //
            // Type 2 == Playlist
            // Type 4 == Genre
            //
            // And the others we use the normal uri.
            when (true) {
                (type == 2 && selection == null) -> {
                    cursor = resolver.query(
                        MediaStore.Audio.Playlists.Members.getContentUri("external", id.toLong()),
                        arrayOf(
                            MediaStore.Audio.Playlists.Members.DATA,
                            MediaStore.Audio.Playlists.Members.AUDIO_ID
                        ),
                        null,
                        null,
                        null
                    )
                }
                (type == 4 && selection == null) -> {
                    cursor = resolver.query(
                        MediaStore.Audio.Genres.Members.getContentUri("external", id.toLong()),
                        arrayOf(
                            MediaStore.Audio.Genres.Members.DATA,
                            MediaStore.Audio.Genres.Members.AUDIO_ID
                        ),
                        null,
                        null,
                        null
                    )
                }
                else -> {
                    cursor = resolver.query(
                        MediaStore.Audio.Media.EXTERNAL_CONTENT_URI,
                        arrayOf(MediaStore.Audio.Media.DATA, MediaStore.Audio.Media._ID),
                        selection,
                        arrayOf(id.toString()),
                        null
                    )
                }
            }
        } catch (e: Exception) {
        //     Log.i("on_audio_error", e.toString())
        }

        //
        if (cursor != null) {
            cursor.moveToFirst()
            // Try / Catch to avoid problems. Everytime someone request the first song from a playlist and
            // this playlist is empty will crash the app, so we just 'print' the error.
            try {
                dataOrId =
                    if (Build.VERSION.SDK_INT >= 29 && (type == 2 || type == 3 || type == 4)) {
                        cursor.getString(1)
                    } else {
                        cursor.getString(0)
                    }
            } catch (e: Exception) {
                Log.i("on_audio_error", e.toString())
            }
        }
        cursor?.close()

        return dataOrId
    }

    fun chooseWithFilterType(uri: Uri, itemProperty: String, cursor: Cursor): Any? {
        return when (uri) {
            MediaStore.Audio.Media.EXTERNAL_CONTENT_URI -> loadSongItem(itemProperty, cursor)
            MediaStore.Audio.Albums.EXTERNAL_CONTENT_URI -> loadAlbumItem(itemProperty, cursor)
            MediaStore.Audio.Playlists.EXTERNAL_CONTENT_URI -> loadPlaylistItem(
                itemProperty,
                cursor
            )
            MediaStore.Audio.Artists.EXTERNAL_CONTENT_URI -> loadArtistItem(itemProperty, cursor)
            MediaStore.Audio.Genres.EXTERNAL_CONTENT_URI -> loadGenreItem(itemProperty, cursor)
            else -> null
        }
    }
}
