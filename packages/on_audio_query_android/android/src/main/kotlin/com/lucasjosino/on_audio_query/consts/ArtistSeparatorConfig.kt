package com.lucasjosino.on_audio_query.consts

import java.util.concurrent.ConcurrentHashMap

// This object provides shared constants and methods for splitting combined artist strings
// (e.g. "Artist A, Artist B") into individual artists
object ArtistSeparatorConfig {

    val SEPARATORS = listOf(
        " feat. ",
        " ft. ",
        " featuring ",
        " / ",
        "/",
        ", ",
        " & ",
        "&",
        " and ",
        " x ",
        " X ",
    )

    // Pattern-based exceptions for detecting band names that contain separators
    // but should NOT be split
    // Only use very specific patterns to avoid false positives!
    // For individual artist names like "Tyler, The Creator", use EXACT_EXCEPTIONS instead.
    private val EXCEPTION_PATTERNS = listOf<Regex>(
        //Use exact matches only to avoid false positives
    )

    // Exact-match exceptions for well-known band names/groups that dont match patterns
    // but should not be split
    private val EXACT_EXCEPTIONS = setOf(
        "simon & garfunkel",
        "hall & oates",
        "earth, wind & fire",
        "emerson, lake & palmer",
        "crosby, stills, nash & young",
        "peter, paul and mary",
        "blood, sweat & tears",
        "up, bustle and out",
        "me first and the gimme gimmes",
        "hootie & the blowfish",
        "katrina and the waves",
        "kc and the sunshine band",
        "martha and the vandellas",
        "gladys knight & the pips",
        "bob seger & the silver bullet band",
        "huey lewis and the news",
        "echo & the bunnymen",
        "tom petty and the heartbreakers",
        "bob marley & the wailers",
        "sly & the family stone",
        "bruce springsteen & the e street band",
        "diana ross & the supremes",
        "smokey robinson & the miracles",
        "joan jett & the blackhearts",
        "prince & the revolution",
        "derek & the dominos",
        "sergio mendes & brasil '66",

        "tyler, the creator",
        "panic! at the disco",
        "florence + the machine",
        "florence and the machine",
    )

    // Index mapping split artist names to the combined artist strings they appear in
    // Example: "artist a" -> ["Artist A, Artist B", "Artist A & Artist C"]
    private val splitArtistIndex = ConcurrentHashMap<String, MutableSet<String>>()

    // Index mapping split artist IDs to their display names
    // Key: split artist ID (negative number)
    // Value: artist display name
    private val idToNameMap = ConcurrentHashMap<Long, String>()

    // Checks if an artist name should be treated as an exception and NOT split
    fun isExceptionArtist(artistString: String): Boolean {
        val lower = artistString.lowercase()

        //Check exact matches first
        if (EXACT_EXCEPTIONS.contains(lower)) return true

        //Check pattern matches
        return EXCEPTION_PATTERNS.any { it.matches(artistString) }
    }


    // This method uses smart splitting to protect exception artists within combined strings
    // 1. Checks if entire string is an exception => returns unsplit if so
    // 2. Replaces exception artists with placeholders to protect them
    // 3. Builds a regex from all separators
    // 4. Splits on any(!) matching separators => handles multiple separator types
    // 5. Restores placeholders with original exception artist names
    //
    // Example: "Artist A, Artist B" => ["Artist A", "Artist B"]
    // Example: "Tyler, The Creator, Frank Ocean" => ["Tyler, The Creator", "Frank Ocean"]
    fun splitArtistString(artistString: String): List<String> {
        //Check if entire string is an exception first
        if (isExceptionArtist(artistString)) return listOf(artistString)

        //Smart splitting: protect exception artists within combined strings
        //Replace exception artists with placeholders before splitting
        var workingString = artistString
        val placeholders = mutableMapOf<String, String>()
        var placeholderIndex = 0

        //Sort exceptions by length (longest first) to avoid partial matches
        val sortedExceptions = EXACT_EXCEPTIONS.sortedByDescending { it.length }

        for (exception in sortedExceptions) {
            //Case-insensitive search for exception within the string
            val regex = Regex(Regex.escape(exception), RegexOption.IGNORE_CASE)
            val match = regex.find(workingString)
            if (match != null) {
                val placeholder = "___ARTIST_PLACEHOLDER_${placeholderIndex++}___"
                placeholders[placeholder] = match.value
                workingString = workingString.replaceFirst(regex, placeholder)
            }
        }

        //Build combined regex pattern from all separators
        //(Regex.escape ensures special characters in separators are treated literally)
        val pattern = SEPARATORS.joinToString("|") { Regex.escape(it) }
        val regex = Regex(pattern, RegexOption.IGNORE_CASE)

        //Split by any separator => trim whitespace => filter empty strings
        var parts = workingString.split(regex)
            .map { it.trim() }
            .filter { it.isNotEmpty() }

        //Restore placeholders with original exception artist names
        parts = parts.map { part ->
            var result = part
            placeholders.forEach { (placeholder, original) ->
                result = result.replace(placeholder, original)
            }
            result.trim()
        }

        // Only return split if we actually got multiple parts
        return if (parts.size > 1) parts else listOf(artistString)
    }

    // Generates a deterministic, stable ID for a split Artist
    // ID is,
    // - Negative => to distinguish from MediaStore IDS
    // - Deterministic => same njame always produces same ID
    // 
    // @param artistName => The artist name (duh)
    // @return => A negative Long ID
    fun generateSplitArtistId(artistName: String): Long {
        //Normalize to lowercase and trim for consistency
        val normalized = artistName.trim().lowercase()
        val hash = normalized.hashCode().toLong()

        //Ensure negative to distinguish from MediaStore IDs
        //Use bitwise AND to keep within valid range
        return -(hash.and(0x7FFFFFFF))
    }

    // Adds an artist to the split artist index
    //
    // @param splitArtistName => Individual artist name (lowecase for key)
    // @param combinedArtistString => Original combined string from MediaStore
    fun addToIndex(splitArtistName: String, combinedArtistString: String) {
        val key = splitArtistName.lowercase()
        splitArtistIndex.getOrPut(key) { mutableSetOf() }.add(combinedArtistString)
    }

    // Gets all combined artist strings that contain a given artist
    //
    // @param artistName => The individuel artist name to look up
    // @return => Set of combined artist string (or empty set if not found)
    fun getCombinedArtistsFor(artistName: String): Set<String> {
        val key = artistName.lowercase()
        return splitArtistIndex[key]?.toSet() ?: emptySet()
    }

    // Adds an ID-to-name mapping for a split artist
    //
    // @param artistID => The split artist ID (negative: explained further above)
    // @param artistName => The split artist name (duh)
    fun addIdMapping(artistId: Long, artistName: String) {
        idToNameMap[artistId] = artistName
    }

    // Gets the artist name for a given split artist ID
    //
    // @param artistId => The split artist ID
    // @return => The artist name (or null if not found)
    fun getArtistNameById(artistId: Long): String? {
        return idToNameMap[artistId]
    }

    // Clears all index data
    // => Should be called when reloading artist list
    fun clearIndex() {
        splitArtistIndex.clear()
        idToNameMap.clear()
    }

    // Checks if an artist ID represents a split artist (negative ID: explained further above)
    //
    // @param artistId => The artist ID to check
    // @return true: if is a split artist; false: not a split artist
    fun isSplitArtistId(artistId: Long): Boolean {
        return artistId < 0
    }
}