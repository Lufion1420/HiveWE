#pragma once

#include <QIcon>
#include <QString>

/// Application themes. "Dark" and "Light" are the old data/themes/<name>.qss files. "Flat Dark" and "Flat Light" are built from
/// data/themes/Flat.tokens and data/themes/Flat.qss.in (see src/utilities/theme_tokens.ixx), with the Inter font and a recoloured palette.
namespace theme {
inline const QString flat_dark = QStringLiteral("Flat Dark");
inline const QString flat_light = QStringLiteral("Flat Light");

/// True for "Flat Dark" and "Flat Light".
bool is_flat(const QString& name);

/// Name of the theme applied last (empty before the first apply).
const QString& current();
bool current_is_flat();

/// Applies the theme to the whole application: style sheet, palette and font. Returns an empty string on success, otherwise the error text.
/// On failure the previous theme stays active.
QString apply(const QString& name);

/// Re-applies the theme from the QSettings key "theme" (default "Dark"). Used by the F5 reload and the settings dialog.
QString apply_from_settings();

/// The SVG icon data/icons/flat/<name>.svg drawn in the active theme's text colour (disabled colour for the disabled state).
/// An empty QIcon when the file is missing.
QIcon icon(const QString& name);
}
