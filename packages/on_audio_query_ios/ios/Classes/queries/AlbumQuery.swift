import Flutter
import MediaPlayer
class AlbumQuery {
    var args: [String: Any]
    var result: FlutterResult
    
    init() {
        self.args = try! PluginProvider.call().arguments as! [String: Any]
        self.result = try! PluginProvider.result()
    }
    
    func queryAlbums() {
        let sortType = args["sortType"] as? Int ?? 0
        
        let cursor = MPMediaQuery.albums()
        
        // Using native sort from [IOS] you can only use the [Album] and [Artist].
        // The others will be sorted "manually" using [formatAlbumList] before
        // send to Dart.
        cursor.groupingType = checkSongSortType(sortType: sortType)
        
        Log.type.debug("Query config: ")
        Log.type.debug("\tsortType: \(sortType)")
        
        // Query everything in background for a better performance.
        loadAlbums(cursor: cursor.collections)
    }

    private func loadAlbums(cursor: [MPMediaItemCollection]?) {
        Task {
            var listOfAlbums: [[String: Any?]] = Array()
            // Load from Apple Music lib (guard against nil empty)
            if let collections = cursor {
                for album in collections {
                    if !album.items[0].isCloudItem, album.items[0].assetURL != nil {
                        let albumData = loadAlbumItem(album: album)
                        listOfAlbums.append(albumData)
                    }
                }
            }
            // Load albums built from apps local documents (async)
            let localAlbums = await LocalFileQuery().queryAlbums()
            listOfAlbums.append(contentsOf: localAlbums)
            
            let finalList = formatAlbumList(args: self.args, allAlbums: listOfAlbums)
            
            DispatchQueue.main.async {
                self.result(finalList)
            }
        }
    }
}
