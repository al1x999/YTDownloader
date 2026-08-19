#include "download_manager.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <chrono>
#include <ctime>
#include <regex>
#include <cstdlib>

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#else
#include <unistd.h>
#endif

namespace {
    std::string generateUniqueId() {
        static std::atomic<unsigned long long> counter{1000};
        auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        return std::to_string(now) + "_" + std::to_string(++counter);
    }

    std::string getSystemDownloadsFolder() {
#ifdef _WIN32
        PWSTR path = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Downloads, 0, NULL, &path))) {
            char buffer[MAX_PATH];
            wcstombs(buffer, path, MAX_PATH);
            CoTaskMemFree(path);
            return std::string(buffer);
        }
        return "C:\\Downloads";
#else
        const char* home = getenv("HOME");
        if (home) return std::string(home) + "/Downloads";
        return "./Downloads";
#endif
    }

    std::string escapeJsonStr(const std::string& input) {
        std::string out;
        out.reserve(input.size());
        for (char c : input) {
            if (c == '"') out += "\\\"";
            else if (c == '\\') out += "\\\\";
            else if (c == '\n') out += "\\n";
            else if (c == '\r') out += "\\r";
            else if (c == '\t') out += "\\t";
            else out += c;
        }
        return out;
    }
}

DownloadManager::DownloadManager() {
    default_output_dir_ = getSystemDownloadsFolder();
    loadHistoryFromFile();
    worker_ = std::thread(&DownloadManager::workerThread, this);
}

DownloadManager::~DownloadManager() {
    stop_worker_ = true;
    if (worker_.joinable()) {
        worker_.join();
    }
}

std::string DownloadManager::enqueueTask(const DownloadOptions& options, const std::string& title, const std::string& thumbnail) {
    std::lock_guard<std::mutex> lock(tasks_mutex_);
    DownloadTask task;
    task.id = generateUniqueId();
    task.url = options.url;
    task.title = title.empty() ? options.url : title;
    task.thumbnail = thumbnail;
    task.status = "queued";
    task.options = options;
    if (task.options.output_dir.empty()) {
        task.options.output_dir = default_output_dir_;
    }
    tasks_.push_back(task);
    return task.id;
}

bool DownloadManager::cancelTask(const std::string& taskId) {
    std::lock_guard<std::mutex> lock(tasks_mutex_);
    for (auto& task : tasks_) {
        if (task.id == taskId) {
            task.cancel_requested = true;
            if (task.status == "queued" || task.status == "paused") {
                task.status = "cancelled";
            }
            return true;
        }
    }
    return false;
}

bool DownloadManager::pauseTask(const std::string& taskId) {
    std::lock_guard<std::mutex> lock(tasks_mutex_);
    for (auto& task : tasks_) {
        if (task.id == taskId) {
            if (task.status == "downloading" || task.status == "queued") {
                task.status = "paused";
                task.cancel_requested = true;
                return true;
            }
        }
    }
    return false;
}

bool DownloadManager::resumeTask(const std::string& taskId) {
    std::lock_guard<std::mutex> lock(tasks_mutex_);
    for (auto& task : tasks_) {
        if (task.id == taskId) {
            if (task.status == "paused" || task.status == "error") {
                task.status = "queued";
                task.cancel_requested = false;
                return true;
            }
        }
    }
    return false;
}

void DownloadManager::pauseAllTasks() {
    std::lock_guard<std::mutex> lock(tasks_mutex_);
    for (auto& task : tasks_) {
        if (task.status == "downloading" || task.status == "queued") {
            task.status = "paused";
            task.cancel_requested = true;
        }
    }
}

void DownloadManager::resumeAllTasks() {
    std::lock_guard<std::mutex> lock(tasks_mutex_);
    for (auto& task : tasks_) {
        if (task.status == "paused" || task.status == "error") {
            task.status = "queued";
            task.cancel_requested = false;
        }
    }
}

bool DownloadManager::removeTask(const std::string& taskId) {
    std::lock_guard<std::mutex> lock(tasks_mutex_);
    auto it = std::remove_if(tasks_.begin(), tasks_.end(), [&](const DownloadTask& t) {
        return t.id == taskId;
    });
    if (it != tasks_.end()) {
        tasks_.erase(it, tasks_.end());
        return true;
    }
    return false;
}

void DownloadManager::clearCompletedTasks() {
    std::lock_guard<std::mutex> lock(tasks_mutex_);
    auto it = std::remove_if(tasks_.begin(), tasks_.end(), [](const DownloadTask& t) {
        return t.status == "completed" || t.status == "cancelled" || t.status == "error";
    });
    tasks_.erase(it, tasks_.end());
}

std::vector<DownloadTask> DownloadManager::getAllTasks() {
    std::lock_guard<std::mutex> lock(tasks_mutex_);
    return tasks_;
}

