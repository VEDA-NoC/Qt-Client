#include "main_window.h"

#include "theme.h"
#include "app_logging.h"

#include <QApplication>
#include <QCoreApplication>
#include <QIcon>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName("VEDA VMS Console");
    QCoreApplication::setApplicationVersion(VEDA_VMS_VERSION);
    AppLogging::install();
    app.setWindowIcon(QIcon(":/icons/app-icon.png"));
    AppTheme::loadBundledFonts();
    AppTheme::apply(app);

    MainWindow window;
    window.setMinimumSize(1280, 720);
    window.resize(1600, 900);
    window.showMaximized();

    return QApplication::exec();
}
