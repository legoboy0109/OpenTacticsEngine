// The command line that "Play" runs: the player program, pointed at the open game folder. The player's --game takes a
// game ID inside a games folder, not a path, so a project folder /x/y/my-game becomes `--games /x/y --game my-game`.
#pragma once
#include <QString>
#include <QStringList>
#include <functional>

namespace ote_native {

struct PlayCommand {
  QString program;
  QStringList args;
  QString workingDir;
  QStringList toList() const { return QStringList{program} + args; }
};

// `playerOverride` (from the settings) wins; else ote-player next to the editor program (`appDir`). The assets folder is
// passed along when one sits next to the player (installed layout) or one level up (a build folder inside the repo).
PlayCommand makePlayCommand(const QString& gameFolder, const QString& playerOverride, const QString& appDir);

// Starts it as a separate, detached process. Tests install a hook instead, so no player is ever started from a test.
bool startPlayer(const PlayCommand& cmd);

}  // namespace ote_native
