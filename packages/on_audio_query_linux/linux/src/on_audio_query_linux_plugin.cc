#include "on_audio_query_linux/on_audio_query_linux_plugin.h"

#include <flutter_linux/flutter_linux.h>
#include <gtk/gtk.h>
#include <sys/utsname.h>

#include <cstring>
#include <thread>
#include <iostream>
#include <filesystem>

#include "core/database_manager.h"
#include "core/ffprobe_extractor.h"
#include "core/thread_pool.h"
#include "scanner/file_scanner.h"
#include "scanner/scan_coordinator.h"
#include "queries/audio_query.h"
#include "queries/album_query.h"
#include "queries/artist_query.h"
#include "queries/genre_query.h"
#include "queries/playlist_query.h"
#include "queries/artwork_query.h"
#include "queries/audios_from_query.h"
#include "queries/with_filters_query.h"
#include "queries/folder_query.h"

using namespace on_audio_query_linux;

#define ON_AUDIO_QUERY_LINUX_PLUGIN(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), on_audio_query_linux_plugin_get_type(), \
                               OnAudioQueryLinuxPlugin))

struct _OnAudioQueryLinuxPlugin {
  GObject parent_instance;

  // Core components
  DatabaseManager* db_manager;
  FFprobeExtractor* ffprobe;
  ThreadPool* thread_pool;
  ScanCoordinator* scan_coordinator;
};

G_DEFINE_TYPE(OnAudioQueryLinuxPlugin, on_audio_query_linux_plugin, g_object_get_type())

