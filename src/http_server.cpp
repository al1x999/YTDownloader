#include "http_server.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <algorithm>
#include <regex>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <shellapi.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#define SOCKET int
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define closesocket close
#endif

namespace {
    std::string getContentType(const std::string& path) {
        if (path.rfind(".html") != std::string::npos) return "text/html; charset=utf-8";
        if (path.rfind(".css") != std::string::npos) return "text/css; charset=utf-8";
        if (path.rfind(".js") != std::string::npos) return "application/javascript; charset=utf-8";
        if (path.rfind(".json") != std::string::npos) return "application/json; charset=utf-8";
        if (path.rfind(".png") != std::string::npos) return "image/png";
        if (path.rfind(".jpg") != std::string::npos || path.rfind(".jpeg") != std::string::npos) return "image/jpeg";
        if (path.rfind(".svg") != std::string::npos) return "image/svg+xml";
        return "text/plain";
    }

    std::string getJsonValue(const std::string& json, const std::string& key) {
        std::regex re("\"" + key + "\":\\s*\"([^\"]*)\"");
        std::smatch m;
        if (std::regex_search(json, m, re)) {
            return m[1].str();
        }
        std::regex reBool("\"" + key + "\":\\s*(true|false)");
        if (std::regex_search(json, m, reBool)) {
            return m[1].str();
        }
        std::regex reNum("\"" + key + "\":\\s*([0-9]+)");
        if (std::regex_search(json, m, reNum)) {
            return m[1].str();
        }
        return "";
    }
}

HttpServer::HttpServer(int port, DownloadManager& dm)
    : port_(port), download_manager_(dm) {
#ifdef _WIN32
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
}

HttpServer::~HttpServer() {
    stop();
#ifdef _WIN32
    WSACleanup();
#endif
}

bool HttpServer::start() {
    listen_socket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listen_socket_ == INVALID_SOCKET) {
        std::cerr << "[HttpServer] Failed to create socket." << std::endl;
        return false;
    }

    int opt = 1;
    setsockopt(listen_socket_, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

    sockaddr_in service;
    service.sin_family = AF_INET;
    service.sin_addr.s_addr = inet_addr("127.0.0.1");
    service.sin_port = htons(port_);

    if (bind(listen_socket_, (sockaddr*)&service, sizeof(service)) == SOCKET_ERROR) {
        std::cerr << "[HttpServer] Bind failed on port " << port_ << std::endl;
        closesocket(listen_socket_);
        return false;
    }

    if (listen(listen_socket_, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "[HttpServer] Listen failed." << std::endl;
        closesocket(listen_socket_);
        return false;
    }

    running_ = true;
    server_thread_ = std::thread(&HttpServer::listenLoop, this);
    std::cout << "[HttpServer] Server active at http://127.0.0.1:" << port_ << std::endl;
    return true;
}

void HttpServer::stop() {
    if (running_) {
        running_ = false;
        if (listen_socket_ != INVALID_SOCKET) {
            closesocket(listen_socket_);
            listen_socket_ = INVALID_SOCKET;
        }
        if (server_thread_.joinable()) {
            server_thread_.join();
        }
    }
}

void HttpServer::listenLoop() {
    while (running_) {
        sockaddr_in clientAddr;
        int clientAddrSize = sizeof(clientAddr);
        SOCKET clientSocket = accept(listen_socket_, (sockaddr*)&clientAddr, &clientAddrSize);
        if (clientSocket != INVALID_SOCKET) {
            std::thread(&HttpServer::handleClient, this, static_cast<uintptr_t>(clientSocket)).detach();
        } else {
            if (!running_) break;
        }
    }
}

void HttpServer::handleClient(uintptr_t clientSocketHandle) {
    SOCKET clientSocket = static_cast<SOCKET>(clientSocketHandle);
    char buffer[4096];
    int bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);

    if (bytesReceived > 0) {
        buffer[bytesReceived] = '\0';
        std::string request(buffer);

        std::istringstream stream(request);
        std::string method, path, protocol;
        stream >> method >> path >> protocol;

        std::size_t bodyPos = request.find("\r\n\r\n");
        std::string body = (bodyPos != std::string::npos) ? request.substr(bodyPos + 4) : "";

        std::string httpResponse;

        if (path.find("/api/") == 0) {
            std::string jsonBody = handleApiRequest(method, path, body);
            std::stringstream res;
            res << "HTTP/1.1 200 OK\r\n"
                << "Content-Type: application/json; charset=utf-8\r\n"
                << "Access-Control-Allow-Origin: *\r\n"
                << "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
                << "Access-Control-Allow-Headers: Content-Type\r\n"
                << "Content-Length: " << jsonBody.length() << "\r\n"
                << "Connection: close\r\n\r\n"
                << jsonBody;
            httpResponse = res.str();
        } else if (method == "OPTIONS") {
            httpResponse = "HTTP/1.1 200 OK\r\n"
                           "Access-Control-Allow-Origin: *\r\n"
                           "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
                           "Access-Control-Allow-Headers: Content-Type\r\n"
                           "Content-Length: 0\r\n"
                           "Connection: close\r\n\r\n";
        } else {
            httpResponse = serveStaticFile(path);
        }

        send(clientSocket, httpResponse.c_str(), static_cast<int>(httpResponse.length()), 0);
    }

    closesocket(clientSocket);
}