DownloadTask DownloadManager::getTask(const std::string& taskId) {
    std::lock_guard<std::mutex> lock(tasks_mutex_);
    for (const auto& task : tasks_) {
        if (task.id == taskId) return task;
    }
    return DownloadTask{};
}

void DownloadManager::setMaxConcurrentDownloads(int maxDownloads) {
    max_concurrent_downloads_ = std::max(1, maxDownloads);
}

int DownloadManager::getMaxConcurrentDownloads() const {
    return max_concurrent_downloads_;
}

void DownloadManager::setGlobalSpeedLimit(const std::string& speedLimit) {
    global_speed_limit_ = speedLimit;
}

std::string DownloadManager::getGlobalSpeedLimit() const {
    return global_speed_limit_;
}

void DownloadManager::setDefaultOutputDir(const std::string& path) {
    if (!path.empty()) {
        default_output_dir_ = path;
    }
}

std::string DownloadManager::getDefaultOutputDir() const {
    return default_output_dir_;
}

void DownloadManager::workerThread() {
    while (!stop_worker_) {
        std::string taskIdToRun = "";
        {
            std::lock_guard<std::mutex> lock(tasks_mutex_);
            int activeCount = 0;
            for (const auto& t : tasks_) {
                if (t.status == "downloading") activeCount++;
            }
            if (activeCount < max_concurrent_downloads_) {
                for (auto& t : tasks_) {
                    if (t.status == "queued") {
                        t.status = "downloading";
                        taskIdToRun = t.id;
                        break;
                    }
                }
            }
        }

        if (!taskIdToRun.empty()) {
            DownloadTask taskToRun;
            {
                std::lock_guard<std::mutex> lock(tasks_mutex_);
                for (const auto& t : tasks_) {
                    if (t.id == taskIdToRun) {
                        taskToRun = t;
                        break;
                    }
                }
            }
            
            runTaskProcess(taskToRun);
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
    }
}

std::string DownloadManager::buildYtDlpCommand(const DownloadTask& task) {
    std::stringstream cmd;
    cmd << "yt-dlp --continue --newline --no-colors --no-check-certificates --js-runtimes node --extractor-args \"youtube:player_client=web_embedded,android\" ";

    const auto& opt = task.options;

    // Output template with channel/playlist subfolder support
    if (!opt.subfolder.empty()) {
        cmd << "-o \"" << opt.output_dir << "/" << opt.subfolder << "/%(title)s.%(ext)s\" ";
    } else {
        cmd << "-o \"" << opt.output_dir << "/%(title)s.%(ext)s\" ";
    }

    // Speed limits
    std::string speed = opt.speed_limit.empty() ? global_speed_limit_ : opt.speed_limit;
    if (!speed.empty() && speed != "unlimited") {
        cmd << "--limit-rate " << speed << " ";
    }

    // Subtitles
    if (opt.download_subtitles) {
        cmd << "--write-sub --write-auto-sub --sub-lang \"" << opt.subtitle_lang << "\" --embed-subs ";
    }

    // Audio vs Video
    if (opt.type == "audio") {
        cmd << "-x --audio-format " << opt.audio_format << " ";
        if (!opt.audio_quality.empty()) {
            cmd << "--audio-quality " << opt.audio_quality << "k ";
        }
    } else {
        // FPS filter string
        std::string fpsFilter = "";
        if (opt.fps == "60") {
            fpsFilter = "[fps>=50]";
        } else if (opt.fps == "30") {
            fpsFilter = "[fps<=30]";
        }

        // Video format selection with height and fps filters
        if (opt.format == "8k" || opt.format == "4320p") {
            cmd << "-f \"bestvideo[height<=4320]" << fpsFilter << "+bestaudio/bestvideo[height<=4320]+bestaudio/best\" ";
        } else if (opt.format == "4k" || opt.format == "2160p") {
            cmd << "-f \"bestvideo[height<=2160]" << fpsFilter << "+bestaudio/bestvideo[height<=2160]+bestaudio/best\" ";
        } else if (opt.format == "2k" || opt.format == "1440p") {
            cmd << "-f \"bestvideo[height<=1440]" << fpsFilter << "+bestaudio/bestvideo[height<=1440]+bestaudio/best\" ";
        } else if (opt.format == "1080p") {
            cmd << "-f \"bestvideo[height<=1080]" << fpsFilter << "+bestaudio/bestvideo[height<=1080]+bestaudio/best\" ";
        } else if (opt.format == "720p") {
            cmd << "-f \"bestvideo[height<=720]" << fpsFilter << "+bestaudio/bestvideo[height<=720]+bestaudio/best\" ";
        } else if (opt.format == "480p") {
            cmd << "-f \"bestvideo[height<=480]" << fpsFilter << "+bestaudio/bestvideo[height<=480]+bestaudio/best\" ";
        } else if (opt.format == "360p") {
            cmd << "-f \"bestvideo[height<=360]" << fpsFilter << "+bestaudio/bestvideo[height<=360]+bestaudio/best\" ";
        } else {
            cmd << "-f \"bestvideo+bestaudio/best\" ";
        }

        if (!opt.video_format.empty()) {
            cmd << "--merge-output-format " << opt.video_format << " ";
        }
    }

    cmd << "\"" << task.url << "\" 2>&1";

    return cmd.str();
}

void DownloadManager::runTaskProcess(DownloadTask& task) {
    std::string command = buildYtDlpCommand(task);
    std::cout << "[YTDownloader] Executing: " << command << std::endl;

#ifdef _WIN32
    FILE* pipe = _popen(command.c_str(), "r");
#else
    FILE* pipe = popen(command.c_str(), "r");
#endif

    if (!pipe) {
        std::lock_guard<std::mutex> lock(tasks_mutex_);
        for (auto& t : tasks_) {
            if (t.id == task.id) {
                t.status = "error";
                t.error_message = "Failed to start yt-dlp process.";
            }
        }
        return;
    }

    char buffer[1024];
    std::regex progressRegex(R"raw(\[download\]\s+(\d+(?:\.\d+)?)%\s+of\s+([~\d\.\w]+)\s+at\s+([\d\.\w/]+)\s+ETA\s+([\d:]+))raw");
    std::regex destRegex(R"raw(\[download\]\s+Destination:\s+(.+))raw");
    std::regex mergeRegex(R"raw(\[ffmpeg\]\s+Merging formats into "(.+)")raw");
    std::regex titleRegex(R"raw(\[youtube\]\s+Extracting URL:|\[download\]\s+Downloading item\s+\d+\s+of\s+\d+)raw");

    while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
        std::string line(buffer);

        // Check if task cancel/pause requested
        bool cancelReq = false;
        {
            std::lock_guard<std::mutex> lock(tasks_mutex_);
            for (const auto& t : tasks_) {
                if (t.id == task.id) {
                    cancelReq = t.cancel_requested;
                    break;
                }
            }
        }

        if (cancelReq) {
            break;
        }

        std::smatch match;
        if (std::regex_search(line, match, progressRegex)) {
            float pct = std::stof(match[1].str());
            std::string sizeStr = match[2].str();
            std::string speedStr = match[3].str();
            std::string etaStr = match[4].str();

            std::lock_guard<std::mutex> lock(tasks_mutex_);
            for (auto& t : tasks_) {
                if (t.id == task.id) {
                    t.progress = pct;
                    t.speed = speedStr;
                    t.eta = etaStr;
                }
            }
        } else if (std::regex_search(line, match, destRegex)) {
            std::string filepath = match[1].str();
            std::lock_guard<std::mutex> lock(tasks_mutex_);
            for (auto& t : tasks_) {
                if (t.id == task.id) {
                    t.filepath = filepath;
                }
            }
        } else if (std::regex_search(line, match, mergeRegex)) {
            std::string filepath = match[1].str();
            std::lock_guard<std::mutex> lock(tasks_mutex_);
            for (auto& t : tasks_) {
                if (t.id == task.id) {
                    t.filepath = filepath;
                }
            }
        }
    }

#ifdef _WIN32
    int exitCode = _pclose(pipe);
#else
    int exitCode = pclose(pipe);
#endif

    DownloadTask finishedTask;
    bool shouldAddHistory = false;

    {
        std::lock_guard<std::mutex> lock(tasks_mutex_);
        for (auto& t : tasks_) {
            if (t.id == task.id) {
                if (t.cancel_requested) {
                    if (t.status != "paused") {
                        t.status = "cancelled";
                    }
                } else if (exitCode == 0) {
                    t.status = "completed";
                    t.progress = 100.0f;
                    t.speed = "Done";
                    t.eta = "00:00";
                } else {
                    t.status = "error";
                    t.error_message = "Download process exited with code " + std::to_string(exitCode);
                }

                if (t.status == "completed" || t.status == "cancelled" || t.status == "error") {
                    finishedTask = t;
                    shouldAddHistory = true;
                }
            }
        }
    }

    if (shouldAddHistory) {
        addToHistory(finishedTask);
    }
}

