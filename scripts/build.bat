@echo off
echo ========================================================
echo   Building 4K Video Downloader Plus C++ Desktop App
echo ========================================================

cd /d "%~dp0\.."

if not exist build mkdir build

echo [1/3] Running CMake configuration...
cmake -B build -G "MinGW Makefiles"
if %errorlevel% neq 0 (
    echo [ERROR] CMake configuration failed!
    exit /b %errorlevel%
)

echo [2/3] Compiling C++ Executable...
cmake --build build
if %errorlevel% neq 0 (
    echo [ERROR] Compilation failed!
    exit /b %errorlevel%
)

echo [3/3] Build complete successfully!
echo Executable generated at: build\YTDownloaderCore.exe
pause
