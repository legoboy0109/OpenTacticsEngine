#include "play_command.h"

#include <QDir>
#include <QFileInfo>
#include <QProcess>

namespace ote_native {

PlayCommand makePlayCommand(const QString& gameFolder, const QString& playerOverride, const QString& appDir) {
  PlayCommand c;
  if (!playerOverride.isEmpty()) {
    c.program = playerOverride;
  } else {
#ifdef Q_OS_WIN
    const QString exe = "ote-player.exe";
#else
    const QString exe = "ote-player";
#endif
    c.program = QDir(appDir).filePath(exe);
    // A build tree keeps the two programs apart (<build>/editor/native and <build>/engine/player): look there too, so
    // Play works straight from a developer build without a setting.
    const QString inBuildTree = QDir(appDir).filePath("../../engine/player/" + exe);
    if (!QFileInfo::exists(c.program) && QFileInfo::exists(inBuildTree)) c.program = QDir::cleanPath(inBuildTree);
  }
  const QFileInfo game(QDir::cleanPath(gameFolder));
  c.args << "--games" << game.absolutePath() << "--game" << game.fileName();
  const QDir playerDir = QFileInfo(c.program).absoluteDir();
  // Next to the player, next to the editor, or up to four folders above the player (the source tree around a build tree).
  for (const QDir& d : {playerDir, QDir(appDir), QDir(playerDir.filePath("..")), QDir(playerDir.filePath("../..")),
                        QDir(playerDir.filePath("../../..")), QDir(playerDir.filePath("../../../.."))}) {
    if (QFileInfo(d.filePath("assets/fonts")).isDir()) {
      c.args << "--assets" << QDir::cleanPath(d.filePath("assets"));
      break;
    }
  }
  c.workingDir = playerDir.absolutePath();
  return c;
}

bool startPlayer(const PlayCommand& cmd) {
  return QProcess::startDetached(cmd.program, cmd.args, cmd.workingDir);
}

}  // namespace ote_native