std::vector<DownloadHistoryItem> DownloadManager::getHistory() {
    std::lock_guard<std::mutex> lock(history_mutex_);
    return history_;
}

bool DownloadManager::clearHistory() {
    {
        std::lock_guard<std::mutex> lock(history_mutex_);
        history_.clear();
    }
    saveHistoryToFile();
    return true;
}

bool DownloadManager::removeHistoryItem(const std::string& historyId) {
    bool found = false;
    {
        std::lock_guard<std::mutex> lock(history_mutex_);
        auto it = std::remove_if(history_.begin(), history_.end(), [&](const DownloadHistoryItem& item) {
            return item.id == historyId;
        });
        if (it != history_.end()) {
            history_.erase(it, history_.end());
            found = true;
        }
    }
    if (found) {
        saveHistoryToFile();
    }
    return found;
}

void DownloadManager::loadHistoryFromFile() {
    std::ifstream file("download_history.json");
    if (!file.is_open()) return;

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string jsonStr = buffer.str();

    std::regex objectRegex(R"raw(\{([^{}]+)\})raw");
    std::sregex_iterator iter(jsonStr.begin(), jsonStr.end(), objectRegex);
    std::sregex_iterator end;

    std::lock_guard<std::mutex> lock(history_mutex_);
    history_.clear();

    auto getKey = [](const std::string& str, const std::string& key) -> std::string {
        std::regex re("\"" + key + "\"\\s*:\\s*\"([^\"]*)\"");
        std::smatch m;
        if (std::regex_search(str, m, re)) return m[1].str();
        std::regex reNum("\"" + key + "\"\\s*:\\s*([0-9]+)");
        if (std::regex_search(str, m, reNum)) return m[1].str();
        return "";
    };

    while (iter != end) {
        std::string objStr = iter->str();
        DownloadHistoryItem item;
        item.id = getKey(objStr, "id");
        item.title = getKey(objStr, "title");
        item.url = getKey(objStr, "url");
        item.thumbnail = getKey(objStr, "thumbnail");
        item.type = getKey(objStr, "type");
        item.format = getKey(objStr, "format");
        item.filepath = getKey(objStr, "filepath");
        item.status = getKey(objStr, "status");
        item.timestamp = getKey(objStr, "timestamp");
        std::string sizeStr = getKey(objStr, "filesize");
        if (!sizeStr.empty()) item.filesize = std::stoll(sizeStr);

        if (!item.id.empty()) {
            history_.push_back(item);
        }
        ++iter;
    }
}

