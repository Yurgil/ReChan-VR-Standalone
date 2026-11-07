#pragma once
// Game title
#if defined(RC_PLATFORM_QUEST) && defined(RECHAN_QUEST_VERSION)
#define JCS_TITLE "ReChan Quest"
#define GAME_VERSION RECHAN_QUEST_VERSION
#else
#define JCS_TITLE "ReChan"
#define GAME_VERSION "v1.0.2"
#endif
#define UPDATE_REPO_OWNER "SilverwireGames"
#define UPDATE_REPO_NAME "ReChan"

inline constexpr const char* LEGAL_TEXT = R"(
LEGAL NOTICE

ReChan is an independent, unofficial reimplementation project for Jackie Chan Stuntmaster.

No original game assets, disc images, ROM data, videos, music, voice recordings, textures, models, levels, or other copyrighted game content are included or distributed with ReChan. A legally obtained copy of the original game is required.

ReChan is provided free of charge. The source code developed for this project is released under the MIT License.

Jackie Chan Stuntmaster and all related trademarks, copyrights, characters, names, logos, assets, and original game data remain the property of their respective owners.

ReChan is not affiliated with, endorsed by, sponsored by, or approved by the original developers, publishers, license holders, or rights holders.

For updates and more information:
silverwiregames.com/rechan

© 2026 Silverwire Games
)";
