#ifndef YOUTUBE_PARSER_H
#define YOUTUBE_PARSER_H

#include <string>
#include <vector>

struct VideoItemInfo {
    std::string id;
    std::string title;
    std::string url;
    std::string thumbnail;
    std::string duration;
    std::string uploader;
    std::vector<std::string> available_qualities;
};

struct MediaUrlMetaData {
    std::string type;          // "video", "playlist", "channel", "unknown"
    std::string title;
    std::string channel_name;
    std::string avatar_url;
    std::string thumbnail_url;
    int total_items = 0;
    std::vector<VideoItemInfo> items;
    bool success = false;
    std::string error_message;
};

class YouTubeParser {
public:
    static MediaUrlMetaData parseUrl(const std::string& url);
    static std::string detectUrlType(const std::string& url);
    static std::string escapeJsonString(const std::string& input);

private:
    static std::string executeCommand(const std::string& command);
};

#endif // YOUTUBE_PARSER_H