// Handle method calls from Dart
static void on_audio_query_linux_plugin_handle_method_call(
    OnAudioQueryLinuxPlugin* self,
    FlMethodCall* method_call) {
  g_autoptr(FlMethodResponse) response = nullptr;

  const gchar* method = fl_method_call_get_name(method_call);

  // Permission methods (always return true on Linux)
  if (strcmp(method, "permissionsStatus") == 0) {
    g_autoptr(FlValue) result = fl_value_new_bool(TRUE);
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(result));
  } else if (strcmp(method, "permissionsRequest") == 0) {
    g_autoptr(FlValue) result = fl_value_new_bool(TRUE);
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(result));
  }
  // Device info
  else if (strcmp(method, "queryDeviceInfo") == 0) {
    struct utsname uts_info;
    uname(&uts_info);

    g_autoptr(FlValue) result_map = fl_value_new_map();
    fl_value_set_string_take(result_map, "device_sys_type",
                            fl_value_new_string("Linux"));
    fl_value_set_string_take(result_map, "device_type",
                            fl_value_new_string(uts_info.machine));
    fl_value_set_string_take(result_map, "device_release",
                            fl_value_new_string(uts_info.release));
    fl_value_set_string_take(result_map, "device_sdk",
                            fl_value_new_int(0)); // Not applicable on Linux

    response = FL_METHOD_RESPONSE(fl_method_success_response_new(result_map));
  }
  // Query methods
  else if (strcmp(method, "querySongs") == 0) {
    FlValue* args = fl_method_call_get_args(method_call);
    QueryParams params;

    if (args && fl_value_get_type(args) == FL_VALUE_TYPE_MAP) {
      FlValue* sort_type_val = fl_value_lookup_string(args, "sortType");
      FlValue* order_type_val = fl_value_lookup_string(args, "orderType");

      if (sort_type_val) {
        int sort_type = fl_value_get_int(sort_type_val);
        params.sort_type = static_cast<QueryParams::SortType>(sort_type);
      }

      if (order_type_val) {
        int order_type = fl_value_get_int(order_type_val);
        params.order_type = static_cast<QueryParams::OrderType>(order_type);
      }
    }

    AudioQuery query(self->db_manager, params);
    FlValue* result = query.Execute();
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(result));
  } else if (strcmp(method, "queryAlbums") == 0) {
    FlValue* args = fl_method_call_get_args(method_call);
    QueryParams params;

    if (args && fl_value_get_type(args) == FL_VALUE_TYPE_MAP) {
      FlValue* artist_id_val = fl_value_lookup_string(args, "artistId");
      FlValue* order_type_val = fl_value_lookup_string(args, "orderType");

      if (artist_id_val && fl_value_get_type(artist_id_val) == FL_VALUE_TYPE_INT) {
        int64_t artist_id = fl_value_get_int(artist_id_val);
        params.artist_filter = artist_id;
      }

      if (order_type_val && fl_value_get_type(order_type_val) == FL_VALUE_TYPE_INT) {
        int order_type = fl_value_get_int(order_type_val);
        params.order_type = static_cast<QueryParams::OrderType>(order_type);
      }
    }

    AlbumQuery query(self->db_manager, params);
    FlValue* result = query.Execute();
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(result));
  } else if (strcmp(method, "queryArtists") == 0) {
    ArtistQuery query(self->db_manager);
    FlValue* result = query.Execute();
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(result));
  } else if (strcmp(method, "queryGenres") == 0) {
    QueryParams params;  // Default params
    GenreQuery query(self->db_manager, params);
    FlValue* result = query.Execute();
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(result));
  } else if (strcmp(method, "queryPlaylists") == 0) {
    PlaylistQuery query(self->db_manager);
    FlValue* result = query.Execute();
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(result));
  } else if (strcmp(method, "queryAudiosFrom") == 0) {
    FlValue* args = fl_method_call_get_args(method_call);
    int64_t id = 0;
    int type = 0;

    if (args && fl_value_get_type(args) == FL_VALUE_TYPE_MAP) {
      FlValue* id_val = fl_value_lookup_string(args, "where");
      FlValue* type_val = fl_value_lookup_string(args, "type");
      if (id_val) id = fl_value_get_int(id_val);
      if (type_val) type = fl_value_get_int(type_val);
    }

    AudiosFromQuery query(self->db_manager, id, type);
    FlValue* result = query.Execute();
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(result));
  } else if (strcmp(method, "queryWithFilters") == 0) {
    FlValue* args = fl_method_call_get_args(method_call);
    std::string search = "";

    if (args && fl_value_get_type(args) == FL_VALUE_TYPE_MAP) {
      FlValue* search_val = fl_value_lookup_string(args, "argsVal");
      if (search_val && fl_value_get_type(search_val) == FL_VALUE_TYPE_STRING) {
        search = fl_value_get_string(search_val);
      }
    }

    WithFiltersQuery query(self->db_manager, search);
    FlValue* result = query.Execute();
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(result));
  } else if (strcmp(method, "queryFromFolder") == 0) {
    FlValue* args = fl_method_call_get_args(method_call);
    std::string path = "";

    if (args && fl_value_get_type(args) == FL_VALUE_TYPE_MAP) {
      FlValue* path_val = fl_value_lookup_string(args, "path");
      if (path_val && fl_value_get_type(path_val) == FL_VALUE_TYPE_STRING) {
        path = fl_value_get_string(path_val);
      }
    }

    FolderQuery query(self->db_manager, path);
    FlValue* result = query.Execute();
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(result));
  } else if (strcmp(method, "queryAllPath") == 0) {
    auto paths = self->db_manager->GetAllSongPaths();
    FlValue* result = fl_value_new_list();

    for (const auto& path : paths) {
      fl_value_append_take(result, fl_value_new_string(path.c_str()));
    }

    response = FL_METHOD_RESPONSE(fl_method_success_response_new(result));
  } else if (strcmp(method, "queryArtwork") == 0) {
    FlValue* args = fl_method_call_get_args(method_call);
    int64_t id = 0;
    int type = 0;
    std::string format = "jpeg";

    if (args && fl_value_get_type(args) == FL_VALUE_TYPE_MAP) {
      FlValue* id_val = fl_value_lookup_string(args, "id");
      FlValue* type_val = fl_value_lookup_string(args, "type");
      FlValue* format_val = fl_value_lookup_string(args, "format");

      if (id_val) id = fl_value_get_int(id_val);
      if (type_val) type = fl_value_get_int(type_val);
      if (format_val && fl_value_get_type(format_val) == FL_VALUE_TYPE_STRING) {
        format = fl_value_get_string(format_val);
      }
    }

    ArtworkQuery query(self->db_manager, self->ffprobe, id, type, format);
    FlValue* result = query.Execute();
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(result));
  } else if (strcmp(method, "scanMedia") == 0) {
    // Trigger incremental scan in background
    FileScanner scanner;
    std::string music_dir = scanner.GetDefaultMusicDirectory();
    self->scan_coordinator->AsyncScan(music_dir, true, nullptr);

    g_autoptr(FlValue) result = fl_value_new_bool(TRUE);
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(result));
  }
  // Playlist methods
  else if (strcmp(method, "createPlaylist") == 0) {
    FlValue* args = fl_method_call_get_args(method_call);
    std::string name = "";

    if (args && fl_value_get_type(args) == FL_VALUE_TYPE_MAP) {
      FlValue* name_val = fl_value_lookup_string(args, "name");
      if (name_val && fl_value_get_type(name_val) == FL_VALUE_TYPE_STRING) {
        name = fl_value_get_string(name_val);
      }
    }

    int64_t id = self->db_manager->CreatePlaylist(name);
    g_autoptr(FlValue) result = fl_value_new_bool(id > 0);
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(result));
  } else if (strcmp(method, "removePlaylist") == 0) {
    FlValue* args = fl_method_call_get_args(method_call);
    int64_t id = 0;

    if (args && fl_value_get_type(args) == FL_VALUE_TYPE_MAP) {
      FlValue* id_val = fl_value_lookup_string(args, "playlistId");
      if (id_val) id = fl_value_get_int(id_val);
    }

    bool success = self->db_manager->DeletePlaylist(id);
    g_autoptr(FlValue) result = fl_value_new_bool(success);
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(result));
  } else if (strcmp(method, "addToPlaylist") == 0) {
    FlValue* args = fl_method_call_get_args(method_call);
    int64_t playlist_id = 0, audio_id = 0;

    if (args && fl_value_get_type(args) == FL_VALUE_TYPE_MAP) {
      FlValue* pid = fl_value_lookup_string(args, "playlistId");
      FlValue* aid = fl_value_lookup_string(args, "audioId");
      if (pid) playlist_id = fl_value_get_int(pid);
      if (aid) audio_id = fl_value_get_int(aid);
    }

    bool success = self->db_manager->AddToPlaylist(playlist_id, audio_id);
    g_autoptr(FlValue) result = fl_value_new_bool(success);
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(result));
  } else if (strcmp(method, "removeFromPlaylist") == 0) {
    FlValue* args = fl_method_call_get_args(method_call);
    int64_t playlist_id = 0, audio_id = 0;

    if (args && fl_value_get_type(args) == FL_VALUE_TYPE_MAP) {
      FlValue* pid = fl_value_lookup_string(args, "playlistId");
      FlValue* aid = fl_value_lookup_string(args, "audioId");
      if (pid) playlist_id = fl_value_get_int(pid);
      if (aid) audio_id = fl_value_get_int(aid);
    }

    bool success = self->db_manager->RemoveFromPlaylist(playlist_id, audio_id);
    g_autoptr(FlValue) result = fl_value_new_bool(success);
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(result));
  } else if (strcmp(method, "renamePlaylist") == 0) {
    FlValue* args = fl_method_call_get_args(method_call);
    int64_t id = 0;
    std::string name = "";

    if (args && fl_value_get_type(args) == FL_VALUE_TYPE_MAP) {
      FlValue* id_val = fl_value_lookup_string(args, "playlistId");
      FlValue* name_val = fl_value_lookup_string(args, "newName");
      if (id_val) id = fl_value_get_int(id_val);
      if (name_val && fl_value_get_type(name_val) == FL_VALUE_TYPE_STRING) {
        name = fl_value_get_string(name_val);
      }
    }

    bool success = self->db_manager->RenamePlaylist(id, name);
    g_autoptr(FlValue) result = fl_value_new_bool(success);
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(result));
  } else if (strcmp(method, "moveItemTo") == 0) {
    FlValue* args = fl_method_call_get_args(method_call);
    int64_t playlist_id = 0;
    int from_pos = 0;
    int to_pos = 0;

    if (args && fl_value_get_type(args) == FL_VALUE_TYPE_MAP) {
      FlValue* pid = fl_value_lookup_string(args, "playlistId");
      FlValue* from = fl_value_lookup_string(args, "from");
      FlValue* to = fl_value_lookup_string(args, "to");
      if (pid) playlist_id = fl_value_get_int(pid);
      if (from) from_pos = fl_value_get_int(from);
      if (to) to_pos = fl_value_get_int(to);
    }

    bool success = self->db_manager->MovePlaylistItem(playlist_id, from_pos, to_pos);
    g_autoptr(FlValue) result = fl_value_new_bool(success);
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(result));
  }
  // Method not implemented
  else {
    response = FL_METHOD_RESPONSE(fl_method_not_implemented_response_new());
  }

  fl_method_call_respond(method_call, response, nullptr);
}

