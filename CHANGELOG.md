# Changelog

All notable changes to this project will be documented in this file.

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