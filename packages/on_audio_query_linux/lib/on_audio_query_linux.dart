/*
=============
Author: Mathis Laarmanns
Github: https://github.com/mathiiiiiis
Website: https://mathiiis.de/
=============
Plugin/Id: on_audio_query#0
Homepage: https://github.com/LucJosin/on_audio_query
Homepage(Linux): https://github.com/appsono/on_audio_query/tree/main/packages/on_audio_query_linux
Pub: https://pub.dev/packages/on_audio_query
License: https://github.com/LucJosin/on_audio_query/blob/main/on_audio_query/LICENSE
Copyright: © 2021, Lucas Josino. All rights reserved. © 2026, Mathis Laarmanns. All rights reserved.
=============
*/

library on_audio_query_linux;

import '../../on_audio_query_platform_interface/lib/on_audio_query_platform_interface.dart';

/// Linux implementation of the on_audio_query plugin.
///
/// This class is registered automatically by the Flutter plugin system
/// Actual implementation is in the native C++ code which communicates
/// via MethodChannel to provide audio querying functionality on Linux
class OnAudioQueryLinuxPlugin extends OnAudioQueryPlatform {
  /// Registers this class as the default instance of [OnAudioQueryPlatform]
  static void registerWith() {
    OnAudioQueryPlatform.instance = OnAudioQueryLinuxPlugin();
  }
}
