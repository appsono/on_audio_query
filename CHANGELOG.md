# Changelog

All notable changes to this project will be documented in this file.

## [3.0.3] - 2026-02-20

### Fixed
- **iOS: All queries failed when Apple Music not authorized** — The plugin blocked every query (songs, albums, artists, recently added) behind an `MPMediaLibrary` permission check. Since local-file queries use `FileManager`/`AVFoundation` and require no Apple Music authorization, the gate has been removed. Queries now always execute and return local-file results; Apple Music items are included when the user has granted that permission.
- **iOS: Albums never appeared in the library** — `buildAlbums()` used `first["album_id"] as Any` to populate the `"_id"` field of each album entry. Subscripting a `[String: Any?]` dictionary returns `Any??` (double optional); casting that to `Any` wraps the double optional rather than extracting the integer, so the Flutter platform codec received `null` instead of the album ID. The ID-based filter in `getFilteredAlbums()` therefore discarded every local album. Fixed by iterating key-value pairs of the grouping dictionary and using the `Int` key directly. Same fix applied to `buildArtists()`.
- **iOS: Fatal crash when sorting by duration** — `formatSongList` cast the `duration` field as `Double`, but duration is stored as `Int` (milliseconds). In Swift `as!` is a type cast, not a numeric conversion, so this caused a fatal runtime crash whenever a duration sort was applied. Changed to `as! Int`.
- **iOS: Fatal crash when sorting by file size** — `formatSongList` force-cast `_size as! Int`, but Apple Music items that do not expose a file size store `nil` for this field. Force-casting `nil as! Int` is a fatal crash. Changed to `as? Int ?? 0`.
- **iOS: AlbumQuery used song sort-type grouping** — `AlbumQuery` passed `checkSongSortType()` to `cursor.groupingType`, whose default is `.title` (group by song title). An album query must use `checkAlbumSortType()` (default `.album`) to group Apple Music items correctly by album.

## [3.0.2] - 2026-02-20

Fixed some issue that made the App unable to build.

## [3.0.1] - 2026-02-20

### Changed
- **iOS async file scanning**: Converted `LocalFileQuery` from synchronous semaphore-based processing to Swift structured concurrency (`async/await` + `TaskGroup`), scanning all local audio files in parallel for ~60-80% faster query times on multi-core devices.
- All five callers (`AudioQuery`, `AlbumQuery`, `ArtistQuery`, `AudioFromQuery`, `ArtworkQuery`) now bridge into async context via `Task` instead of `DispatchQueue.global`.

### Fixed
- **iOS corrupted file handling**: Files with unreadable metadata are now caught by `do-catch` and skipped with a warning log instead of throwing an exception that could halt the entire scan.
- **iOS thread blocking**: Removed `DispatchSemaphore` usage in `loadLocalSong` that held a thread hostage during disk I/O. Metadata loading now uses `try await AVURLAsset.load(.duration, .commonMetadata)`.

### Technical Details
- **Modified**: `LocalFileQuery.swift` — `querySongs()`, `queryAlbums()`, `queryArtists()`, `queryAudiosFrom(type:where:)`, and `artworkData(forId:type:size:format:quality:)` are now `async`. `loadLocalSong` uses the modern `AVAsset.load(_:)` API. `querySongs` uses `withTaskGroup` for parallel processing.
- **Modified**: `AudioQuery.swift`, `AlbumQuery.swift`, `ArtistQuery.swift`, `AudioFromQuery.swift`, `ArtworkQuery.swift` — replaced `DispatchQueue.global(qos: .userInitiated).async` with `Task` blocks to call async `LocalFileQuery` methods. Flutter results still dispatch to main thread.
- **Note**: Minimum deployment target is now iOS 15+ (required for `AVAsset.load(_:)`).

## [3.0.0] - 2026-02-24

