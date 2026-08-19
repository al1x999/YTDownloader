#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include "download_manager.h"
#include "youtube_parser.h"
#include <string>
#include <thread>
#include <atomic>

class HttpServer {
public:
    HttpServer(int port, DownloadManager& dm);
    ~HttpServer();

    bool start();
    void stop();

    int getPort() const { return port_; }

private:
    void listenLoop();
    void handleClient(uintptr_t clientSocket);
    
    std::string handleApiRequest(const std::string& method, const std::string& path, const std::string& body);
    std::string serveStaticFile(const std::string& filePath);

    int port_;
    DownloadManager& download_manager_;
    std::thread server_thread_;
    std::atomic<bool> running_{false};
    uintptr_t listen_socket_ = ~0;
};

#endif // HTTP_SERVER_H
