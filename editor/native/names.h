// Every name the user can see lives here, so renaming the product is a one-line change (the owner has not chosen the
// final name yet). Nothing else in editor/native spells these out.
#pragma once

namespace ote_native {

inline constexpr const char* kProductName = "OpenTacticsEngine Editor";
inline constexpr const char* kWindowTitle = "OpenTacticsEngine Editor (native, preview)";
// QSettings identity (also the folder name of the settings file).
inline constexpr const char* kSettingsOrganization = "OpenTacticsEngine";
inline constexpr const char* kSettingsApplication = "NativeEditor";

}  // namespace ote_native
