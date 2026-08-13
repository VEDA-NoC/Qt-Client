include("C:/Users/shini/Documents/QtProjects/qt_4ch_viewer/build-codex-mingw/.qt/QtDeploySupport.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/qt_4ch_viewer-plugins.cmake" OPTIONAL)
set(__QT_DEPLOY_I18N_CATALOGS "qtbase")

qt6_deploy_runtime_dependencies(
    EXECUTABLE "C:/Users/shini/Documents/QtProjects/qt_4ch_viewer/build-codex-mingw/qt_4ch_viewer.exe"
    GENERATE_QT_CONF
)
