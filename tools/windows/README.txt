OpenTacticsEngine - Windows (64-bit)
===================================

Start: double-click ote-player.exe. Nothing to install; this folder can be anywhere (a USB stick is fine).
Needs Windows 10 or 11, 64-bit. A 3D-capable graphics driver is only needed for the optional hardware 3D drawing;
without one the game draws its 3D views in software.

What is in here
  ote-player.exe   the game player (a window opens, no console)
  SDL3.dll         SDL 3, the library that talks to the window, keyboard, controller and sound
  games\           the games the launcher offers (three demo games). Put another game's folder in here
                   (it holds a game.json) and the launcher lists it.
  assets\          fonts and launcher sounds
  licenses\        the licences of the engine and the parts it is built from

Your settings and saves are kept in your user profile, not in this folder:
  %APPDATA%\OpenTacticsEngine\player\     (settings.json, saves\)
Delete that folder to start fresh.

Controls: keyboard, mouse or a game controller (arrows/d-pad move, Enter/A confirm, Esc/B back).

From a terminal the player prints its log and accepts options, for example:
  ote-player.exe --help
  ote-player.exe --headless --no-audio --no-settings --no-saving --frames 3

OpenTacticsEngine is free software under the GNU General Public License v3.0 (licenses\GPL-3.0.txt).
Source: https://github.com/legoboy0109/OpenTacticsEngine
