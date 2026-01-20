package com.lucasjosino.on_audio_query.queries

import android.content.ContentResolver
import android.net.Uri
import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import com.lucasjosino.on_audio_query.PluginProvider
import com.lucasjosino.on_audio_query.consts.ArtistSeparatorConfig
import com.lucasjosino.on_audio_query.controllers.PermissionController
import com.lucasjosino.on_audio_query.queries.helper.QueryHelper
import com.lucasjosino.on_audio_query.types.checkArtistsUriType
import com.lucasjosino.on_audio_query.types.sorttypes.checkArtistSortType
import com.lucasjosino.on_audio_query.utils.artistProjection
import io.flutter.Log
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

/** OnArtistsQuery */
class ArtistQuery : ViewModel() {

    companion object {
        private const val TAG = "OnArtistsQuery"
    }

    //Main parameters
    private val helper = QueryHelper()

    // None of this methods can be null.
    private lateinit var uri: Uri
    private lateinit var resolver: ContentResolver
    private lateinit var sortType: String

    /**
     * Method to "query" all artists.
     */
    fun queryArtists() {
        val call = PluginProvider.call()
        val result = PluginProvider.result()
        val context = PluginProvider.context()
        this.resolver = context.contentResolver

        // Sort: Type and Order
        sortType = checkArtistSortType(
            call.argument<Int>("sortType"),
            call.argument<Int>("orderType")!!,
            call.argument<Boolean>("ignoreCase")!!
        )

        // Check uri:
        //   * 0 -> External.
        //   * 1 -> Internal.
        uri = checkArtistsUriType(call.argument<Int>("uri")!!)

        Log.d(TAG, "Query config: ")
        Log.d(TAG, "\tsortType: $sortType")
        Log.d(TAG, "\turi: $uri")

        // Query everything in background for a better performance.
        viewModelScope.launch {
            val queryResult = loadArtists()
            result.success(queryResult)
        }
    }

    // Loading in Background
    private suspend fun loadArtists(): ArrayList<MutableMap<String, Any?>> =
        withContext(Dispatchers.IO) {
            //Clear previous index data
            ArtistSeparatorConfig.clearIndex()

            // Setup the cursor with 'uri', 'projection' and 'sortType'
            val cursor = resolver.query(uri, artistProjection, null, null, sortType)

            // First pass: collect all raw MediaStore data and build ID lookup
            val rawArtists = mutableListOf<MutableMap<String, Any?>>()
            val mediaStoreIdLookup = mutableMapOf<String, Any?>() //artist name (lowercase) -> MediaStore ID

            Log.d(TAG, "Cursor count: ${cursor?.count}")

            while (cursor != null && cursor.moveToNext()) {
                val tempData: MutableMap<String, Any?> = HashMap()

                for (artistMedia in cursor.columnNames) {
                    tempData[artistMedia] = helper.loadArtistItem(artistMedia, cursor)
                }

                val artistString = tempData["artist"] as? String ?: continue
                rawArtists.add(tempData)

                //If this is a single artist (not combined) => record the MediaStore ID
                val splitCheck = ArtistSeparatorConfig.splitArtistString(artistString)
                if (splitCheck.size == 1) {
                    mediaStoreIdLookup[artistString.lowercase()] = tempData["_id"]
                }
            }

            cursor?.close()

            // Second pass: split and merge artists
            val seenArtists = mutableMapOf<String, MutableMap<String, Any?>>()

            for (tempData in rawArtists) {
                val artistString = tempData["artist"] as? String ?: continue
                val splitArtists = ArtistSeparatorConfig.splitArtistString(artistString)
                val originalId = tempData["_id"]
                val numAlbums = tempData["number_of_albums"] as? Int ?: 0
                val numTracks = tempData["number_of_tracks"] as? Int ?: 0

                //Build split artist index if this is a combined artist
                if (splitArtists.size > 1) {
                    for (artistName in splitArtists) {
                        ArtistSeparatorConfig.addToIndex(artistName, artistString)
                    }
                }

                for (artistName in splitArtists) {
                    val artistKey = artistName.lowercase()

                    if (seenArtists.containsKey(artistKey)) {
                        //Artist already exists => merge counts
                        val existingArtist = seenArtists[artistKey]!!
                        val existingAlbums = existingArtist["number_of_albums"] as? Int ?: 0
                        val existingTracks = existingArtist["number_of_tracks"] as? Int ?: 0
                        existingArtist["number_of_albums"] = existingAlbums + numAlbums
                        existingArtist["number_of_tracks"] = existingTracks + numTracks
                    } else {
                        //New artist => create entry
                        val splitData: MutableMap<String, Any?> = HashMap()

                        // Determine the ID to use:
                        // 1. If this artist exists as standalone in MediaStore => use that ID
                        // 2. If not split (single artist) => use original ID
                        // 3. Otherwise => generate deterministic negative ID for split artist
                        val artistId = when {
                            mediaStoreIdLookup.containsKey(artistKey) -> mediaStoreIdLookup[artistKey]
                            splitArtists.size == 1 -> originalId
                            else -> ArtistSeparatorConfig.generateSplitArtistId(artistName)
                        }

                        splitData["_id"] = artistId

                        //Add ID-to-name mapping for split artists
                        if (splitArtists.size > 1 && artistId is Long) {
                            ArtistSeparatorConfig.addIdMapping(artistId, artistName)
                        }

                        splitData["artist"] = artistName
                        splitData["number_of_albums"] = numAlbums
                        splitData["number_of_tracks"] = numTracks

                        seenArtists[artistKey] = splitData
                    }
                }
            }

            Log.d(TAG, "Split artist count: ${seenArtists.size} (from ${rawArtists.size} raw entries)")

            return@withContext ArrayList(seenArtists.values)
        }
}