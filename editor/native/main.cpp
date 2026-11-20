// Entry point of the native editor.
//   ote-editor-native [GAME_FOLDER] [--settings-file FILE] [--schema-dir FOLDER]
// Without a folder it reopens the project of the last run. --settings-file keeps all remembered state (recent projects,
// window layout, player path) in that ini file instead of the user's normal settings; scripts and tests use it.
#include <QApplication>
#include <QCommandLineParser>
#include <QSettings>
#include <memory>

#include "main_window.h"
#include "names.h"

int main(int argc, char** argv) {
  QApplication app(argc, argv);
  QApplication::setApplicationName(ote_native::kSettingsApplication);
  QApplication::setOrganizationName(ote_native::kSettingsOrganization);
  QApplication::setApplicationDisplayName(ote_native::kProductName);

  QCommandLineParser parser;
  parser.setApplicationDescription(ote_native::kProductName);
  parser.addHelpOption();
  parser.addPositionalArgument("folder", "A game folder (with a game.json) to open.");
  QCommandLineOption settingsOpt("settings-file", "Keep remembered state in this ini file.", "file");
  parser.addOption(settingsOpt);
  QCommandLineOption schemaOpt("schema-dir", "Read the form schemas from this folder instead of the built-in ones.", "folder");
  parser.addOption(schemaOpt);
  parser.process(app);

  std::unique_ptr<QSettings> settings;
  if (parser.isSet(settingsOpt)) settings = std::make_unique<QSettings>(parser.value(settingsOpt), QSettings::IniFormat);
  else settings = std::make_unique<QSettings>();

  ote_native::MainWindow window(settings.get());
  if (parser.isSet(schemaOpt)) window.setSchemaDir(parser.value(schemaOpt));
  window.show();
  const QStringList args = parser.positionalArguments();
  if (!args.isEmpty()) window.openProject(args.first());
  else window.restoreLastProject();
  return app.exec();
}