### Added
- **iOS local file support**: Songs placed in `Documents/Music/` inside the app sandbox are now included in all queries alongside Apple Music library items.
  - Users can populate the folder from the iOS Files app (requires `UIFileSharingEnabled` in `Info.plist`).
  - Supported formats: `mp3`, `m4a`, `flac`, `wav`, `aac`, `ogg`, `opus`, `aiff`, `aif`, `alac`, `wv`, `ape`.
  - Metadata (title, artist, album, composer, duration, file size, dates) is extracted via `AVFoundation`.
  - Stable, deterministic IDs are generated from file paths / metadata using an FNV-1a hash so IDs are consistent across app launches.
  - Embedded cover art in local files is surfaced through the existing artwork query API with the same resize/format/quality options.

### Changed
- Updated `on_audio_query_ios` to v1.1.5

### Fixed
- **iOS nil safety**: Replaced forced-unwrap (`!`) accesses on `MPMediaQuery` items and `MPMediaItemCollection` arrays with safe optional binding throughout `AudioQuery`, `AlbumQuery`, `ArtistQuery`, and `AudioFromQuery`. This prevents crashes when the media library is empty or authorization is restricted.

### Technical Details
- **New**: `LocalFileQuery.swift` — scans `Documents/Music/`, reads `AVURLAsset` metadata, and exposes `querySongs()`, `queryAlbums()`, `queryArtists()`, `queryAudiosFrom(type:where:)`, and `artworkData(forId:type:size:format:quality:)`.
- **Modified**: `AudioQuery.swift`, `AlbumQuery.swift`, `ArtistQuery.swift` — results from `LocalFileQuery` are appended to the Apple Music results before sorting/filtering is applied.
- **Modified**: `AudioFromQuery.swift` — falls back to `LocalFileQuery.queryAudiosFrom` when `MPMediaQuery` returns no items (covers local-file album/artist IDs that don't exist in Apple Music).
- **Modified**: `ArtworkQuery.swift` — falls back to `LocalFileQuery.artworkData` when Apple Music returns no artwork for a given ID.

## [2.9.8 - 2.9.9] - 2026-02-24

### Fixed
- **iOS Apple Music/iCloud songs**: Fixed issue where offline songs from Apple Music or iCloud were not being queried correctly.
  - The filter preventing cloud items from being fetched has been removed, as it was incorrectly blocking legally downloaded tracks.

### Changed
- Updated `on_audio_query_ios` to v1.1.3
- Updated `on_audio_query_ios` to v1.1.4

### Technical Details
- **Modified**: `AudioQuery.swift`, `AlbumQuery.swift` - Removed the `is_cloud` item filter to allow songs from Apple Music and iCloud to be queried and refactored query logic for improved readability.

## [2.9.7] - 2026-02-17

### Added
- **Linux platform support**: Full native Linux implementation using C++ with FFmpeg/ffprobe
  - SQLite-backed metadata database with incremental scanning
  - Multi-threaded file scanning with thread pool for parallel metadata extraction
  - LRU caching for frequently accessed queries
  - Supports all query types: songs, albums, artists, genres, playlists, folders, and artwork
  - Artist separation system ported to Linux with full split artist handling
  - Split artists integrated into database queries and updates for consistent behavior

### Changed
- Added `on_audio_query_linux` v1.0.0

### Technical Details
- **New package**: `on_audio_query_linux` - Complete Linux platform implementation
- **Key components**: `DatabaseManager` (SQLite), `FfprobeExtractor` (metadata), `ScanCoordinator` (incremental scanning), `ThreadPool` (concurrency)
- **Artist splitting**: `ArtistSeparator` class with same separator/exception logic as Android, integrated into `DatabaseManager` for split artist persistence
- **Performance**: Index-based lookups and LRU cache for split artist queries

## [2.9.6] - 2026-02-06

### Fixed
- **Artist separation with exception artists**: Fixed incorrect splitting of exception artists within combined strings
  - Exception artists (e.g., "Tyler, The Creator") are now protected when appearing in combined strings
  - Implemented placeholder mechanism to preserve exception artists during split operation
  - Sorts exceptions by length (longest first) to avoid partial matches
  - Example: "Tyler, The Creator, Frank Ocean" now correctly splits to ["Tyler, The Creator", "Frank Ocean"]

### Changed
- Updated `on_audio_query_android` to v1.1.6

### Technical Details
- **Modified**: `ArtistSeparatorConfig.kt` - Added smart splitting with placeholder protection for exception artists
- Exception artists are temporarily replaced with placeholders before splitting, then restored afterward
- Case-insensitive matching for better exception detection

## [2.9.5] - 2026-01-24

### Fixed
- **DISC_NUMBER column crash**: Fixed SQLite crash on Android 9 (API 28) and earlier devices
  - `disc_number` column doesn't exist in MediaStore before API 29
  - Query now conditionally includes the column only on supported Android versions

### Changed
- Updated `on_audio_query_android` to v1.1.5

### Technical Details
- **Modified**: `CursorProjection.kt` - Moved `MediaStore.Audio.Media.DISC_NUMBER` from unconditional projection to `Build.VERSION.SDK_INT >= 29` conditional block

## [2.9.4] - 2026-01-20

### Changed
- **Artist separation system**: Better artist detection and handling of edge cases
  - Added unescaped "/" and "&" as separators for better artist splitting
  - Replaced pattern-based exceptions with exact-match-only approach to avoid false positives
  - Implemented third-pass song querying to catch featured artists that MediaStore.Audio.Artists missed
  - Split artist index now built during song metadata parsing for more complete artist detection
  - Better track count accuracy by comparing MediaStore counts with song-based counting
- Updated `on_audio_query_android` to v1.1.4

### Technical Details
- **Modified**: `ArtistSeparatorConfig.kt` - Expanded separator list and refined exception handling strategy
- **Modified**: `ArtistQuery.kt` - Added direct song querying pass to discover missing artists from combined strings
- Split artist detection now more comprehensive and accurate across all edge cases

## [2.9.3] - 2026-01-20

### Added
- **Artist separation system**: Automatic splitting of combined artist strings (e.g., "Artist A, Artist B") into individual artists
  - Centralized all artist splitting logic in `ArtistSeparatorConfig` for consistency
  - Pattern-based exception detection for band names with separators (scales infinitely, e.g., "X, The Y" pattern)
  - Regex-based multi-separator parsing (correctly handles "A, B & C feat. D")
  - Index-based lookup for querying songs by split artist (~100-1000x faster for large libraries)
  - LRU cache (50 entries) for frequently accessed split artists (~5000x faster on cache hits)
  - Support for 9 different separators: feat., ft., featuring, /, comma, &, and, x, X

### Technical Details
- **New file**: `ArtistSeparatorConfig.kt` - Centralized configuration for all artist separation logic
- **Modified**: `ArtistQuery.kt` - Builds split artist index and ID-to-name mappings during load
- **Modified**: `AudioFromQuery.kt` - Uses index-based lookup with LRU caching for split artist queries
- Pattern-based exceptions using regex: `^.+, the .+$`, `^.+ & the .+$`, `^.+ and the .+$`
- Performance: 20,000+ song libraries can query split artists in ~5ms (first access) and ~0.1ms (cached)
- Split artists use deterministic negative IDs to distinguish from MediaStore IDs

## [2.9.2] - 2026-01-19

### Changed
- **discNumber robustness**: Updated the `discNumber` getter to support both `String` and `int` values, automatically parsing `String` values to `int` when required
- Updated `on_audio_query_platform_interface` to v1.7.2
- Updated `on_audio_query_android` to v1.1.2
- Updated `on_audio_query_ios` to v1.1.2

### Technical Details
- Improved type handling in the Dart `SongModel.discNumber` getter to ensure consistent integer output across platforms

## [2.9.1] - 2026-01-19

### Added
- **discNumber support**: Added `discNumber` property to `SongModel` to expose disc number metadata from audio files
  - Android: Reads `MediaStore.Audio.Media.DISC_NUMBER` from MediaStore
  - iOS: Reads `discNumber` from `MPMediaItem`
  - Enables proper multi-disc album separation and display

### Changed
- Updated `on_audio_query_platform_interface` to v1.7.1
- Updated `on_audio_query_android` to v1.1.1
- Updated `on_audio_query_ios` to v1.1.1

### Technical Details
- Added `DISC_NUMBER` to Android cursor projection in `CursorProjection.kt`
- Added `disc_number` field to iOS `OnAudioHelper.swift`
- Added `discNumber` getter to Dart `SongModel` class

## [2.9.0] - Original version
- Base functionality from upstream repository