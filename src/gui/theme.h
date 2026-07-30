#pragma once

#include <QString>

namespace bili::gui::theme {

inline const QString PINK          = QStringLiteral("#FB7299");
inline const QString PINK_HOVER    = QStringLiteral("#FF5C8A");
inline const QString PINK_PRESSED  = QStringLiteral("#E8457A");
inline const QString BLUE          = QStringLiteral("#00AEEC");
inline const QString BLUE_HOVER    = QStringLiteral("#0098D4");
inline const QString BLUE_LIGHT    = QStringLiteral("#E0F4FC");
inline const QString PINK_LIGHT    = QStringLiteral("#FFF0F3");
inline const QString BG            = QStringLiteral("#F7F6F9");
inline const QString CARD          = QStringLiteral("#FFFFFF");
inline const QString SURFACE_HOVER = QStringLiteral("#FAFAFD");
inline const QString TEXT          = QStringLiteral("#1F1F2E");
inline const QString TEXT_2        = QStringLiteral("#4A4A5A");
inline const QString TEXT_3        = QStringLiteral("#8A8A9A");
inline const QString BORDER        = QStringLiteral("#EBE8EF");
inline const QString SUCCESS       = QStringLiteral("#00B578");
inline const QString WARNING       = QStringLiteral("#FF9900");
inline const QString DANGER        = QStringLiteral("#FF4D4F");

QString globalStyleSheet();

} // namespace bili::gui::theme
