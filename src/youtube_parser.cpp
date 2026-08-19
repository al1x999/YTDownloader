#include "youtube_parser.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cstdlib>

#ifdef _WIN32
#include <windows.h>
#endif

namespace {
    std::string extractJsonField(const std::string& json, const std::string& fieldName, size_t startPos = 0) {
        std::string pattern = "\"" + fieldName + "\": \"";
        size_t pos = json.find(pattern, startPos);
        if (pos == std::string::npos) {
            pattern = "\"" + fieldName + "\":\"";
            pos = json.find(pattern, startPos);
            if (pos == std::string::npos) return "";
        }
        size_t start = pos + pattern.length();
        size_t end = json.find("\"", start);
        if (end == std::string::npos) return "";
        return json.substr(start, end - start);
    }
}

std::string YouTubeParser::executeCommand(const std::string& command) {
    std::string result = "";
    char buffer[8192];
#ifdef _WIN32
    FILE* pipe = _popen(command.c_str(), "r");
#else
    FILE* pipe = popen(command.c_str(), "r");
#endif
    if (!pipe) return "";
    while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
        result += buffer;
    }
#ifdef _WIN32
    _pclose(pipe);
#else
    pclose(pipe);
#endif
    return result;
}

std::string YouTubeParser::escapeJsonString(const std::string& input) {
    std::stringstream ss;
    for (char c : input) {
        switch (c) {
            case '"':  ss << "\\\""; break;
            case '\\': ss << "\\\\"; break;
            case '\b': ss << "\\b";  break;
            case '\f': ss << "\\f";  break;
            case '\n': ss << "\\n";  break;
            case '\r': ss << "\\r";  break;
            case '\t': ss << "\\t";  break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    ss << "\\u" << std::hex << (int)c;
                } else {
                    ss << c;
                }
        }
    }
    return ss.str();
}

std::string YouTubeParser::detectUrlType(const std::string& url) {
    if (url.find("playlist?list=") != std::string::npos) {
        return "playlist";
    }
    if (url.find("/@") != std::string::npos || 
        url.find("/channel/") != std::string::npos || 
        url.find("/c/") != std::string::npos || 
        url.find("/user/") != std::string::npos ||
        url.find("/videos") != std::string::npos) {
        return "channel";
    }
    return "video";
}

MediaUrlMetaData YouTubeParser::parseUrl(const std::string& url) {
    MediaUrlMetaData meta;
    if (url.empty()) {
        meta.error_message = "URL cannot be empty";
        return meta;
    }

    meta.type = detectUrlType(url);
    std::cout << "[YTParser] Parsing " << meta.type << " URL: " << url << std::endl;

    std::string cmd = "yt-dlp --dump-single-json --flat-playlist --no-warnings \"" + url + "\" 2>&1";
    std::string jsonRaw = executeCommand(cmd);

    if (jsonRaw.empty() || jsonRaw.find("ERROR:") != std::string::npos) {
        meta.success = false;
        meta.error_message = jsonRaw.empty() ? "Failed to execute yt-dlp" : jsonRaw.substr(0, 300);
        return meta;
    }

    meta.success = true;

    // Extract Title
    meta.title = extractJsonField(jsonRaw, "title");
    if (meta.title.empty()) meta.title = extractJsonField(jsonRaw, "playlist_title");
    if (meta.title.empty()) meta.title = extractJsonField(jsonRaw, "channel");
    if (meta.title.empty()) meta.title = "YouTube Channel/Playlist";

    // Extract Channel/Uploader
    meta.channel_name = extractJsonField(jsonRaw, "uploader");
    if (meta.channel_name.empty()) meta.channel_name = extractJsonField(jsonRaw, "channel");
    if (meta.channel_name.empty()) meta.channel_name = meta.title;

    // Extract Thumbnail
    meta.thumbnail_url = extractJsonField(jsonRaw, "thumbnail");
    if (meta.thumbnail_url.empty()) meta.thumbnail_url = extractJsonField(jsonRaw, "url");
    meta.thumbnail_url.erase(std::remove(meta.thumbnail_url.begin(), meta.thumbnail_url.end(), '\\'), meta.thumbnail_url.end());

    // Parse entries if channel or playlist
    size_t entriesPos = jsonRaw.find("\"entries\": [");
    if (entriesPos != std::string::npos) {
        size_t currentPos = entriesPos;
        while ((currentPos = jsonRaw.find("\"_type\": \"url\"", currentPos)) != std::string::npos ||
               (currentPos = jsonRaw.find("\"_type\":\"url\"", currentPos)) != std::string::npos) {
            
            std::string itemUrl = extractJsonField(jsonRaw, "url", currentPos);
            std::string itemTitle = extractJsonField(jsonRaw, "title", currentPos);
            std::string itemThumb = extractJsonField(jsonRaw, "url", jsonRaw.find("\"thumbnails\"", currentPos));
            if (itemThumb.empty()) itemThumb = meta.thumbnail_url;
            itemThumb.erase(std::remove(itemThumb.begin(), itemThumb.end(), '\\'), itemThumb.end());

            if (!itemUrl.empty()) {
                VideoItemInfo item;
                item.url = itemUrl;
                item.title = itemTitle.empty() ? itemUrl : itemTitle;
                item.uploader = meta.channel_name;
                item.thumbnail = itemThumb;
                item.available_qualities = {"8k", "4k", "2k", "1080p", "720p", "480p", "360p"};
                meta.items.push_back(item);
            }
            currentPos += 15;
        }
    }

    if (meta.items.empty()) {
        // Single video fallback
        VideoItemInfo singleItem;
        singleItem.url = url;
        singleItem.title = meta.title;
        singleItem.uploader = meta.channel_name;
        singleItem.thumbnail = meta.thumbnail_url;
        singleItem.available_qualities = {"8k", "4k", "2k", "1080p", "720p", "480p", "360p"};
        meta.items.push_back(singleItem);
    }

    meta.total_items = static_cast<int>(meta.items.size());
    return meta;
}
