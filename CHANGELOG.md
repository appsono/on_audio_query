# Changelog

All notable changes to this project will be documented in this file.

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
- 
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
