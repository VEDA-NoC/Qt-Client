@echo off
setlocal
set "REPO_ROOT=%~dp0.."
set "PATH=C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\mingw1310_64\bin;C:\Qt\6.11.0\mingw_64\bin;%PATH%"
pushd "%REPO_ROOT%"

echo Configuring the Release performance build...
cmake -S . -B build-perf -G "MinGW Makefiles" ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_PREFIX_PATH=C:/Qt/6.11.0/mingw_64 ^
  -DCMAKE_TOOLCHAIN_FILE=C:/dev/vcpkg/scripts/buildsystems/vcpkg.cmake ^
  -DVCPKG_INSTALLED_DIR=C:/dev/vcpkg/installed ^
  -DVCPKG_TARGET_TRIPLET=x64-mingw-dynamic
if errorlevel 1 goto :failed

echo Building the latest Release executable...
cmake --build build-perf --parallel 8
if errorlevel 1 goto :failed

powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0collect_qt_perf.ps1" -ExePath "%REPO_ROOT%\build-perf\qt_4ch_viewer.exe" %*
set "QT_PERF_EXIT=%ERRORLEVEL%"
popd
echo.
if not "%QT_PERF_EXIT%"=="0" echo Measurement failed with exit code %QT_PERF_EXIT%.
pause
exit /b %QT_PERF_EXIT%

:failed
set "QT_PERF_EXIT=%ERRORLEVEL%"
popd
echo.
echo Release build failed with exit code %QT_PERF_EXIT%.
pause
exit /b %QT_PERF_EXIT%