void DownloadManager::saveHistoryToFile() {
    std::vector<DownloadHistoryItem> copy;
    {
        std::lock_guard<std::mutex> lock(history_mutex_);
        copy = history_;
    }

    std::ofstream file("download_history.json", std::ios::trunc);
    if (!file.is_open()) return;

    file << "[\n";
    for (size_t i = 0; i < copy.size(); ++i) {
        const auto& h = copy[i];
        file << "  {\n"
             << "    \"id\": \"" << escapeJsonStr(h.id) << "\",\n"
             << "    \"title\": \"" << escapeJsonStr(h.title) << "\",\n"
             << "    \"url\": \"" << escapeJsonStr(h.url) << "\",\n"
             << "    \"thumbnail\": \"" << escapeJsonStr(h.thumbnail) << "\",\n"
             << "    \"type\": \"" << escapeJsonStr(h.type) << "\",\n"
             << "    \"format\": \"" << escapeJsonStr(h.format) << "\",\n"
             << "    \"filepath\": \"" << escapeJsonStr(h.filepath) << "\",\n"
             << "    \"status\": \"" << escapeJsonStr(h.status) << "\",\n"
             << "    \"timestamp\": \"" << escapeJsonStr(h.timestamp) << "\",\n"
             << "    \"filesize\": " << h.filesize << "\n"
             << "  }" << (i + 1 < copy.size() ? "," : "") << "\n";
    }
    file << "]\n";
}

void DownloadManager::addToHistory(const DownloadTask& task) {
    if (task.status == "paused") return;

    DownloadHistoryItem item;
    item.id = generateUniqueId();
    item.title = task.title;
    item.url = task.url;
    item.thumbnail = task.thumbnail;
    item.type = task.options.type;
    item.format = (task.options.type == "audio") ? task.options.audio_format : task.options.format;
    item.filepath = task.filepath;
    item.status = task.status;
    item.options = task.options;

    auto now = std::chrono::system_clock::now();
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);
    char dateBuf[64];
    std::strftime(dateBuf, sizeof(dateBuf), "%Y-%m-%d %H:%M", std::localtime(&now_c));
    item.timestamp = dateBuf;

    if (!task.filepath.empty()) {
        std::ifstream f(task.filepath, std::ios::binary | std::ios::ate);
        if (f.is_open()) {
            item.filesize = f.tellg();
        }
    }

    {
        std::lock_guard<std::mutex> lock(history_mutex_);
        history_.insert(history_.begin(), item);
        if (history_.size() > 500) {
            history_.resize(500);
        }
    }
    saveHistoryToFile();
}
