#ifndef DOWNLOAD_MANAGER_H
#define DOWNLOAD_MANAGER_H

#include <string>
#include <vector>
#include <mutex>
#include <thread>
#include <atomic>
#include <memory>

struct DownloadOptions {
    std::string url;
    std::string format = "1080p"; // "best", "8k", "4k", "2k", "1080p", "720p", "480p", "360p"
    std::string fps = "auto";     // "60", "30", "auto"
    std::string type = "video";   // "video" or "audio"
    std::string video_format = "mp4"; // "mp4", "mkv", "webm", "3gp", "avi"
    std::string audio_format = "mp3"; // "mp3", "m4a", "flac", "wav", "ogg", "aac", "opus"
    std::string audio_quality = "320"; // "320", "256", "128"
    bool download_subtitles = false;
    std::string subtitle_lang = "en";
    std::string output_dir = "";  // Base folder
    std::string subfolder = "";   // Channel or Playlist subfolder name
    std::string speed_limit = ""; // e.g. "5M", "1M" or "" for unlimited
};

struct DownloadTask {
    std::string id;
    std::string title;
    std::string url;
    std::string thumbnail;
    std::string status;          // "queued", "downloading", "completed", "error", "paused", "cancelled"
    float progress = 0.0f;
    std::string speed = "0 KB/s";
    std::string eta = "--:--";
    long long downloaded_bytes = 0;
    long long total_bytes = 0;
    std::string filepath;
    std::string error_message;
    DownloadOptions options;
    bool cancel_requested = false;
};

class DownloadManager {
public:
    DownloadManager();
    ~DownloadManager();

    std::string enqueueTask(const DownloadOptions& options, const std::string& title = "", const std::string& thumbnail = "");
    bool cancelTask(const std::string& taskId);
    bool pauseTask(const std::string& taskId);
    bool resumeTask(const std::string& taskId);
    bool removeTask(const std::string& taskId);
    void clearCompletedTasks();

    std::vector<DownloadTask> getAllTasks();
    DownloadTask getTask(const std::string& taskId);

    void setMaxConcurrentDownloads(int maxDownloads);
    int getMaxConcurrentDownloads() const;

    void setGlobalSpeedLimit(const std::string& speedLimit);
    std::string getGlobalSpeedLimit() const;

    void setDefaultOutputDir(const std::string& path);
    std::string getDefaultOutputDir() const;

private:
    void workerThread();
    void runTaskProcess(DownloadTask& task);
    std::string buildYtDlpCommand(const DownloadTask& task);

    std::vector<DownloadTask> tasks_;
    mutable std::mutex tasks_mutex_;
    std::thread worker_;
    std::atomic<bool> stop_worker_{false};

    int max_concurrent_downloads_ = 3;
    std::string global_speed_limit_ = "";
    std::string default_output_dir_;
};

#endif // DOWNLOAD_MANAGER_H
