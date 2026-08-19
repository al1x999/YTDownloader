@echo off
echo ========================================================
echo   Packaging 4K Video Downloader Plus Desktop Installer
echo ========================================================

cd /d "%~dp0"

if not exist "..\dist" mkdir "..\dist"

where ISCC >nul 2>nul
if %errorlevel% equ 0 (
    echo Compiling Inno Setup Script...
    ISCC installer_script.iss
    echo Setup Executable created in dist\4K_Video_Downloader_Plus_Setup.exe
) else (
    echo [INFO] Inno Setup compiler (ISCC) not found in PATH.
    echo Creating Portable Zip Distribution...
    powershell -Command "Compress-Archive -Path '..\build\YTDownloaderCore.exe', '..\ui' -DestinationPath '..\dist\4K_Video_Downloader_Plus_Portable.zip' -Force"
    echo Portable package created at dist\4K_Video_Downloader_Plus_Portable.zip
)

pause
