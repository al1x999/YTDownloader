#include <iostream>
#include <chrono>
#include <thread>
#include <csignal>
#include "download_manager.h"
#include "http_server.h"

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif

std::atomic<bool> g_running{true};

void signalHandler(int signum) {
    std::cout << "\n[YTDownloader] Interrupt signal (" << signum << ") received. Exiting...\n";
    g_running = false;
}

int main(int argc, char* argv[]) {
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    std::cout << "========================================================\n";
    std::cout << "     4K Video Downloader Plus - C++ Core Engine         \n";
    std::cout << "========================================================\n";

    DownloadManager downloadManager;
    int port = 8080;

    if (argc > 1) {
        port = std::atoi(argv[1]);
        if (port <= 0) port = 8080;
    }

    HttpServer server(port, downloadManager);
    if (!server.start()) {
        std::cerr << "[YTDownloader] Failed to launch HTTP Server on port " << port << std::endl;
        return 1;
    }

    std::string appUrl = "http://127.0.0.1:" + std::to_string(port);
    std::cout << "[YTDownloader] Opening Desktop UI at " << appUrl << " ...\n";

#ifdef _WIN32
    ShellExecuteA(NULL, "open", appUrl.c_str(), NULL, NULL, SW_SHOWNORMAL);
#else
    std::string openCmd = "xdg-open " + appUrl;
    system(openCmd.c_str());
#endif

    std::cout << "[YTDownloader] Engine active. Press Ctrl+C in terminal to stop.\n";

    while (g_running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    server.stop();
    std::cout << "[YTDownloader] Shutdown clean.\n";
    return 0;
}
