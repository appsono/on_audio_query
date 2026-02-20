import Flutter
import MediaPlayer

class ArtistQuery {
    var args: [String: Any]
    var result: FlutterResult
    
    init() {
        self.args = try! PluginProvider.call().arguments as! [String: Any]
        self.result = try! PluginProvider.result()
    }
    
    func queryArtists() {
        let cursor = MPMediaQuery.artists()
        
        // We don't need to define a sortType here. [IOS] only support
        // the [Artist]. The others will be sorted "manually" using
        // [formatSongList] before send to Dart.
        
        // Filter to avoid audios/songs from cloud library.
        let cloudFilter = MPMediaPropertyPredicate(
            value: false,
            forProperty: MPMediaItemPropertyIsCloudItem
        )
        cursor.addFilterPredicate(cloudFilter)

        loadArtists(cursor: cursor.collections)
    }
    
    private func loadArtists(cursor: [MPMediaItemCollection]?) {
        Task {
            var listOfArtists: [[String: Any?]] = Array()

            // Load from Apple Music lib (guard against nil when empty)
            if let collections = cursor {
                for artist in collections {
                    if !artist.items[0].isCloudItem, artist.items[0].assetURL != nil {
                        let artistData = loadArtistItem(artist: artist)
                        listOfArtists.append(artistData)
                    }
                }
            }

            // Also load artists built from apps local documents (async)
            let localArtists = await LocalFileQuery().queryArtists()
            listOfArtists.append(contentsOf: localArtists)

            let finalList = formatArtistList(args: self.args, allArtists: listOfArtists)
            
            DispatchQueue.main.async {
                self.result(finalList)
            }
        }
    }
}
