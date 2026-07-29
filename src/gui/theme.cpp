#include "theme.h"

namespace bili::gui::theme {

QString globalStyleSheet()
{
    return QStringLiteral(
        "QMainWindow { background-color: %1; }"
        "QLabel#title { color: %2; font-size: 18px; font-weight: 600; }"
        "QPushButton { border: none; border-radius: 8px; padding: 8px 16px; "
        "              font-size: 13px; font-weight: 500; color: %2; background-color: %3; }"
        "QPushButton:hover { background-color: %4; }"
        "QPushButton:pressed { background-color: %5; }"
        "QPushButton#primaryButton { background-color: %6; color: #FFFFFF; }"
        "QPushButton#primaryButton:hover { background-color: %7; }"
        "QPushButton#primaryButton:pressed { background-color: %8; }"
        "QPushButton#secondaryButton { background-color: #FFFFFF; color: %6; border: 1px solid %9; }"
        "QPushButton#secondaryButton:hover { background-color: %4; }"
        "QLineEdit { background-color: #FFFFFF; color: %2; border: 1px solid %9; "
        "            border-radius: 8px; padding: 6px 10px; font-size: 13px; }"
        "QLineEdit:focus { border: 1px solid %6; }"
        "QTableView { background-color: #FFFFFF; color: %2; border: 1px solid %9; "
        "             border-radius: 8px; gridline-color: %9; selection-background-color: %10; "
        "             selection-color: %2; alternate-background-color: %11; }"
        "QTableView::item { padding: 8px; border-bottom: 1px solid %9; }"
        "QHeaderView::section { background-color: %11; color: %2; padding: 8px; "
        "                       border: none; border-bottom: 1px solid %9; font-weight: 500; }"
        "QScrollArea { border: none; background-color: transparent; }"
        "QScrollBar:vertical { background-color: %11; width: 8px; border-radius: 4px; }"
        "QScrollBar::handle:vertical { background-color: %12; border-radius: 4px; min-height: 32px; }"
    ).arg(BG)
     .arg(TEXT)
     .arg(CARD)
     .arg(SURFACE_HOVER)
     .arg(BORDER)
     .arg(PINK)
     .arg(PINK_HOVER)
     .arg(PINK_PRESSED)
     .arg(BORDER)
     .arg(BLUE_LIGHT)
     .arg(SURFACE_HOVER)
     .arg(TEXT_3);
}

} // namespace bili::gui::theme
