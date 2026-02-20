import AVFoundation
import CoreMedia
import UIKit

/// Queries audio files stored in the apps Documents
/// This allows users to drop files via the iOS Files app and have them
/// appear in the library alongside Apple Music items
class LocalFileQuery {

    private static let audioExtensions: Set<String> = [
        "mp3", "m4a", "flac", "wav", "aac", "ogg", "opus",
        "aiff", "aif", "alac", "wv", "ape"
    ]

    // MARK: - Directory

    /// Returns (and creates if needed) Documents/Music/ inside the app sandbox
    /// With UIFileSharingEnabled set in Info.plist the user can see and populate
    /// this folder from the iOS Files app
    static func getMusicDirectory() -> URL? {
        guard let docs = FileManager.default
            .urls(for: .documentDirectory, in: .userDomainMask).first else {
            return nil
        }
        let musicDir = docs.appendingPathComponent("Music", isDirectory: true)
        try? FileManager.default.createDirectory(
            at: musicDir,
            withIntermediateDirectories: true,
            attributes: nil
        )
        return musicDir
    }

    // MARK: - Public Query Methods (Async)

    func querySongs() async -> [[String: Any?]] {
        let urls = enumerateAudioFiles()

        return await withTaskGroup(of: [String: Any?]?.self) { group in
            for url in urls {
                group.addTask {
                    return await self.loadLocalSong(fileURL: url)
                }
            }

            var results: [[String: Any?]] = []
            for await song in group {
                if let song = song {
                    results.append(song)
                }
            }
            return results
        }
    }

    func queryAlbums() async -> [[String: Any?]] {
        return buildAlbums(from: await querySongs())
    }

    func queryArtists() async -> [[String: Any?]] {
        return buildArtists(from: await querySongs())
    }

    /// Filters local songs matching the same type/where semantics used by MPMediaQuery
    /// type 0 = album name, 1 = album id, 2 = artist name, 3 = artist id
    func queryAudiosFrom(type: Int, where whereValue: Any) async -> [[String: Any?]] {
        let songs = await querySongs()
        switch type {
        case 0: //ALBUM_TITLE (String)
            guard let name = whereValue as? String else { return [] }
            return songs.filter { ($0["album"] as? String) == name }
        case 1: //ALBUM_ID (Int)
            guard let id = whereValue as? Int else { return [] }
            return songs.filter { ($0["album_id"] as? Int) == id }
        case 2: //ARTIST_NAME (String)
            guard let name = whereValue as? String else { return [] }
            return songs.filter { ($0["artist"] as? String) == name }
        case 3: //ARTIST_ID (Int)
            guard let id = whereValue as? Int else { return [] }
            return songs.filter { ($0["artist_id"] as? Int) == id }
        default:
            return []
        }
    }

    /// Returns embedded cover art for a local song or album
    /// type 0 = look up by song id, type 1 = look up by album id
    func artworkData(forId id: Int, type: Int, size: Int, format: Int, quality: Int) async -> Data? {
        let songs = await querySongs()

        let match: [String: Any?]?
        if type == 0 {
            match = songs.first { ($0["_id"] as? Int) == id }
        } else {
            match = songs.first { ($0["album_id"] as? Int) == id }
        }

        guard let songData = match,
              let uriString = songData["_uri"] as? String,
              let fileURL = URL(string: uriString) else { return nil }

        let asset = AVURLAsset(url: fileURL)

        do {
            let metadata = try await asset.load(.commonMetadata)

            for item in metadata {
                guard item.commonKey == .commonKeyArtwork,
                      let rawData = item.value as? Data,
                      let image = UIImage(data: rawData) else { continue }

                let cgSize = CGSize(width: size, height: size)
                guard let resized = resizeImage(image, to: cgSize) else { return nil }
                let convertedQuality = CGFloat(Double(quality) / 100.0)
                return format == 0
                    ? resized.jpegData(compressionQuality: convertedQuality)
                    : resized.pngData()
            }
        } catch {
            Log.type.warning("Failed to load artwork metadata for \(fileURL.lastPathComponent): \(error)")
        }

        return nil
    }

    // MARK: - Private Helpers

    private func enumerateAudioFiles() -> [URL] {
        guard let musicDir = LocalFileQuery.getMusicDirectory() else { return [] }
        let fm = FileManager.default
        guard let enumerator = fm.enumerator(
            at: musicDir,
            includingPropertiesForKeys: [
                .fileSizeKey, .creationDateKey, .contentModificationDateKey
            ],
            options: [.skipsHiddenFiles]
        ) else { return [] }

        var files: [URL] = []
        for case let fileURL as URL in enumerator {
            if LocalFileQuery.audioExtensions.contains(
                fileURL.pathExtension.lowercased()
            ) {
                files.append(fileURL)
            }
        }
        return files
    }

