#include "theme.h"

#include <QApplication>
#include <QDebug>
#include <QFont>
#include <QFontDatabase>
#include <QStringList>

namespace AppTheme {

bool loadBundledFonts() {
    const QStringList resources = {
        ":/fonts/Pretendard-Regular.ttf",
        ":/fonts/Pretendard-Medium.ttf",
        ":/fonts/Pretendard-SemiBold.ttf",
        ":/fonts/Pretendard-Bold.ttf",
    };

    bool all_loaded = true;
    QStringList loaded_families;
    for (const QString &resource : resources) {
        const int font_id = QFontDatabase::addApplicationFont(resource);
        if (font_id < 0) {
            all_loaded = false;
            qWarning().noquote() << "[ui] bundled font load failed:" << resource;
            continue;
        }
        loaded_families.append(QFontDatabase::applicationFontFamilies(font_id));
    }
    loaded_families.removeDuplicates();
    qInfo().noquote() << "[ui] bundled font families=" << loaded_families.join(", ");
    return all_loaded;
}

void apply(QApplication &application) {
    QFont font("Pretendard");
    if (!QFontDatabase::families().contains("Pretendard")) {
        font.setFamilies({"Segoe UI", "Malgun Gothic", "sans-serif"});
    }
    font.setPointSizeF(10.0);
    font.setStyleStrategy(static_cast<QFont::StyleStrategy>(
        QFont::PreferAntialias | QFont::PreferQuality));
    application.setFont(font);

    application.setStyleSheet(R"QSS(
        * {
            color: #25283D;
            font-family: "Pretendard", "Segoe UI", "Malgun Gothic", sans-serif;
            font-size: 10pt;
            selection-background-color: #F89B6C;
            selection-color: #1D1E37;
        }
        QMainWindow, QWidget#appRoot, QStackedWidget#pageStack,
        QWidget#liveGridHost, QWidget#liveGridViewport,
        QScrollArea#liveGridScroll { background: #F4F5F7; }
        QWidget#sidebar { background: #1D1E37; }
        QLabel#brandTitle { color: #FFFFFF; font-size: 17pt; font-weight: 700; }
        QLabel#brandSubtitle, QLabel#sidebarCaption { color: #989BAE; font-size: 8pt; }
        QToolButton[nav="true"] {
            background: transparent; border: 0; border-left: 4px solid transparent; border-radius: 0;
            color: #C8CAD5; font-size: 10pt; font-weight: 500; min-height: 46px;
            padding: 0 16px; text-align: left;
        }
        QToolButton[nav="true"]:hover { background: #292B49; color: #FFFFFF; }
        QToolButton[nav="true"]:checked {
            background: #303252; border-left-color: #F37321; color: #FFFFFF; font-weight: 600;
        }
        QToolButton[sidebarAction="true"] {
            background: #292B49; border: 1px solid #40436B; border-radius: 6px;
            color: #E3E4EC; font-size: 9.5pt; font-weight: 600; min-height: 38px;
            padding: 0 14px; text-align: center; margin: 4px 16px 0 16px;
        }
        QToolButton[sidebarAction="true"]:hover { background: #34365A; border-color: #52558A; }
        QToolButton[sidebarAction="true"][critical="true"] {
            background: transparent; border-color: #7A3A3E; color: #F2A1A6;
        }
        QToolButton[sidebarAction="true"][critical="true"]:hover {
            background: #4A2529; border-color: #B42318; color: #FFFFFF;
        }
        QToolButton[legalSectionToggle="true"] {
            background: #F7F7F9; border: 1px solid #E1E3E9; border-radius: 6px;
            color: #353968; font-weight: 600; min-height: 34px; padding: 0 10px;
            text-align: left;
        }
        QToolButton[legalSectionToggle="true"]:hover {
            border-color: #F37321; color: #D85D10;
        }
        QFrame#topBar, QFrame#bottomBar { background: #FFFFFF; border: 0; }
        QFrame#topBar { border-bottom: 1px solid #E2E4EA; }
        QFrame#bottomBar { border-top: 1px solid #E2E4EA; }
        QWidget#footerAccount { border-right: 1px solid #E2E4EA; }
        QLabel[footerConnection="true"] { font-size: 8pt; }
        QLabel[footerConnection="true"][severity="ok"] { color: #18794E; }
        QLabel[footerConnection="true"][severity="warning"] { color: #B54708; }
        QLabel[footerConnection="true"][severity="critical"] { color: #B42318; }
        QLabel#pageTitle { color: #1D1E37; font-size: 16pt; font-weight: 700; }
        QLabel#pageSubtitle, QLabel[muted="true"] { color: #71758A; }
        QFrame[card="true"] { background: #FFFFFF; border: 1px solid #E1E3E9; border-radius: 10px; }
        QLabel[sectionTitle="true"] { color: #1D1E37; font-size: 10.5pt; font-weight: 600; }
        QLabel[value="true"] { color: #1D1E37; font-size: 14pt; font-weight: 700; }
        QLabel[badge="true"] {
            background: #EEF0F4; border-radius: 10px; color: #5B6075; font-size: 8.5pt;
            font-weight: 600; padding: 3px 9px;
        }
        QLabel[badge="true"][severity="ok"] { background: #E7F5EE; color: #18794E; }
        QLabel[badge="true"][severity="warning"] { background: #FFF1E7; color: #B54708; }
        QLabel[badge="true"][severity="critical"] { background: #FDEBEC; color: #B42318; }
        QLabel[settingsFeedback="true"][severity="ok"] { color: #18794E; }
        QLabel[settingsFeedback="true"][severity="warning"] { color: #B54708; }
        QLabel[settingsFeedback="true"][severity="critical"] { color: #B42318; font-weight: 600; }
        QLabel[legalValue="true"] { color: #44485D; }
        QLabel[legalValue="true"][missing="true"] { color: #B54708; font-style: italic; }
        QLabel[legalLink="true"] { color: #D85D10; }
        QPushButton {
            background: #FFFFFF; border: 1px solid #CDD0D9; border-radius: 6px;
            min-height: 34px; padding: 0 15px; font-weight: 600;
        }
        QPushButton:hover { border-color: #F37321; color: #D85D10; }
        QPushButton:disabled { background: #F0F1F4; border-color: #E0E2E7; color: #A5A8B4; }
        QPushButton[primary="true"] { background: #F37321; border-color: #F37321; color: #FFFFFF; }
        QPushButton[primary="true"]:hover { background: #D85D10; border-color: #D85D10; color: #FFFFFF; }
        QPushButton[primary="true"]:disabled { background: #F5C7A6; border-color: #F5C7A6; color: #FFFFFF; }
        QLineEdit, QComboBox, QDateEdit, QTimeEdit {
            background: #FFFFFF; border: 1px solid #CDD0D9; border-radius: 6px;
            min-height: 34px; padding: 0 10px;
        }
        QLineEdit:focus, QComboBox:focus, QDateEdit:focus, QTimeEdit:focus { border: 1px solid #F37321; }
        QCalendarWidget QWidget { background: #FFFFFF; color: #25283D; }
        QCalendarWidget QToolButton {
            background: #FFFFFF; border: 0; color: #25283D; font-weight: 600;
        }
        QCalendarWidget QAbstractItemView {
            background: #FFFFFF; alternate-background-color: #F7F7F9;
            color: #25283D; selection-background-color: #F37321;
            selection-color: #FFFFFF; outline: 0;
        }
        QSlider::groove:horizontal {
            background: #DDE0E7; height: 4px; border-radius: 2px;
        }
        QSlider::sub-page:horizontal {
            background: #F37321; height: 4px; border-radius: 2px;
        }
        QSlider::handle:horizontal {
            background: #F37321; border: 2px solid #FFFFFF;
            width: 14px; height: 14px; margin: -6px 0; border-radius: 8px;
        }
        QFrame[notice="warning"] { background: #FFF7ED; border: 1px solid #FED7AA; border-radius: 8px; }
        QLabel[noticeTitle="true"] { color: #9A3412; font-weight: 700; }
        QLabel[emptyTitle="true"] { color: #44485D; font-size: 10.5pt; font-weight: 600; }
        QWidget[videoPanel="true"] { background: #FFFFFF; border: 1px solid #DDE0E7; border-radius: 8px; }
        QLabel[videoSurface="true"] { background: #11131B; border: 0; color: #858A9C; }
        QLabel[streamState="online"] { color: #18794E; font-weight: 600; }
        QLabel[streamState="pending"] { color: #B54708; font-weight: 600; }
        QLabel[streamState="error"] { color: #B42318; font-weight: 600; }
        QLabel[streamState="offline"] { color: #777B8E; font-weight: 600; }
        QTableWidget { background: #FFFFFF; border: 1px solid #E1E3E9; border-radius: 8px; gridline-color: #ECEEF2; }
        QHeaderView::section {
            background: #F7F7F9; border: 0; border-bottom: 1px solid #E1E3E9;
            color: #5F6377; font-weight: 600; min-height: 36px; padding: 0 10px;
        }
        QToolTip { background: #1D1E37; border: 1px solid #353968; color: #FFFFFF; padding: 5px; }
    )QSS");
}

}  // namespace AppTheme
