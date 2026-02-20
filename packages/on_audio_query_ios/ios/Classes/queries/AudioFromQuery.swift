import Flutter
import MediaPlayer

class AudioFromQuery {
    var args: [String: Any]
    var result: FlutterResult
    var type: Int = -1
    
    init() {
        self.args = try! PluginProvider.call().arguments as! [String: Any]
        self.result = try! PluginProvider.result()
    }
    
    func queryAudiosFrom() {
        //
        type = args["type"] as! Int
        let wh3re = args["where"] as Any
        // The sortType.
        let sortType = args["sortType"] as? Int ?? 0
        
        // Choose the type(To match android side, let's call "cursor").
        var cursor: MPMediaQuery? = checkAudiosFrom(type: type, where: wh3re)
        
        // Using native sort from [IOS] you can only use the [Title], [Album] and
        // [Artist]. The others will be sorted "manually" using [formatSongList] before
        // send to Dart.
        cursor?.groupingType = checkSongSortType(sortType: sortType)
        
        // Here we'll check if the request is to [Playlist] or other.
        if type != 6, cursor != nil {
            // This filter will avoid audios/songs outside phone library(cloud).
            let cloudFilter = MPMediaPropertyPredicate(
                value: false,
                forProperty: MPMediaItemPropertyIsCloudItem
            )
            cursor?.addFilterPredicate(cloudFilter)
                
            // Query everything in background for a better performance.
            loadQueryAudiosFrom(cursor: cursor)
        } else {
            // Query everything in background for a better performance.
            cursor = MPMediaQuery.playlists()
                
            // This filter will avoid audios/songs outside phone library(cloud).
            let cloudFilter = MPMediaPropertyPredicate(
                value: false,
                forProperty: MPMediaItemPropertyIsCloudItem
            )
            cursor?.addFilterPredicate(cloudFilter)
                
            loadSongsFromPlaylist(cursor: cursor?.collections)
        }
    }
    
    private func loadQueryAudiosFrom(cursor: MPMediaQuery?) {
        Task {
            var listOfSongs: [[String: Any?]] = Array()

            // Load from Apple Music library (guard against nil items)
            if let items = cursor?.items {
                for song in items {
                    // If song file dont has an assetURL, it's a Cloud item
                    if !song.isCloudItem, song.assetURL != nil {
                        let songData = loadSongItem(song: song)
                        listOfSongs.append(songData)
                    }
                }
            }

            // If nothing came back from MediaLibrary, try the local Documents/Music folder.
            // This handles local-file album/artist IDs that aren't in Apple Music.
            if listOfSongs.isEmpty {
                let whereValue = self.args["where"] as Any
                listOfSongs = await LocalFileQuery().queryAudiosFrom(
                    type: self.type,
                    where: whereValue
                )
            }

            let finalList = formatSongList(args: self.args, allSongs: listOfSongs)
            
            DispatchQueue.main.async {
                self.result(finalList)
            }
        }
    }
    
    // Playlist loading doesnt use LocalFileQuery, so stays as DispatchQueue
    private func loadSongsFromPlaylist(cursor: [MPMediaItemCollection]?) {
        DispatchQueue.global(qos: .userInitiated).async {
            var listOfSongs: [[String: Any?]] = Array()
            
            // Here we need a different approach.
            //
            // First, query all playlists. After check if the argument is a:
            //   - [String]: The playlist [Name].
            //   - [Int]: The playlist [Id].
            //
            // Second, find the specific playlist using/comparing the argument.
            // After, query all item(song) from this playlist.
            for playlist in cursor ?? [] {
                let iPlaylist = playlist as! MPMediaPlaylist
                let iWhere = self.args["where"] as Any
                // Using this check we can define if [where] is the [Playlist] name or id
                if iWhere is String {
                    // Check if playlist name is equal to defined name
                    if iPlaylist.name == iWhere as? String {
                        // For each song, format and add to the list
                        for song in playlist.items {
                            // If the song file don't has a assetURL, is a Cloud item.
                            if !song.isCloudItem, song.assetURL != nil {
                                let songData = loadSongItem(song: song)
                                listOfSongs.append(songData)
                            }
                        }
                    }
                } else {
                    // Check if playlist id is equal to defined id
                    if iPlaylist.persistentID == iWhere as! Int {
                        // For each song, format and add to the list
                        for song in playlist.items {
                            // If the song file don't has a assetURL, is a Cloud item.
                            if !song.isCloudItem, song.assetURL != nil {
                                let songData = loadSongItem(song: song)
                                listOfSongs.append(songData)
                            }
                        }
                    }
                }
            }
            
            // After finish the "query", go back to the "main" thread(You can only call flutter
            // inside the main thread).
            DispatchQueue.main.async {
                // Here we'll check the "custom" sort and define a order to the list.
                let finalList = formatSongList(args: self.args, allSongs: listOfSongs)
                self.result(finalList)
            }
        }
    }
}
