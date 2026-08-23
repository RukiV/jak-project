#pragma once

#include "common/listener_common.h"
#include "common/versions/versions.h"

//! Supported languages.
enum class Language {
  English = 0,
  French = 1,
  German = 2,
  Spanish = 3,
  Italian = 4,
  Japanese = 5,
  UK_English = 6,
  Portuguese = 9
};

struct GameLaunchOptions {
  GameVersion game_version = GameVersion::Jak1;
  bool disable_display = false;
  // -1 = per-game default (DECI2_PORT - 1 + game index). gk's main sets a real value
  // (the --port flag or that same arithmetic); embedded callers that never choose a
  // port, like the goalc test runner, must keep the per-game default or their
  // listener attach targets a different port than the runtime binds.
  int server_port = -1;
};
