# 4K Video Downloader Plus - C++ Desktop App Clone

A full-featured clone of **4K Video Downloader Plus** built with a C++ Engine (`CMake` + `g++`) supporting YouTube video, playlist, and channel downloads across all resolutions (8K, 4K, 2K, 1080p, 720p, MP3) with subtitle extraction, speed limiting, and desktop installer support.

![4K Downloader Banner](ui/assets/preview.png)

## Features

- **Full YouTube Channel & Playlist Downloading**: Download complete channels (`@handle`, `/channel/UC...`) and playlists with batch video parsing.
- **Multi-Format Export**:
  - **Video**: 8K Ultra HD (4320p), 4K (2160p), 2K (1440p), 1080p 60fps, 720p, 480p, 360p (MP4, MKV, WEBM).
  - **Audio Extraction**: MP3, M4A, FLAC, WAV, OGG, AAC (320kbps, 256kbps, 128kbps).
- **Subtitles**: Auto-generated and official `.srt` subtitle downloads in multiple languages.
- **Smart Mode**: Quick one-click downloads using saved preset resolution, audio, folder, and subtitle settings.
- **Speed Limiter & Queue Controls**: Control bandwidth (5MB/s, 2MB/s, 1MB/s, unlimited), pause, resume, cancel, and clear downloads.
- **4K Downloader Plus Theme**: Premium dark theme UI with neon green accents, glassmorphic modals, and progress indicators.
- **Desktop Executable & Installer**: Native C++ core engine compiled with `CMake` and MinGW `g++`.

---

## Tech Stack & Architecture

```
YTDownloader/
├── CMakeLists.txt              # CMake C++20 build configuration
├── src/
│   ├── main.cpp                # C++ application entry point
│   ├── download_manager.h/.cpp # C++ multi-threaded download manager & process runner
│   ├── youtube_parser.h/.cpp   # C++ YouTube metadata & playlist/channel parser
│   └── http_server.h/.cpp      # C++ WinSock HTTP REST server & UI bridge
├── ui/                         # 4K Video Downloader Plus Clone UI
│   ├── index.html              # Main App Window UI
│   ├── style.css               # Premium Dark Theme CSS
│   └── app.js                  # Frontend REST API connector & queue renderer
└── scripts/
    ├── build.bat               # Automated C++ compilation script
    ├── package_installer.bat   # Desktop Setup package builder
    └── installer_script.iss    # Inno Setup desktop installer configuration
```

---

## Quick Start (Running the App)

### 1. Build the C++ Engine
Double-click `scripts/build.bat` or run:
```cmd
cmake -B build -G "MinGW Makefiles"
cmake --build build
```

### 2. Run the App
Run the generated executable:
```cmd
.\build\YTDownloaderCore.exe
```
This automatically starts the C++ core server and opens the 4K Video Downloader Plus desktop interface in your browser/window at `http://127.0.0.1:8080`.

---

## Building Installable Desktop Package (.exe)

To generate a Windows desktop installer:
Run `scripts/package_installer.bat`.
- If Inno Setup is installed, it creates `dist/4K_Video_Downloader_Plus_Setup.exe`.
- Otherwise, it creates a portable desktop zip package at `dist/4K_Video_Downloader_Plus_Portable.zip`.