// Method call handler wrapper
static void method_call_cb(FlMethodChannel* channel, FlMethodCall* method_call,
                          gpointer user_data) {
  (void)channel;
  OnAudioQueryLinuxPlugin* plugin = ON_AUDIO_QUERY_LINUX_PLUGIN(user_data);
  on_audio_query_linux_plugin_handle_method_call(plugin, method_call);
}

static void on_audio_query_linux_plugin_dispose(GObject* object) {
  OnAudioQueryLinuxPlugin* self = ON_AUDIO_QUERY_LINUX_PLUGIN(object);

  // Cleanup
  delete self->scan_coordinator;
  delete self->thread_pool;
  delete self->ffprobe;
  delete self->db_manager;

  G_OBJECT_CLASS(on_audio_query_linux_plugin_parent_class)->dispose(object);
}

static void on_audio_query_linux_plugin_class_init(OnAudioQueryLinuxPluginClass* klass) {
  G_OBJECT_CLASS(klass)->dispose = on_audio_query_linux_plugin_dispose;
}

static void on_audio_query_linux_plugin_init(OnAudioQueryLinuxPlugin* self) {
  std::cout << "[Plugin] Initializing on_audio_query_linux..." << std::endl;

  // Setup database path
  const char* home = getenv("HOME");
  std::string db_path = std::string(home ? home : "/tmp") + "/.local/share/on_audio_query/metadata.db";

  // Initialize core components
  self->db_manager = new DatabaseManager(db_path);
  self->db_manager->Initialize();

  self->ffprobe = new FFprobeExtractor();
  self->thread_pool = new ThreadPool(std::thread::hardware_concurrency());
  self->scan_coordinator = new ScanCoordinator(
    self->db_manager,
    self->ffprobe,
    self->thread_pool
  );

  // Check if initial scan is needed (database empty)
  if (self->db_manager->IsDatabaseEmpty()) {
    std::cout << "[Plugin] Database empty - starting initial scan in background..." << std::endl;

    // Run initial scan in background thread
    std::thread([self]() {
      FileScanner scanner;
      std::string music_dir = scanner.GetDefaultMusicDirectory();
      self->scan_coordinator->FullScan(music_dir, nullptr);
      std::cout << "[Plugin] Initial scan complete!" << std::endl;
    }).detach();
  } else {
    std::cout << "[Plugin] Database loaded with " << self->db_manager->GetSongCount() << " songs" << std::endl;
  }

  std::cout << "[Plugin] Initialization complete!" << std::endl;
}

void on_audio_query_linux_plugin_register_with_registrar(FlPluginRegistrar* registrar) {
  OnAudioQueryLinuxPlugin* plugin = ON_AUDIO_QUERY_LINUX_PLUGIN(
      g_object_new(on_audio_query_linux_plugin_get_type(), nullptr));

  g_autoptr(FlStandardMethodCodec) codec = fl_standard_method_codec_new();
  g_autoptr(FlMethodChannel) channel =
      fl_method_channel_new(fl_plugin_registrar_get_messenger(registrar),
                           "com.lucasjosino.on_audio_query",
                           FL_METHOD_CODEC(codec));
  fl_method_channel_set_method_call_handler(channel, method_call_cb,
                                           g_object_ref(plugin),
                                           g_object_unref);

  g_object_unref(plugin);
}
