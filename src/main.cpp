#include <iostream>
#include <fstream>
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
    g_running = false;
}

void launchDesktopAppWindow(const std::string& appUrl) {
#ifdef _WIN32
    std::string edgePath = "C:\\Program Files (x86)\\Microsoft\\Edge\\Application\\msedge.exe";
    std::ifstream edgeFile(edgePath);
    if (!edgeFile.good()) {
        edgePath = "C:\\Program Files\\Microsoft\\Edge\\Application\\msedge.exe";
    }

    char localAppData[MAX_PATH];
    std::string userDataDir = "";
    if (GetEnvironmentVariableA("LOCALAPPDATA", localAppData, MAX_PATH) > 0) {
        userDataDir = std::string(localAppData) + "\\YTDownloaderAppData";
    } else {
        userDataDir = "C:\\Temp\\YTDownloaderAppData";
    }

    std::string cmdArgs = " --app=" + appUrl + " --user-data-dir=\"" + userDataDir + "\" --window-size=1120,740";
    
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    std::string fullCmd = "\"" + edgePath + "\"" + cmdArgs;
    
    BOOL success = CreateProcessA(
        NULL,
        const_cast<char*>(fullCmd.c_str()),
        NULL, NULL, FALSE,
        CREATE_NO_WINDOW,
        NULL, NULL,
        &si, &pi
    );

    if (success) {
        WaitForSingleObject(pi.hProcess, INFINITE);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        g_running = false;
    } else {
        ShellExecuteA(NULL, "open", "msedge.exe", ("--app=" + appUrl).c_str(), NULL, SW_SHOWNORMAL);
    }
#else
    std::string openCmd = "xdg-open " + appUrl;
    system(openCmd.c_str());
#endif
}

int main(int argc, char* argv[]) {
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    DownloadManager downloadManager;
    int port = 8080;

    if (argc > 1) {
        port = std::atoi(argv[1]);
        if (port <= 0) port = 8080;
    }

    HttpServer server(port, downloadManager);
    if (!server.start()) {
        return 1;
    }

    std::string appUrl = "http://127.0.0.1:" + std::to_string(port);
    launchDesktopAppWindow(appUrl);

    while (g_running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    server.stop();
    return 0;
}

#ifdef _WIN32
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    return main(__argc, __argv);
}
#endif