std::string HttpServer::serveStaticFile(const std::string& reqPath) {
    std::string relPath = reqPath;
    if (relPath == "/" || relPath.empty()) {
        relPath = "/index.html";
    }

    std::string fullPath = "ui" + relPath;
    std::ifstream file(fullPath, std::ios::binary);

    if (!file.is_open()) {
        std::string notFound = "<h1>404 Not Found</h1>";
        std::stringstream ss;
        ss << "HTTP/1.1 404 Not Found\r\n"
           << "Content-Type: text/html\r\n"
           << "Content-Length: " << notFound.length() << "\r\n"
           << "Connection: close\r\n\r\n"
           << notFound;
        return ss.str();
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();

    std::stringstream res;
    res << "HTTP/1.1 200 OK\r\n"
        << "Content-Type: " << getContentType(relPath) << "\r\n"
        << "Content-Length: " << content.length() << "\r\n"
        << "Connection: close\r\n\r\n"
        << content;

    return res.str();
}

std::string HttpServer::handleApiRequest(const std::string& method, const std::string& path, const std::string& body) {
    if (path == "/api/parse" && method == "POST") {
        std::string url = getJsonValue(body, "url");
        MediaUrlMetaData meta = YouTubeParser::parseUrl(url);

        std::stringstream ss;
        ss << "{"
           << "\"success\":" << (meta.success ? "true" : "false") << ","
           << "\"type\":\"" << meta.type << "\","
           << "\"title\":\"" << YouTubeParser::escapeJsonString(meta.title) << "\","
           << "\"channel_name\":\"" << YouTubeParser::escapeJsonString(meta.channel_name) << "\","
           << "\"thumbnail_url\":\"" << YouTubeParser::escapeJsonString(meta.thumbnail_url) << "\","
           << "\"total_items\":" << meta.total_items << ","
           << "\"error_message\":\"" << YouTubeParser::escapeJsonString(meta.error_message) << "\","
           << "\"items\":[";

        for (size_t i = 0; i < meta.items.size(); ++i) {
            const auto& item = meta.items[i];
            ss << "{"
               << "\"url\":\"" << YouTubeParser::escapeJsonString(item.url) << "\","
               << "\"title\":\"" << YouTubeParser::escapeJsonString(item.title) << "\","
               << "\"uploader\":\"" << YouTubeParser::escapeJsonString(item.uploader) << "\","
               << "\"thumbnail\":\"" << YouTubeParser::escapeJsonString(item.thumbnail) << "\""
               << "}" << (i + 1 < meta.items.size() ? "," : "");
        }
        ss << "]}";
        return ss.str();

    } else if (path == "/api/download" && method == "POST") {
        std::string url = getJsonValue(body, "url");
        std::string format = getJsonValue(body, "format");
        std::string fps = getJsonValue(body, "fps");
        std::string type = getJsonValue(body, "type");
        std::string title = getJsonValue(body, "title");
        std::string thumbnail = getJsonValue(body, "thumbnail");
        std::string audio_format = getJsonValue(body, "audio_format");
        std::string audio_quality = getJsonValue(body, "audio_quality");
        std::string video_format = getJsonValue(body, "video_format");
        std::string sub_lang = getJsonValue(body, "sub_lang");
        std::string subfolder = getJsonValue(body, "subfolder");
        bool subs = getJsonValue(body, "subtitles") == "true";

        DownloadOptions opt;
        opt.url = url;
        opt.format = format.empty() ? "1080p" : format;
        opt.fps = fps.empty() ? "auto" : fps;
        opt.type = type.empty() ? "video" : type;
        opt.audio_format = audio_format.empty() ? "mp3" : audio_format;
        opt.audio_quality = audio_quality.empty() ? "320" : audio_quality;
        opt.video_format = video_format.empty() ? "mp4" : video_format;
        opt.download_subtitles = subs;
        opt.subtitle_lang = sub_lang.empty() ? "en" : sub_lang;
        opt.subfolder = subfolder;

        std::string taskId = download_manager_.enqueueTask(opt, title, thumbnail);

        return "{\"success\":true, \"task_id\":\"" + taskId + "\"}";

    } else if (path == "/api/tasks" && method == "GET") {
        auto tasks = download_manager_.getAllTasks();
        std::stringstream ss;
        ss << "[";
        for (size_t i = 0; i < tasks.size(); ++i) {
            const auto& t = tasks[i];
            ss << "{"
               << "\"id\":\"" << t.id << "\","
               << "\"title\":\"" << YouTubeParser::escapeJsonString(t.title) << "\","
               << "\"url\":\"" << YouTubeParser::escapeJsonString(t.url) << "\","
               << "\"thumbnail\":\"" << YouTubeParser::escapeJsonString(t.thumbnail) << "\","
               << "\"status\":\"" << t.status << "\","
               << "\"progress\":" << t.progress << ","
               << "\"speed\":\"" << t.speed << "\","
               << "\"eta\":\"" << t.eta << "\","
               << "\"filepath\":\"" << YouTubeParser::escapeJsonString(t.filepath) << "\","
               << "\"error\":\"" << YouTubeParser::escapeJsonString(t.error_message) << "\""
               << "}" << (i + 1 < tasks.size() ? "," : "");
        }
        ss << "]";
        return ss.str();

    } else if (path == "/api/cancel" && method == "POST") {
        std::string id = getJsonValue(body, "id");
        bool res = download_manager_.cancelTask(id);
        return "{\"success\":" + std::string(res ? "true" : "false") + "}";

    } else if (path == "/api/pause" && method == "POST") {
        std::string id = getJsonValue(body, "id");
        bool res = download_manager_.pauseTask(id);
        return "{\"success\":" + std::string(res ? "true" : "false") + "}";

    } else if (path == "/api/resume" && method == "POST") {
        std::string id = getJsonValue(body, "id");
        bool res = download_manager_.resumeTask(id);
        return "{\"success\":" + std::string(res ? "true" : "false") + "}";

    } else if (path == "/api/remove" && method == "POST") {
        std::string id = getJsonValue(body, "id");
        bool res = download_manager_.removeTask(id);
        return "{\"success\":" + std::string(res ? "true" : "false") + "}";

    } else if (path == "/api/settings" && method == "GET") {
        std::stringstream ss;
        ss << "{"
           << "\"output_dir\":\"" << YouTubeParser::escapeJsonString(download_manager_.getDefaultOutputDir()) << "\","
           << "\"max_concurrent\":" << download_manager_.getMaxConcurrentDownloads() << ","
           << "\"speed_limit\":\"" << download_manager_.getGlobalSpeedLimit() << "\""
           << "}";
        return ss.str();

    } else if (path == "/api/settings" && method == "POST") {
        std::string max_c = getJsonValue(body, "max_concurrent");
        std::string speed = getJsonValue(body, "speed_limit");
        std::string dir = getJsonValue(body, "output_dir");

        if (!max_c.empty()) download_manager_.setMaxConcurrentDownloads(std::stoi(max_c));
        if (!speed.empty()) download_manager_.setGlobalSpeedLimit(speed);
        if (!dir.empty()) download_manager_.setDefaultOutputDir(dir);

        return "{\"success\":true}";

    } else if (path == "/api/clear-completed" && method == "POST") {
        download_manager_.clearCompletedTasks();
        return "{\"success\":true}";

    } else if (path == "/api/pause-all" && method == "POST") {
        download_manager_.pauseAllTasks();
        return "{\"success\":true}";

    } else if (path == "/api/resume-all" && method == "POST") {
        download_manager_.resumeAllTasks();
        return "{\"success\":true}";

    } else if (path == "/api/history" && method == "GET") {
        auto history = download_manager_.getHistory();
        std::stringstream ss;
        ss << "[";
        for (size_t i = 0; i < history.size(); ++i) {
            const auto& h = history[i];
            ss << "{"
               << "\"id\":\"" << h.id << "\","
               << "\"title\":\"" << YouTubeParser::escapeJsonString(h.title) << "\","
               << "\"url\":\"" << YouTubeParser::escapeJsonString(h.url) << "\","
               << "\"thumbnail\":\"" << YouTubeParser::escapeJsonString(h.thumbnail) << "\","
               << "\"type\":\"" << YouTubeParser::escapeJsonString(h.type) << "\","
               << "\"format\":\"" << YouTubeParser::escapeJsonString(h.format) << "\","
               << "\"filepath\":\"" << YouTubeParser::escapeJsonString(h.filepath) << "\","
               << "\"status\":\"" << YouTubeParser::escapeJsonString(h.status) << "\","
               << "\"timestamp\":\"" << YouTubeParser::escapeJsonString(h.timestamp) << "\","
               << "\"filesize\":" << h.filesize
               << "}" << (i + 1 < history.size() ? "," : "");
        }
        ss << "]";
        return ss.str();

    } else if (path == "/api/history/clear" && method == "POST") {
        download_manager_.clearHistory();
        return "{\"success\":true}";

    } else if (path == "/api/history/remove" && method == "POST") {
        std::string id = getJsonValue(body, "id");
        bool res = download_manager_.removeHistoryItem(id);
        return "{\"success\":" + std::string(res ? "true" : "false") + "}";

    } else if (path == "/api/open-file" && method == "POST") {
        std::string filepath = getJsonValue(body, "path");
        if (!filepath.empty()) {
#ifdef _WIN32
            ShellExecuteA(NULL, "open", filepath.c_str(), NULL, NULL, SW_SHOWDEFAULT);
#else
            std::string cmd = "xdg-open \"" + filepath + "\"";
            system(cmd.c_str());
#endif
        }
        return "{\"success\":true}";

    } else if (path == "/api/open-folder" && method == "POST") {
        std::string dir = getJsonValue(body, "path");
        if (dir.empty()) {
            dir = download_manager_.getDefaultOutputDir();
        }
#ifdef _WIN32
        DWORD attribs = GetFileAttributesA(dir.c_str());
        if (attribs != INVALID_FILE_ATTRIBUTES && !(attribs & FILE_ATTRIBUTE_DIRECTORY)) {
            std::string cmd = "explorer.exe /select,\"" + dir + "\"";
            system(cmd.c_str());
        } else {
            ShellExecuteA(NULL, "open", dir.c_str(), NULL, NULL, SW_SHOWDEFAULT);
        }
#else
        std::string cmd = "xdg-open \"" + dir + "\"";
        system(cmd.c_str());
#endif
        return "{\"success\":true}";
    }

    return "{\"error\":\"Unknown API endpoint\"}";
}
