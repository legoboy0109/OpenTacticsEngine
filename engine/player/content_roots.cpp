// Turns the platform's content roots (platform.h "Where things live") and the command line into the folders the
// player really reads: Args::gamesDir, assetsDir and extraGames. Precedence, highest first: --games / --assets /
// --extra-games, then --content-root (and OTE_CONTENT_ROOT / OTE_EXTRA_GAMES), then the platform's own contentRoot().
// With nothing given on desktop the result is the old "games" / "assets" relative to the working directory.
#include "args.h"
#include "platform/platform.h"

namespace ote {

void applyContentRoots(Args& a) {
  if (!a.contentRoot.empty()) setContentRootOverride(a.contentRoot);
  if (!a.extraGames.empty()) setExtraContentRootOverride(a.extraGames);
  const std::string root = contentRoot();
  if (!root.empty()) {
    if (!a.gamesGiven) a.gamesDir = root + "/games";
    if (!a.assetsGiven) a.assetsDir = root + "/assets";
  }
  if (a.extraGames.empty()) a.extraGames = extraContentRoot();
}

}  // namespace ote