    private func loadLocalSong(fileURL: URL) async -> [String: Any?]? {
        let asset = AVURLAsset(
            url: fileURL,
            options: [AVURLAssetPreferPreciseDurationAndTimingKey: false]
        )

        do {
            // Load duration and metadata in parallel using the modern async API
            // This replaces the old DispatchSemaphore-based synchronous loading
            let (durationCM, commonMetadata) = try await asset.load(.duration, .commonMetadata)

            // Duration
            var duration = 0
            if durationCM.isNumeric && !durationCM.isIndefinite {
                duration = max(0, Int(CMTimeGetSeconds(durationCM) * 1000))
            }

            // Metadata
            var title = fileURL.deletingPathExtension().lastPathComponent
            var artist: String? = nil
            var albumTitle: String? = nil
            var composer: String? = nil

            for item in commonMetadata {
                guard let key = item.commonKey else { continue }
                switch key {
                case .commonKeyTitle:
                    if let val = item.stringValue { title = val }
                case .commonKeyArtist:
                    artist = item.stringValue
                case .commonKeyAlbumName:
                    albumTitle = item.stringValue
                case .commonKeyCreator:
                    composer = item.stringValue
                default:
                    break
                }
            }

            // File attributes
            let resources = try? fileURL.resourceValues(
                forKeys: [.fileSizeKey, .creationDateKey, .contentModificationDateKey]
            )
            let fileSize = resources?.fileSize ?? 0
            let dateAdded = resources?.creationDate ?? Date()
            let dateModified = resources?.contentModificationDate ?? Date()
            let fileExt = fileURL.pathExtension.lowercased()

            // Stable IDs derived from path / metadata strings
            let songId   = stableId(for: fileURL.path)
            let albumKey = "\(albumTitle ?? "")|\(artist ?? "")"
            let albumId  = stableId(for: albumKey)
            let artistId = stableId(for: artist ?? "")

            let displayNameWOExt: String
            if let a = artist, !a.isEmpty {
                displayNameWOExt = "\(a) - \(title)"
            } else {
                displayNameWOExt = title
            }

            return [
                "_id":                  songId,
                "_data":                fileURL.absoluteString,
                "_uri":                 fileURL.absoluteString,
                "_display_name":        "\(displayNameWOExt).\(fileExt)",
                "_display_name_wo_ext": displayNameWOExt,
                "_size":                fileSize,
                "album":                albumTitle,
                "album_id":             albumId,
                "artist":               artist,
                "artist_id":            artistId,
                "genre":                nil as String?,
                "genre_id":             nil as Int?,
                "bookmark":             0,
                "composer":             composer,
                "date_added":           Int(dateAdded.timeIntervalSince1970),
                "date_modified":        Int(dateModified.timeIntervalSince1970),
                "duration":             duration,
                "title":                title,
                "track":                0,
                "disc_number":          0,
                "file_extension":       fileExt
            ]
        } catch {
            Log.type.warning("Failed to load metadata for \(fileURL.lastPathComponent): \(error)")
            return nil
        }
    }

    private func buildAlbums(from songs: [[String: Any?]]) -> [[String: Any?]] {
        var albumGroups: [Int: [[String: Any?]]] = [:]
        for song in songs {
            guard let albumId = song["album_id"] as? Int else { continue }
            albumGroups[albumId, default: []].append(song)
        }
        // Iterate key-value pairs so we can use the Int key directly for "_id"
        // and "album_id". Using "first["album_id"] as Any" would produce a
        // double-optional (Any??) that the Flutter codec cannot bridge, causing
        // the field to arrive as null in Dart and breaking the album-ID filter
        return albumGroups.compactMap { (albumId, albumSongs) -> [String: Any?]? in
            guard let first = albumSongs.first else { return nil }
            return [
                "numsongs":  albumSongs.count,
                "artist":    first["artist"] as? String,
                "_id":       albumId,
                "album":     first["album"] as? String,
                "artist_id": first["artist_id"] as? Int,
                "album_id":  albumId
            ]
        }
    }

    private func buildArtists(from songs: [[String: Any?]]) -> [[String: Any?]] {
        var artistGroups: [Int: [[String: Any?]]] = [:]
        for song in songs {
            guard let artistId = song["artist_id"] as? Int else { continue }
            artistGroups[artistId, default: []].append(song)
        }
        // Same fix as buildAlbums: use the Int key directly to avoid double-optional
        return artistGroups.compactMap { (artistId, artistSongs) -> [String: Any?]? in
            guard let first = artistSongs.first else { return nil }
            let albums = Set(artistSongs.compactMap { $0["album"] as? String })
            return [
                "_id":              artistId,
                "artist":           first["artist"] as? String,
                "number_of_albums": albums.count,
                "number_of_tracks": artistSongs.count
            ]
        }
    }

    private func resizeImage(_ image: UIImage, to size: CGSize) -> UIImage? {
        UIGraphicsBeginImageContextWithOptions(size, false, 1.0)
        defer { UIGraphicsEndImageContext() }
        image.draw(in: CGRect(origin: .zero, size: size))
        return UIGraphicsGetImageFromCurrentImageContext()
    }

    /// FNV-1a hash, masked to a positive Int64 so it round-trips safely through
    /// the Flutter platform channel (which uses signed 64-bit integers).
    private func stableId(for string: String) -> Int {
        var hash: UInt64 = 14695981039346656037
        for byte in string.utf8 {
            hash ^= UInt64(byte)
            hash = hash &* 1099511628211
        }
        // Keep 63 bits => always positive when viewed as signed Int64
        let result = Int(bitPattern: UInt(hash & 0x7FFFFFFFFFFFFFFF))
        return result == 0 ? 1 : result
    }
}
