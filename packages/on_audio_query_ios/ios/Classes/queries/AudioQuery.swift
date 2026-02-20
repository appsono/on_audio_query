import Flutter
import MediaPlayer

class AudioQuery {
    var args: [String: Any]
    var result: FlutterResult
    
    init() {
        self.args = try! PluginProvider.call().arguments as! [String: Any]
        self.result = try! PluginProvider.result()
    }
    
    func querySongs() {
        let sortType = args["sortType"] as? Int ?? 0
        
        let cursor = MPMediaQuery.songs()
        
        // Using native sort from [IOS] you can only use the [Title], [Album] and
        // [Artist]. The others will be sorted "manually" using [formatSongList] before
        // send to Dart.
        cursor.groupingType = checkSongSortType(sortType: sortType)
        
        // NOTE: We intentionally do NOT filter by isCloudItem here.
        // Apple Music downloaded tracks still have isCloudItem = true and
        // assetURL = nil (DRM), so the old cloudFilter predicate blocked them entirely.
        
        Log.type.debug("Query config: ")
        Log.type.debug("\tsortType: \(sortType)")

        // Query everything in background for a better performance.
        loadSongs(cursor: cursor)
    }
    
    private func loadSongs(cursor: MPMediaQuery!) {
        DispatchQueue.global(qos: .userInitiated).async {
            var listOfSongs: [[String: Any?]] = Array()
            
            // For each item(song) inside this "cursor", take one and "format"
            // into a [Map<String, dynamic>], all keys are based on [Android]
            // platforms.
            for song in cursor.items! {
                // Include local files and downloaded Apple Music tracks.
                // Apple Music items have isCloudItem=true and assetURL=nil (DRM) even when downloaded.
                let isLocal = song.assetURL != nil
                let isDownloaded = song.isDownloaded
                if isLocal || isDownloaded {
                    let songData = loadSongItem(song: song)
                    listOfSongs.append(songData)
                }
            }
            
            DispatchQueue.main.async {
                // Custom sort/order.
                let finalList = formatSongList(args: self.args, allSongs: listOfSongs)
                self.result(finalList)
            }
        }
    }
}