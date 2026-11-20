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
    c.program = QDir(appDir).filePath("ote-player.exe");
#else
    c.program = QDir(appDir).filePath("ote-player");
#endif
  }
  const QFileInfo game(QDir::cleanPath(gameFolder));
  c.args << "--games" << game.absolutePath() << "--game" << game.fileName();
  const QDir playerDir = QFileInfo(c.program).absoluteDir();
  for (const QDir& d : {playerDir, QDir(appDir), QDir(playerDir.filePath(".."))}) {
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
